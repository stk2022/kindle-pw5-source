// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#include <clk.h>
#include <common.h>
#include <dm.h>
#include <generic-phy.h>
#include <linux/iopoll.h>
#include <power/regulator.h>

#include "mtu3.h"
#include "mtu3_dr.h"

void ssusb_set_force_mode(struct ssusb_mtk *ssusb,
			  enum mtu3_dr_force_mode mode)
{
	u32 value;

	value = mtu3_readl(ssusb->ippc_base, SSUSB_U2_CTRL(0));
	switch (mode) {
	case MTU3_DR_FORCE_DEVICE:
		value |= SSUSB_U2_PORT_FORCE_IDDIG | SSUSB_U2_PORT_RG_IDDIG;
		break;
	case MTU3_DR_FORCE_HOST:
		value |= SSUSB_U2_PORT_FORCE_IDDIG;
		value &= ~SSUSB_U2_PORT_RG_IDDIG;
		break;
	case MTU3_DR_FORCE_NONE:
		value &= ~(SSUSB_U2_PORT_FORCE_IDDIG | SSUSB_U2_PORT_RG_IDDIG);
		break;
	default:
		return;
	}
	mtu3_writel(ssusb->ippc_base, SSUSB_U2_CTRL(0), value);
}

/* u2-port0 should be powered on and enabled; */
int ssusb_check_clocks(struct ssusb_mtk *ssusb, u32 ex_clks)
{
	void __iomem *ibase = ssusb->ippc_base;
	u32 value, check_val;
	int ret;

	check_val = ex_clks | SSUSB_SYS125_RST_B_STS | SSUSB_SYSPLL_STABLE |
			SSUSB_REF_RST_B_STS;

	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS1, value,
				 (check_val == (value & check_val)), 20000);
	if (ret) {
		dev_err(ssusb->dev, "clks of sts1 are not stable!\n");
		return ret;
	}

	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS2, value,
				 (value & SSUSB_U2_MAC_SYS_RST_B_STS), 10000);
	if (ret) {
		dev_err(ssusb->dev, "mac2 clock is not stable\n");
		return ret;
	}

	return 0;
}

int ssusb_phy_setup(struct ssusb_mtk *ssusb)
{
	struct udevice *dev = ssusb->dev;
	struct phy *usb_phys;
	int i, ret, count;

	/* Return if no phy declared */
	if (!dev_read_prop(dev, "phys", NULL))
		return 0;

	count = dev_count_phandle_with_args(dev, "phys", "#phy-cells");
	if (count <= 0)
		return count;

	usb_phys = devm_kcalloc(dev, count, sizeof(*usb_phys),
				GFP_KERNEL);
	if (!usb_phys)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		ret = generic_phy_get_by_index(dev, i, &usb_phys[i]);
		if (ret && ret != -ENOENT) {
			dev_err(dev, "Failed to get USB PHY%d for %s\n",
				i, dev->name);
			return ret;
		}
	}

	for (i = 0; i < count; i++) {
		ret = generic_phy_init(&usb_phys[i]);
		if (ret) {
			dev_err(dev, "Can't init USB PHY%d for %s\n",
				i, dev->name);
			goto phys_init_err;
		}
	}

	for (i = 0; i < count; i++) {
		ret = generic_phy_power_on(&usb_phys[i]);
		if (ret) {
			dev_err(dev, "Can't power USB PHY%d for %s\n",
				i, dev->name);
			goto phys_poweron_err;
		}
	}

	ssusb->phys = usb_phys;
	ssusb->num_phys =  count;

	return 0;

phys_poweron_err:
	for (i = count - 1; i >= 0; i--)
		generic_phy_power_off(&usb_phys[i]);

	for (i = 0; i < count; i++)
		generic_phy_exit(&usb_phys[i]);
	devm_kfree(dev, usb_phys);
	return ret;

phys_init_err:
	for (; i >= 0; i--)
		generic_phy_exit(&usb_phys[i]);

	return ret;
}

void ssusb_phy_shutdown(struct ssusb_mtk *ssusb)
{
	struct udevice *dev = ssusb->dev;
	struct phy *usb_phys;
	int i, ret;

	usb_phys = ssusb->phys;
	for (i = 0; i < ssusb->num_phys; i++) {
		if (!generic_phy_valid(&usb_phys[i]))
			continue;

		ret = generic_phy_power_off(&usb_phys[i]);
		ret |= generic_phy_exit(&usb_phys[i]);
		if (ret) {
			dev_err(dev, "Can't shutdown USB PHY%d for %s\n",
				i, dev->name);
		}
	}
}

static int ssusb_clks_enable(struct ssusb_mtk *ssusb)
{
	int ret;

	ret = clk_enable(ssusb->sys_clk);
	if (ret) {
		dev_err(ssusb->dev, "failed to enable sys_clk\n");
		goto sys_clk_err;
	}

	ret = clk_enable(ssusb->ref_clk);
	if (ret) {
		dev_err(ssusb->dev, "failed to enable ref_clk\n");
		goto ref_clk_err;
	}

	ret = clk_enable(ssusb->mcu_clk);
	if (ret) {
		dev_err(ssusb->dev, "failed to enable mcu_clk\n");
		goto mcu_clk_err;
	}

	if (ssusb->dma_clk) {
		ret = clk_enable(ssusb->dma_clk);
		if (ret) {
			dev_err(ssusb->dev, "failed to enable dma_clk\n");
			goto dma_clk_err;
		}
	}

	return 0;

dma_clk_err:
	clk_disable(ssusb->mcu_clk);
mcu_clk_err:
	clk_disable(ssusb->ref_clk);
ref_clk_err:
	clk_disable(ssusb->sys_clk);
sys_clk_err:
	return ret;
}

static void ssusb_clks_disable(struct ssusb_mtk *ssusb)
{
	if (ssusb->dma_clk)
		clk_disable(ssusb->dma_clk);

	clk_disable(ssusb->mcu_clk);
	clk_disable(ssusb->ref_clk);
	clk_disable(ssusb->sys_clk);
}

static int ssusb_rscs_init(struct ssusb_mtk *ssusb)
{
	int ret = 0;

	ret = regulator_set_enable(ssusb->vusb33_supply, true);
	if (ret < 0 && ret != -ENOSYS) {
		dev_err(ssusb->dev, "failed to enable vusb33\n");
		goto vusb33_err;
	}

	ret = ssusb_clks_enable(ssusb);
	if (ret)
		goto clks_err;

	ret = ssusb_phy_setup(ssusb);
	if (ret) {
		dev_err(ssusb->dev, "failed to setup phy\n");
		goto phy_err;
	}

	return 0;

phy_err:
	ssusb_clks_disable(ssusb);
clks_err:
	regulator_set_enable(ssusb->vusb33_supply, false);
vusb33_err:
	return ret;
}

static void ssusb_rscs_exit(struct ssusb_mtk *ssusb)
{
	ssusb_clks_disable(ssusb);
	regulator_set_enable(ssusb->vusb33_supply, false);
	ssusb_phy_shutdown(ssusb);
}

static void ssusb_ip_sw_reset(struct ssusb_mtk *ssusb)
{
	/* reset whole ip (xhci & u3d) */
	mtu3_setbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
	udelay(1);
	mtu3_clrbits(ssusb->ippc_base, U3D_SSUSB_IP_PW_CTRL0, SSUSB_IP_SW_RST);
}

static int get_ssusb_rscs(struct udevice *dev, struct ssusb_mtk *ssusb)
{
	int ret;

	ret = device_get_supply_regulator(dev, "vusb33-supply",
					  &ssusb->vusb33_supply);
	if (ret) {
		dev_err(dev, "failed to get vusb33 %d\n", ret);
		//return ret;
	}

	ssusb->sys_clk = devm_clk_get(dev, "sys_ck");
	if (IS_ERR(ssusb->sys_clk)) {
		dev_err(dev, "failed to get sys clock\n");
		return PTR_ERR(ssusb->sys_clk);
	}

	ssusb->ref_clk = devm_clk_get_optional(dev, "ref_ck");
	if (IS_ERR(ssusb->ref_clk))
		return PTR_ERR(ssusb->ref_clk);

	ssusb->mcu_clk = devm_clk_get_optional(dev, "mcu_ck");
	if (IS_ERR(ssusb->mcu_clk))
		return PTR_ERR(ssusb->mcu_clk);

	ssusb->dma_clk = devm_clk_get_optional(dev, "dma_ck");
	if (IS_ERR(ssusb->dma_clk))
		return PTR_ERR(ssusb->dma_clk);

	ssusb->ippc_base = devfdt_remap_addr_name(dev, "ippc");
	if (!ssusb->ippc_base)
		return -ENODEV;

	ssusb->dr_mode = usb_get_dr_mode(dev_of_offset(dev));
	if (ssusb->dr_mode == USB_DR_MODE_UNKNOWN)
		ssusb->dr_mode = USB_DR_MODE_OTG;

	dev_info(dev, "dr_mode: %d\n", ssusb->dr_mode);

	return 0;
}

static int mtu3_probe(struct udevice *dev)
{
	struct ssusb_mtk *ssusb = dev_get_priv(dev);
	int ret = -ENOMEM;

	ssusb->dev = dev;

	ret = get_ssusb_rscs(dev, ssusb);
	if (ret)
		return ret;

	ret = ssusb_rscs_init(ssusb);
	if (ret)
		return ret;

	ssusb_ip_sw_reset(ssusb);

	if (IS_ENABLED(CONFIG_USB_MTU3_HOST))
		ssusb->dr_mode = USB_DR_MODE_HOST;
	else if (IS_ENABLED(CONFIG_USB_MTU3_GADGET))
		ssusb->dr_mode = USB_DR_MODE_PERIPHERAL;

	/* default as host */
	ssusb->is_host = !(ssusb->dr_mode == USB_DR_MODE_PERIPHERAL);

	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		ret = ssusb_gadget_init(ssusb);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			goto comm_exit;
		}
		break;
	case USB_DR_MODE_HOST:
		ret = ssusb_host_init(ssusb, ofnode_to_np(dev->node));
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			goto comm_exit;
		}
		break;
	default:
		dev_err(dev, "unsupported mode: %d\n", ssusb->dr_mode);
		ret = -EINVAL;
		goto comm_exit;
	}

	return 0;

comm_exit:
	ssusb_rscs_exit(ssusb);
	return ret;
}

static int mtu3_remove(struct udevice *dev)
{
	struct ssusb_mtk *ssusb = dev_to_ssusb(dev);

	switch (ssusb->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
	case USB_DR_MODE_OTG:
		ssusb_gadget_exit(ssusb);
		break;
	case USB_DR_MODE_HOST:
		ssusb_host_exit(ssusb);
		break;
	default:
		return -EINVAL;
	}

	ssusb_rscs_exit(ssusb);
	return 0;
}

static const struct udevice_id mtu3_of_match[] = {
	{.compatible = "mediatek,mtu3",},
	{},
};

U_BOOT_DRIVER(mtu3) = {
	.name		= "mtu3",
	.id		= UCLASS_USB_GADGET_GENERIC,
	.of_match	= mtu3_of_match,
	.probe		= mtu3_probe,
	.remove		= mtu3_remove,
	.priv_auto_alloc_size = sizeof(struct ssusb_mtk),
};

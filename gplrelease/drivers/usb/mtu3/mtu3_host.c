// SPDX-License-Identifier: GPL-2.0
/*
 * mtu3_dr.c - dual role switch and host glue layer
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#include <dm/lists.h>
#include <linux/iopoll.h>

#include "mtu3.h"
#include "mtu3_dr.h"

static void host_ports_num_get(struct ssusb_mtk *ssusb)
{
	u32 xhci_cap;

	xhci_cap = mtu3_readl(ssusb->ippc_base, U3D_SSUSB_IP_XHCI_CAP);
	ssusb->u2_ports = SSUSB_IP_XHCI_U2_PORT_NUM(xhci_cap);
	ssusb->u3_ports = SSUSB_IP_XHCI_U3_PORT_NUM(xhci_cap);

	dev_dbg(ssusb->dev, "host - u2_ports:%d, u3_ports:%d\n",
		ssusb->u2_ports, ssusb->u3_ports);
}

/* only configure ports will be used later */
int ssusb_host_enable(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	int u3_ports_disabed;
	u32 check_clk;
	u32 value;
	int i;

	/* power on host ip */
	mtu3_clrbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	/* power on and enable u3 ports except skipped ones */
	u3_ports_disabed = 0;
	for (i = 0; i < num_u3p; i++) {
		if ((0x1 << i) & ssusb->u3p_dis_msk) {
			u3_ports_disabed++;
			continue;
		}

		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		value &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		value |= SSUSB_U3_PORT_HOST_SEL;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	/* power on and enable all u2 ports */
	for (i = 0; i < num_u2p; i++) {
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value &= ~(SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
		value |= SSUSB_U2_PORT_HOST_SEL;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);
	}

	check_clk = SSUSB_XHCI_RST_B_STS;
	if (num_u3p > u3_ports_disabed)
		check_clk = SSUSB_U3_MAC_RST_B_STS;

	return ssusb_check_clocks(ssusb, check_clk);
}

int ssusb_host_disable(struct ssusb_mtk *ssusb, bool suspend)
{
	void __iomem *ibase = ssusb->ippc_base;
	int num_u3p = ssusb->u3_ports;
	int num_u2p = ssusb->u2_ports;
	u32 value;
	int ret;
	int i;

	/* power down and disable u3 ports except skipped ones */
	for (i = 0; i < num_u3p; i++) {
		if ((0x1 << i) & ssusb->u3p_dis_msk)
			continue;

		value = mtu3_readl(ibase, SSUSB_U3_CTRL(i));
		value |= SSUSB_U3_PORT_PDN;
		value |= suspend ? 0 : SSUSB_U3_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U3_CTRL(i), value);
	}

	/* power down and disable all u2 ports */
	for (i = 0; i < num_u2p; i++) {
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(i));
		value |= SSUSB_U2_PORT_PDN;
		value |= suspend ? 0 : SSUSB_U2_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U2_CTRL(i), value);
	}

	/* power down host ip */
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	if (!suspend)
		return 0;

	/* wait for host ip to sleep */
	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS1, value,
				 (value & SSUSB_IP_SLEEP_STS), 100000);
	if (ret)
		dev_err(ssusb->dev, "ip sleep failed!!!\n");

	return ret;
}

static void ssusb_host_setup(struct ssusb_mtk *ssusb)
{
	host_ports_num_get(ssusb);

	/*
	 * power on host and power on/enable all ports
	 * if support OTG, gadget driver will switch port0 to device mode
	 */
	ssusb_host_enable(ssusb);
	ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);
}

static void ssusb_host_cleanup(struct ssusb_mtk *ssusb)
{
	ssusb_host_disable(ssusb, false);
}

/*
 * If host supports multiple ports, the VBUSes(5V) of ports except port0
 * which supports OTG are better to be enabled by default in DTS.
 * Because the host driver will keep link with devices attached when system
 * enters suspend mode, so no need to control VBUSes after initialization.
 */
int ssusb_host_init(struct ssusb_mtk *ssusb,
		    const struct device_node *parent_dn)
{
	struct udevice *parent_dev = ssusb->dev;
	struct udevice *dev;
	ofnode node;
	int ret;

	node = dev_read_first_subnode(parent_dev);
	if (!ofnode_valid(node)) {
		dev_dbg(parent_dev, "invalid subnode\n");
		return -ENODEV;
	}

	ssusb_host_setup(ssusb);

	ret = device_bind_driver_to_node(parent_dev, "xhci-mtk",
					 ofnode_get_name(node),
					 node, &dev);
	if (ret) {
		dev_dbg(parent_dev, "failed to create child devices at %pOF\n",
			parent_dn);
		return ret;
	}

	dev_info(parent_dev, "xHCI bind success...\n");

	return 0;
}

void ssusb_host_exit(struct ssusb_mtk *ssusb)
{
	ssusb_host_cleanup(ssusb);
}

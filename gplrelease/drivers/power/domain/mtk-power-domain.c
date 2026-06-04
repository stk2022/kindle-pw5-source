// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Ryder Lee <ryder.lee@mediatek.com>
 */

#include <clk.h>
#include <common.h>
#include <dm.h>
#include <power-domain-uclass.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <linux/iopoll.h>
#include <scpsys-ext.h>

#include <dt-bindings/power/mt7623-power.h>
#include <dt-bindings/power/mt7629-power.h>
#include <dt-bindings/power/mt8512-power.h>

#define SPM_EN			(0xb16 << 16 | 0x1)
#define SPM_VDE_PWR_CON		0x0210
#define SPM_MFG_PWR_CON		0x0214
#define SPM_ISP_PWR_CON		0x0238
#define SPM_DIS_PWR_CON		0x023c
#define SPM_CONN_PWR_CON	0x0280
#define SPM_BDP_PWR_CON		0x029c
#define SPM_ETH_PWR_CON		0x02a0
#define SPM_HIF_PWR_CON		0x02a4
#define SPM_IFR_MSC_PWR_CON	0x02a8
#define SPM_ETHSYS_PWR_CON	0x2e0
#define SPM_HIF0_PWR_CON	0x2e4
#define SPM_HIF1_PWR_CON	0x2e8
#define SPM_PWR_STATUS		0x60c
#define SPM_PWR_STATUS_2ND	0x610

#define PWR_RST_B_BIT		BIT(0)
#define PWR_ISO_BIT		BIT(1)
#define PWR_ON_BIT		BIT(2)
#define PWR_ON_2ND_BIT		BIT(3)
#define PWR_CLK_DIS_BIT		BIT(4)

#define PWR_STATUS_CONN		BIT(1)
#define PWR_STATUS_DISP		BIT(3)
#define PWR_STATUS_MFG		BIT(4)
#define PWR_STATUS_ISP		BIT(5)
#define PWR_STATUS_VDEC		BIT(7)
#define PWR_STATUS_BDP		BIT(14)
#define PWR_STATUS_ETH		BIT(15)
#define PWR_STATUS_HIF		BIT(16)
#define PWR_STATUS_IFR_MSC	BIT(17)
#define PWR_STATUS_ETHSYS	BIT(24)
#define PWR_STATUS_HIF0		BIT(25)
#define PWR_STATUS_HIF1		BIT(26)

/* Infrasys configuration */
#define INFRA_TOPDCM_CTRL	0x10
#define INFRA_TOPAXI_PROT_EN	0x220
#define INFRA_TOPAXI_PROT_STA1	0x228

#define DCM_TOP_EN		BIT(0)

#define MAX_CLKS	3
#define MAX_SUBSYS_CLKS 10

enum scp_domain_type {
	SCPSYS_MT7623,
	SCPSYS_MT7629,
	SCPSYS_MT8512,
};

struct scp_domain;

struct scp_domain_data {
	struct scp_domain *scpd;
	u32 sta_mask;
	int ctl_offs;
	u32 sram_pdn_bits;
	u32 sram_pdn_ack_bits;
	u32 bus_prot_mask;
	struct bus_prot bp_table[MAX_STEPS];
	const char *basic_clk_name[MAX_CLKS];
	const char *subsys_clk_name[MAX_SUBSYS_CLKS];
	struct clk clk[MAX_CLKS];
	struct clk subsys_clk[MAX_SUBSYS_CLKS];
};

struct scp_ctrl_reg {
	int pwr_sta_offs;
	int pwr_sta2nd_offs;
};

struct scp_domain {
	void __iomem *base;
	void __iomem *infracfg;
	enum scp_domain_type type;
	struct scp_domain_data *data;
	int num_domains;
	struct scp_ctrl_reg ctrl_reg;
};

static struct scp_domain_data scp_domain_mt7623[] = {
	[MT7623_POWER_DOMAIN_CONN] = {
		.sta_mask = PWR_STATUS_CONN,
		.ctl_offs = SPM_CONN_PWR_CON,
		.bus_prot_mask = BIT(8) | BIT(2),
	},
	[MT7623_POWER_DOMAIN_DISP] = {
		.sta_mask = PWR_STATUS_DISP,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.bus_prot_mask = BIT(2),
	},
	[MT7623_POWER_DOMAIN_MFG] = {
		.sta_mask = PWR_STATUS_MFG,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT7623_POWER_DOMAIN_VDEC] = {
		.sta_mask = PWR_STATUS_VDEC,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT7623_POWER_DOMAIN_ISP] = {
		.sta_mask = PWR_STATUS_ISP,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
	},
	[MT7623_POWER_DOMAIN_BDP] = {
		.sta_mask = PWR_STATUS_BDP,
		.ctl_offs = SPM_BDP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
	},
	[MT7623_POWER_DOMAIN_ETH] = {
		.sta_mask = PWR_STATUS_ETH,
		.ctl_offs = SPM_ETH_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
	[MT7623_POWER_DOMAIN_HIF] = {
		.sta_mask = PWR_STATUS_HIF,
		.ctl_offs = SPM_HIF_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
	[MT7623_POWER_DOMAIN_IFR_MSC] = {
		.sta_mask = PWR_STATUS_IFR_MSC,
		.ctl_offs = SPM_IFR_MSC_PWR_CON,
	},
};

static struct scp_domain_data scp_domain_mt7629[] = {
	[MT7629_POWER_DOMAIN_ETHSYS] = {
		.sta_mask = PWR_STATUS_ETHSYS,
		.ctl_offs = SPM_ETHSYS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = (BIT(3) | BIT(17)),
	},
	[MT7629_POWER_DOMAIN_HIF0] = {
		.sta_mask = PWR_STATUS_HIF0,
		.ctl_offs = SPM_HIF0_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = GENMASK(25, 24),
	},
	[MT7629_POWER_DOMAIN_HIF1] = {
		.sta_mask = PWR_STATUS_HIF1,
		.ctl_offs = SPM_HIF1_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bus_prot_mask = GENMASK(28, 26),
	},
};

static struct scp_domain_data scp_domain_mt8512[] = {
	[MT8512_POWER_DOMAIN_MM] = {
		.sta_mask = BIT(15),
		.ctl_offs = 0x374,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
		.subsys_clk_name = {"mm-0", "mm-1"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a8, 0x2ac, 0, 0x258,
				BIT(16) | BIT(17), BIT(16) | BIT(17)),
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(8) | BIT(9) | BIT(10) | BIT(11),
				BIT(8) | BIT(9) | BIT(10) | BIT(11)),
		},
	},
	[MT8512_POWER_DOMAIN_IMG] = {
		.sta_mask = BIT(16),
		.ctl_offs = 0x38C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
	},
	[MT8512_POWER_DOMAIN_IP0] = {
		.sta_mask = BIT(25),
		.ctl_offs = 0x39C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipsys"},
		.subsys_clk_name = {"ip0-0", "ip0-1", "ip0-2", "ip0-3"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(12), BIT(12)),
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(5) | BIT(6) | BIT(7),
				BIT(5) | BIT(6) | BIT(7)),
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(14) | BIT(16) | BIT(18),
				BIT(14) | BIT(16) | BIT(18)),
		},
	},
	[MT8512_POWER_DOMAIN_IP1] = {
		.sta_mask = BIT(26),
		.ctl_offs = 0x384,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipsys"},
		.subsys_clk_name = {"ip1-0", "ip1-1"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(4), BIT(4)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(2), BIT(2)),
		},
	},
	[MT8512_POWER_DOMAIN_IP2] = {
		.sta_mask = BIT(27),
		.ctl_offs = 0x388,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipsys"},
		.subsys_clk_name = {"ip2-0", "ip2-1"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(5), BIT(5)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(3), BIT(3)),
		},
	},
	[MT8512_POWER_DOMAIN_CONN] = {
		.sta_mask = BIT(1),
		.ctl_offs = 0x32C,
		.sram_pdn_bits = 0,
		.sram_pdn_ack_bits = 0,
		.basic_clk_name = {"conn", "conn1"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(13), BIT(13)),
			BUS_PROT(IFR_TYPE, 0x2a8, 0x2ac, 0, 0x258,
				BIT(18), BIT(18)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(14), BIT(14)),
			BUS_PROT(IFR_TYPE, 0x2a8, 0x2ac, 0, 0x258,
				BIT(21), BIT(21)),
		},
	},
	[MT8512_POWER_DOMAIN_DSP] = {
		.sta_mask = BIT(17),
		.ctl_offs = 0x37C,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.basic_clk_name = {"dsp", "dsp1"},
		.subsys_clk_name = {"dsp-0", "dsp-1"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(7) | BIT(10) | BIT(11),
				BIT(7) | BIT(10) | BIT(11)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(1) | BIT(24), BIT(1) | BIT(24)),
		},
	},
	[MT8512_POWER_DOMAIN_AUDAFE] = {
		.sta_mask = BIT(24),
		.ctl_offs = 0x314,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(16, 13),
		.basic_clk_name = {"audafe"},
		.subsys_clk_name = {"audafe-0"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(28), BIT(28)),
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(22), BIT(22)),
		},
	},
	[MT8512_POWER_DOMAIN_AUDSRC] = {
		.sta_mask = BIT(23),
		.ctl_offs = 0x328,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audsrc"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x2a0, 0x2a4, 0, 0x228,
				BIT(21), BIT(21)),
		},
	},
	[MT8512_POWER_DOMAIN_USB] = {
		.sta_mask = BIT(20),
		.ctl_offs = 0x3A4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"usb"},
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x42c, 0x430, 0, 0x428,
				BIT(0) | BIT(1), BIT(0) | BIT(1)),
		},
	},
};

/**
 * This function enables the bus protection bits for disabled power
 * domains so that the system does not hang when some unit accesses the
 * bus while in power down.
 */
static int mtk_infracfg_set_bus_protection(void __iomem *infracfg,
					   u32 mask)
{
	u32 val;

	clrsetbits_le32(infracfg + INFRA_TOPAXI_PROT_EN, mask, mask);

	return readl_poll_timeout(infracfg + INFRA_TOPAXI_PROT_STA1, val,
				  (val & mask) == mask, 100);
}

static int mtk_infracfg_clear_bus_protection(void __iomem *infracfg,
					     u32 mask)
{
	u32 val;

	clrbits_le32(infracfg + INFRA_TOPAXI_PROT_EN, mask);

	return readl_poll_timeout(infracfg + INFRA_TOPAXI_PROT_STA1, val,
				  !(val & mask), 100);
}

static int scpsys_clk_enable(struct clk *clk, int max_num)
{
	int i, ret = 0;

	for (i = 0; i < max_num && clk[i].dev; i++) {
		ret = clk_enable(&clk[i]);
		if (ret) {
			for (--i; i >= 0; i--)
				clk_disable(&clk[i]);

			break;
		}
	}

	return ret;
}

static int scpsys_clk_disable(struct clk *clk, int max_num)
{
	int i;

	for (i = max_num - 1; i >= 0; i--) {
		if (clk[i].dev)
			clk_disable(&clk[i]);
	}

	return 0;
}

static int scpsys_domain_is_on(struct scp_domain_data *data)
{
	struct scp_domain *scpd = data->scpd;

	u32 sta = readl(scpd->base + scpd->ctrl_reg.pwr_sta_offs) &
			data->sta_mask;
	u32 sta2 = readl(scpd->base + scpd->ctrl_reg.pwr_sta2nd_offs) &
			 data->sta_mask;

	/*
	 * A domain is on when both status bits are set. If only one is set
	 * return an error. This happens while powering up a domain
	 */
	if (sta && sta2)
		return true;
	if (!sta && !sta2)
		return false;

	return -EINVAL;
}

static int scpsys_power_on(struct power_domain *power_domain)
{
	struct scp_domain *scpd = dev_get_priv(power_domain->dev);
	struct scp_domain_data *data = &scpd->data[power_domain->id];
	void __iomem *ctl_addr = scpd->base + data->ctl_offs;
	u32 pdn_ack = data->sram_pdn_ack_bits;
	u32 val;
	int ret, tmp;

	writel(SPM_EN, scpd->base);

	if (data->clk[0].dev) {
		ret = scpsys_clk_enable(data->clk, MAX_CLKS);
		if (ret)
			return ret;
	}

	val = readl(ctl_addr);
	val |= PWR_ON_BIT;
	writel(val, ctl_addr);

	val |= PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	ret = readx_poll_timeout(scpsys_domain_is_on, data, tmp, tmp > 0,
				 100);
	if (ret < 0)
		return ret;

	val &= ~PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ISO_BIT;
	writel(val, ctl_addr);

	val |= PWR_RST_B_BIT;
	writel(val, ctl_addr);

	val &= ~data->sram_pdn_bits;
	writel(val, ctl_addr);

	ret = readl_poll_timeout(ctl_addr, tmp, !(tmp & pdn_ack), 100);
	if (ret < 0)
		return ret;

	if (data->subsys_clk[0].dev) {
		ret = scpsys_clk_enable(data->subsys_clk, MAX_SUBSYS_CLKS);
		if (ret)
			return ret;
	}

	if (data->bus_prot_mask) {
		ret = mtk_infracfg_clear_bus_protection(scpd->infracfg,
							data->bus_prot_mask);
		if (ret)
			return ret;
	} else if (data->bp_table[0].mask) {
		ret = mtk_scpsys_ext_clear_bus_protection(data->bp_table,
			scpd->infracfg);
		if (ret)
			return ret;
	}

	return 0;
}

static int scpsys_power_off(struct power_domain *power_domain)
{
	struct scp_domain *scpd = dev_get_priv(power_domain->dev);
	struct scp_domain_data *data = &scpd->data[power_domain->id];
	void __iomem *ctl_addr = scpd->base + data->ctl_offs;
	u32 pdn_ack = data->sram_pdn_ack_bits;
	u32 val;
	int ret, tmp;

	if (data->bus_prot_mask) {
		ret = mtk_infracfg_set_bus_protection(scpd->infracfg,
						      data->bus_prot_mask);
		if (ret)
			return ret;
	} else if (data->bp_table[0].mask) {
		ret = mtk_scpsys_ext_set_bus_protection(data->bp_table,
			scpd->infracfg);
		if (ret)
			return ret;
	}

	if (data->subsys_clk[0].dev) {
		ret = scpsys_clk_disable(data->subsys_clk, MAX_SUBSYS_CLKS);
		if (ret)
			return ret;
	}

	val = readl(ctl_addr);
	val |= data->sram_pdn_bits;
	writel(val, ctl_addr);

	ret = readl_poll_timeout(ctl_addr, tmp, (tmp & pdn_ack) == pdn_ack,
				 100);
	if (ret < 0)
		return ret;

	val |= PWR_ISO_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_RST_B_BIT;
	writel(val, ctl_addr);

	val |= PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	ret = readx_poll_timeout(scpsys_domain_is_on, data, tmp, !tmp, 100);
	if (ret < 0)
		return ret;

	if (data->clk[0].dev) {
		ret = scpsys_clk_disable(data->clk, MAX_CLKS);
		if (ret)
			return ret;
	}

	return 0;
}

static int scpsys_power_request(struct power_domain *power_domain)
{
	struct scp_domain *scpd = dev_get_priv(power_domain->dev);
	struct scp_domain_data *data;

	data = &scpd->data[power_domain->id];
	data->scpd = scpd;

	return 0;
}

static int scpsys_power_free(struct power_domain *power_domain)
{
	return 0;
}

static int mtk_power_domain_hook(struct udevice *dev)
{
	struct scp_domain *scpd = dev_get_priv(dev);

	scpd->type = (enum scp_domain_type)dev_get_driver_data(dev);

	switch (scpd->type) {
	case SCPSYS_MT7623:
		scpd->data = scp_domain_mt7623;
		scpd->num_domains = ARRAY_SIZE(scp_domain_mt7623);
		scpd->ctrl_reg.pwr_sta_offs = SPM_PWR_STATUS;
		scpd->ctrl_reg.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND;
		break;
	case SCPSYS_MT7629:
		scpd->data = scp_domain_mt7629;
		scpd->num_domains = ARRAY_SIZE(scp_domain_mt7629);
		scpd->ctrl_reg.pwr_sta_offs = SPM_PWR_STATUS;
		scpd->ctrl_reg.pwr_sta2nd_offs = SPM_PWR_STATUS_2ND;
		break;
	case SCPSYS_MT8512:
		scpd->data = scp_domain_mt8512;
		scpd->num_domains = ARRAY_SIZE(scp_domain_mt8512);
		scpd->ctrl_reg.pwr_sta_offs = 0x0180;
		scpd->ctrl_reg.pwr_sta2nd_offs = 0x0184;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int init_power_domain_clks(struct udevice *dev)
{
	struct scp_domain *scpd = dev_get_priv(dev);
	int i, j, ret;

	for (i = 0; i < scpd->num_domains; i++) {
		struct scp_domain_data *data = &scpd->data[i];

		if (data->basic_clk_name[0]) {
			for (j = 0; j < MAX_CLKS &&
					data->basic_clk_name[j]; j++) {
				ret = clk_get_by_name(dev, data->basic_clk_name[j],
						&data->clk[j]);
				if (ret < 0) {
					pr_err("Unable to get clk(%d)\n",
						data->basic_clk_name[j]);
					return ret;
				}
			}
		}

		if (data->subsys_clk_name[0]) {
			for (j = 0; j < MAX_SUBSYS_CLKS &&
					data->subsys_clk_name[j]; j++) {
				ret = clk_get_by_name(dev, data->subsys_clk_name[j],
						&data->subsys_clk[j]);
				if (ret < 0) {
					pr_err("Unable to get clk(%d)\n",
						data->subsys_clk_name[j]);
					return ret;
				}
			}
		}
	}
}

static int mtk_power_domain_probe(struct udevice *dev)
{
	struct ofnode_phandle_args args;
	struct scp_domain *scpd = dev_get_priv(dev);
	struct regmap *regmap;
	struct clk_bulk bulk;
	int err;

	scpd->base = dev_read_addr_ptr(dev);
	if (!scpd->base)
		return -ENOENT;

	err = mtk_power_domain_hook(dev);
	if (err)
		return err;

	/* get corresponding syscon phandle */
	err = dev_read_phandle_with_args(dev, "infracfg", NULL, 0, 0, &args);
	if (err)
		return err;

	regmap = syscon_node_to_regmap(args.node);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	scpd->infracfg = regmap_get_range(regmap, 0);
	if (!scpd->infracfg)
		return -ENOENT;

	/* enable Infra DCM */
	setbits_le32(scpd->infracfg + INFRA_TOPDCM_CTRL, DCM_TOP_EN);

	if (scpd->type == SCPSYS_MT8512) {
		err = init_power_domain_clks(dev);
	} else {
		err = clk_get_bulk(dev, &bulk);
		if (err)
			return err;

		err = clk_enable_bulk(&bulk);
	}

	return err;
}

static const struct udevice_id mtk_power_domain_ids[] = {
	{
		.compatible = "mediatek,mt7623-scpsys",
		.data = SCPSYS_MT7623,
	},
	{
		.compatible = "mediatek,mt7629-scpsys",
		.data = SCPSYS_MT7629,
	},
	{
		.compatible = "mediatek,mt8512-scpsys",
		.data = SCPSYS_MT8512,
	},
	{ /* sentinel */ }
};

struct power_domain_ops mtk_power_domain_ops = {
	.free = scpsys_power_free,
	.off = scpsys_power_off,
	.on = scpsys_power_on,
	.request = scpsys_power_request,
};

U_BOOT_DRIVER(mtk_power_domain) = {
	.name = "mtk_power_domain",
	.id = UCLASS_POWER_DOMAIN,
	.ops = &mtk_power_domain_ops,
	.probe = mtk_power_domain_probe,
	.of_match = mtk_power_domain_ids,
	.priv_auto_alloc_size = sizeof(struct scp_domain),
};

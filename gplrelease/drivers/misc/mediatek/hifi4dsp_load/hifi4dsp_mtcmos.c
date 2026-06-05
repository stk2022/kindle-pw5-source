/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/freezer.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/spi/spi.h>
#include <mach/mtk_hifi4dsp_api.h>

#define MT8570_POWER_DOMAIN_DSP_SRAM0	1
#define MT8570_POWER_DOMAIN_DSP_SRAM1	2
#define MT8570_POWER_DOMAIN_DSP_SRAM2	3
#define MT8570_POWER_DOMAIN_DSP_SRAM3	4
#define MT8570_POWER_DOMAIN_DSP_SRAM4	5
#define MT8570_POWER_DOMAIN_DSP_SRAM5	6
#define MT8570_POWER_DOMAIN_DSP_SRAM6	7
#define MT8570_POWER_DOMAIN_DSP_SRAM7	8
#define MT8570_POWER_DOMAIN_NR		9

#define ADSP_REG_BASE			(0x1D000000)
#define SCPSYS_BASE			(ADSP_REG_BASE + 0xB000)
#define AUDIOSYS_SEMAPHORE_BASE		(ADSP_REG_BASE + 0x61000)
#define SPM_PWR_STATUS			(SCPSYS_BASE + 0x80)
#define SPM_PWR_STATUS_2ND		(SCPSYS_BASE + 0x84)

#define PWR_RST_B_BIT		BIT(0)
#define PWR_ISO_BIT		BIT(1)
#define PWR_ON_BIT		BIT(2)
#define PWR_ON_2ND_BIT		BIT(3)
#define PWR_CLK_DIS_BIT		BIT(4)

#define MAX_STEPS	3

#define BUS_PROT(_base, _en_ofs, _mask,			\
		_set_ofs,  _clr_ofs, _sta_ofs) {	\
		.base = _base,			\
		.en_ofs = _en_ofs,		\
		.mask = _mask,			\
		.set_ofs = _set_ofs,		\
		.clr_ofs = _clr_ofs,		\
		.sta_ofs = _sta_ofs,		\
	}

#define BUS_WAY(_base, _en_ofs, _mask) {	\
		.base = _base,			\
		.en_ofs = _en_ofs,		\
		.mask = _mask,			\
	}

struct bus_prot {
	u32 base;
	u32 en_ofs;
	u32 mask;
	u32 set_ofs;
	u32 clr_ofs;
	u32 sta_ofs;
};

struct bus_way {
	u32 base;
	u32 en_ofs;
	u32 mask;
};

struct scp_domain_data {
	const char *name;
	u32 sta_mask;
	u32 ctl_ofs;
	u32 sram_pdn_ofs;
	u32 sram_pdn_bits;
	u32 sram_pdn_ack_bits;
	struct bus_prot prot_table[MAX_STEPS];	/* bus protect table */
	struct bus_way way_table[MAX_STEPS];	/* bus way_en table */
};

/*
 * MT8570 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt8570[] = {
	[MT8570_POWER_DOMAIN_DSP_SRAM0] = {
		.name = "dsp_sram0",
		.sta_mask = BIT(2),
		.ctl_ofs = 0x10,
		.sram_pdn_ofs = 0x14,
		.sram_pdn_bits = GENMASK(0, 0),
		.sram_pdn_ack_bits = GENMASK(8, 8),
		.prot_table = {
			[0] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x048, BIT(4) | BIT(14) | BIT(26),
				0x04c, 0x050, 0x040),
		},
		.way_table = {
			[0] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x000, BIT(10)),
			[1] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x02c, BIT(8)),
			[2] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x030, BIT(2)),
		},
	},
	[MT8570_POWER_DOMAIN_DSP_SRAM1] = {
		.name = "dsp_sram1",
		.sta_mask = BIT(3),
		.ctl_ofs = 0x1C,
		.sram_pdn_ofs = 0x20,
		.sram_pdn_bits = GENMASK(0, 0),
		.sram_pdn_ack_bits = GENMASK(8, 8),
		.prot_table = {
			[0] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x048, BIT(5) | BIT(15) | BIT(27),
				0x04c, 0x050, 0x040),
		},
		.way_table = {
			[0] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x000, BIT(11)),
			[1] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x02c, BIT(9)),
			[2] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x030, BIT(3)),
		},
	},
	[MT8570_POWER_DOMAIN_DSP_SRAM2] = {
		.name = "dsp_sram2",
		.sta_mask = BIT(4),
		.ctl_ofs = 0x28,
		.sram_pdn_ofs = 0x2C,
		.sram_pdn_bits = GENMASK(0, 0),
		.sram_pdn_ack_bits = GENMASK(8, 8),
		.prot_table = {
			[0] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x048, BIT(6) | BIT(16) | BIT(28),
				0x04c, 0x050, 0x040),
		},
		.way_table = {
			[0] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x000, BIT(12)),
			[1] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x02c, BIT(10)),
			[2] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x030, BIT(4)),
		},
	},
	[MT8570_POWER_DOMAIN_DSP_SRAM3] = {
		.name = "dsp_sram3",
		.sta_mask = BIT(5),
		.ctl_ofs = 0x34,
		.sram_pdn_ofs = 0x38,
		.sram_pdn_bits = GENMASK(0, 0),
		.sram_pdn_ack_bits = GENMASK(8, 8),
		.prot_table = {
			[0] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x048, BIT(7) | BIT(17) | BIT(29),
				0x04c, 0x050, 0x040),
		},
		.way_table = {
			[0] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x000, BIT(13)),
			[1] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x02c, BIT(11)),
			[2] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x030, BIT(5)),
		},
	},
	[MT8570_POWER_DOMAIN_DSP_SRAM4] = {
		.name = "dsp_sram4",
		.sta_mask = BIT(6),
		.ctl_ofs = 0x40,
		.sram_pdn_ofs = 0x44,
		.sram_pdn_bits = GENMASK(0, 0),
		.sram_pdn_ack_bits = GENMASK(8, 8),
		.prot_table = {
			[0] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x048, BIT(8) | BIT(18) | BIT(30),
				0x04c, 0x050, 0x040),
		},
		.way_table = {
			[0] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x004, BIT(17)),
			[1] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x02c, BIT(18)),
			[2] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x030, BIT(12)),
		},
	},
	[MT8570_POWER_DOMAIN_DSP_SRAM5] = {
		.name = "dsp_sram5",
		.sta_mask = BIT(7),
		.ctl_ofs = 0x4C,
		.sram_pdn_ofs = 0x50,
		.sram_pdn_bits = GENMASK(0, 0),
		.sram_pdn_ack_bits = GENMASK(8, 8),
		.prot_table = {
			[0] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x048, BIT(9) | BIT(19) | BIT(31),
				0x04c, 0x050, 0x040),
		},
		.way_table = {
			[0] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x004, BIT(18)),
			[1] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x02c, BIT(19)),
			[2] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x030, BIT(13)),
		},
	},
	[MT8570_POWER_DOMAIN_DSP_SRAM6] = {
		.name = "dsp_sram6",
		.sta_mask = BIT(8),
		.ctl_ofs = 0x58,
		.sram_pdn_ofs = 0x5C,
		.sram_pdn_bits = GENMASK(0, 0),
		.sram_pdn_ack_bits = GENMASK(8, 8),
		.prot_table = {
			[0] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x048, BIT(10) | BIT(20),
				0x04c, 0x050, 0x040),
			[1] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x05c, BIT(0), 0x060, 0x064, 0x054),
		},
		.way_table = {
			[0] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x004, BIT(19)),
			[1] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x02c, BIT(20)),
			[2] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x030, BIT(14)),
		},
	},
	[MT8570_POWER_DOMAIN_DSP_SRAM7] = {
		.name = "dsp_sram7",
		.sta_mask = BIT(9),
		.ctl_ofs = 0x64,
		.sram_pdn_ofs = 0x68,
		.sram_pdn_bits = GENMASK(0, 0),
		.sram_pdn_ack_bits = GENMASK(8, 8),
		.prot_table = {
			[0] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x048, BIT(11) | BIT(21),
				0x04c, 0x050, 0x040),
			[1] = BUS_PROT(AUDIOSYS_SEMAPHORE_BASE,
				0x05c, BIT(1), 0x060, 0x064, 0x054),
		},
		.way_table = {
			[0] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x004, BIT(20)),
			[1] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x02c, BIT(21)),
			[2] = BUS_WAY(AUDIOSYS_SEMAPHORE_BASE, 0x030, BIT(15)),
		},
	},
};

static int scpsys_clear_bus_prot(const struct bus_prot *prot_table)
{
	int i, time = 0;
	u32 reg_clr, mask, reg_sta, val;

	for (i = 0; i < MAX_STEPS && prot_table[i].mask; i++) {
		reg_clr = prot_table[i].base + prot_table[i].clr_ofs;
		mask = prot_table[i].mask;
		reg_sta = prot_table[i].base + prot_table[i].sta_ofs;

		spi_write_register(reg_clr, mask, SPI_SPEED_HIGH);

		while (1) {
			spi_read_register(reg_sta, &val, SPI_SPEED_HIGH);

			if (!(val & mask))
				break;

			udelay(10);
			time++;
			if (time > 100) {
				pr_err("%s: reg_sta val = 0x%x\n",
						__func__, val);
				return -ETIMEDOUT;
			}
		}
	}

	return 0;
}

static void scpsys_set_way_en(const struct bus_way *way_table, bool en)
{
	int i;
	u32 reg_way, mask, val;

	for (i = 0; i < MAX_STEPS && way_table[i].mask; i++) {
		reg_way = way_table[i].base + way_table[i].en_ofs;
		mask = way_table[i].mask;

		if (en) {
			spi_read_register(reg_way, &val, SPI_SPEED_HIGH);
			spi_write_register(reg_way, ((val & ~mask) | mask),
								SPI_SPEED_HIGH);
		} else {
			spi_read_register(reg_way, &val, SPI_SPEED_HIGH);
			spi_write_register(reg_way, (val & ~mask),
								SPI_SPEED_HIGH);
		}
	}
}

static int scpsys_domain_is_on(const struct scp_domain_data *scpd)
{
	u32 status, status2, val;

	spi_read_register(SPM_PWR_STATUS, &val, SPI_SPEED_HIGH);
	status = val & scpd->sta_mask;

	spi_read_register(SPM_PWR_STATUS_2ND, &val, SPI_SPEED_HIGH);
	status2 = val & scpd->sta_mask;

	/*
	 * A domain is on when both status bits are set. If only one is set
	 * return an error. This happens while powering up a domain
	 */

	if (status && status2)
		return true;
	if (!status && !status2)
		return false;

	return -EINVAL;
}

static int scpsys_power_on(int id)
{
	const struct scp_domain_data *scpd = &scp_domain_data_mt8570[id];
	u32 ctl_addr = SCPSYS_BASE + scpd->ctl_ofs;
	u32 sram_pdn_addr = SCPSYS_BASE + scpd->sram_pdn_ofs;
	u32 sram_pdn_ack = scpd->sram_pdn_ack_bits;
	u32 val;
	int ret = 0;
	int time = 0;

	spi_read_register(ctl_addr, &val, SPI_SPEED_HIGH);
	val |= PWR_ON_BIT;
	spi_write_register(ctl_addr, val, SPI_SPEED_HIGH);
	val |= PWR_ON_2ND_BIT;
	spi_write_register(ctl_addr, val, SPI_SPEED_HIGH);

	/* wait until PWR_ACK = 1 */
	while (1) {
		ret = scpsys_domain_is_on(scpd);
		if (ret > 0)
			break;

		udelay(10);
		time++;
		if (time > 100) {
			pr_err("%s: wait %s pwr_status timeout\n",
				__func__, scpd->name);
			goto err;
		}
	}

	val &= ~PWR_CLK_DIS_BIT;
	spi_write_register(ctl_addr, val, SPI_SPEED_HIGH);

	val &= ~PWR_ISO_BIT;
	spi_write_register(ctl_addr, val, SPI_SPEED_HIGH);

	val |= PWR_RST_B_BIT;
	spi_write_register(ctl_addr, val, SPI_SPEED_HIGH);

	spi_read_register(sram_pdn_addr, &val, SPI_SPEED_HIGH);
	val &= ~scpd->sram_pdn_bits;
	spi_write_register(sram_pdn_addr, val, SPI_SPEED_HIGH);

	time = 0;
	/* wait until SRAM_PDN_ACK all 0 */
	spi_read_register(sram_pdn_addr, &val, SPI_SPEED_HIGH);
	while (sram_pdn_ack && (val & sram_pdn_ack)) {
		udelay(10);
		time++;
		if (time > 100) {
			pr_err("%s: wait %s sram_pdn_ack timeout,",
				__func__, scpd->name);
			pr_err("sram_pdn_ack val = 0x%x\n", val);
			goto err;
		}
		spi_read_register(sram_pdn_addr, &val, SPI_SPEED_HIGH);
	}

	if (scpd->way_table)
		scpsys_set_way_en(scpd->way_table, true);

	if (scpd->prot_table) {
		ret = scpsys_clear_bus_prot(scpd->prot_table);
		if (ret) {
			pr_err("%s: clear %s bus protection error,",
				 __func__, scpd->name);
			pr_err("ret = %d\n", ret);
			goto err;
		}
	}

	pr_info("%s: OK to power on mtcmos %s\n", __func__, scpd->name);
	return 0;

err:
	pr_err("%s: Failed to power on mtcmos %s\n", __func__, scpd->name);
	return ret;
}

void mtcmos_init(void)
{
	int i, ret;

	for (i = MT8570_POWER_DOMAIN_DSP_SRAM1; i < MT8570_POWER_DOMAIN_NR; i++)
		ret = scpsys_power_on(i);
}

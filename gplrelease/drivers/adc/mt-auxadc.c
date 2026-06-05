// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Mingming Lee <mingming.lee@mediatek.com>
 */

#include <common.h>
#include <adc.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <linux/iopoll.h>

/* Register definitions */
#define MT_AUXADC_CON0                    0x00
#define MT_AUXADC_CON1                    0x04
#define MT_AUXADC_CON2                    0x10
#define MT_AUXADC_STA                     BIT(0)

#define MT_AUXADC_DAT0                    0x14
#define MT_AUXADC_RDY0                    BIT(12)

#define MT_AUXADC_MISC                    0x94
#define MT_AUXADC_PDN_EN                  BIT(14)

#define MT_AUXADC_DAT_MASK                0xfff
#define MT_AUXADC_SLEEP_US                1000
#define MT_AUXADC_TIMEOUT_US              10000
#define MT_AUXADC_POWER_READY_MS          1
#define MT_AUXADC_SAMPLE_READY_US         25
#define CHANNEL_NUMS                      16
#define BIT_NUMS                          12

static inline void mt_auxadc_mod_reg(void __iomem *reg,
					 u32 or_mask, u32 and_mask)
{
	u32 val;

	val = readl(reg);
	val |= or_mask;
	val &= ~and_mask;
	writel(val, reg);
}

struct mt_auxadc_priv {
	void __iomem *regs;
	int active_channel;
	struct clk adc_clk;
	struct mutex lock;
};

int mt_auxadc_channel_data(struct udevice *dev, int channel,
				 unsigned int *data)
{
	struct mt_auxadc_priv *priv = dev_get_priv(dev);
	void __iomem *reg_channel;
	int ret;
	int val;

	if (channel != priv->active_channel) {
		pr_err("Requested channel is not active!");
		return -EINVAL;
	}

	reg_channel = priv->regs + MT_AUXADC_DAT0 +
		      channel * 0x04;

	mutex_lock(&priv->lock);

	mt_auxadc_mod_reg(priv->regs + MT_AUXADC_CON1,
			      0, 1 << channel);

	/* read channel and make sure old ready bit == 0 */
	ret = readl_poll_timeout(reg_channel, val,
				 ((val & MT_AUXADC_RDY0) == 0),
				 MT_AUXADC_TIMEOUT_US);
	if (ret < 0) {
		pr_err("wait for channel[%d] ready bit clear time out\n",
			channel);
		goto err_timeout;
	}

	/* set bit to trigger sample */
	mt_auxadc_mod_reg(priv->regs + MT_AUXADC_CON1,
			      1 << channel, 0);

	/* we must delay here for hardware sample channel data */
	udelay(MT_AUXADC_SAMPLE_READY_US);

	/* check MTK_AUXADC_CON2 if auxadc is idle */
	ret = readl_poll_timeout(priv->regs + MT_AUXADC_CON2, val,
				 ((val & MT_AUXADC_STA) == 0),
				 MT_AUXADC_TIMEOUT_US);
	if (ret < 0) {
		pr_err("wait for auxadc idle time out\n");
		goto err_timeout;
	}

	/* read channel and make sure ready bit == 1 */
	ret = readl_poll_timeout(reg_channel, val,
				 ((val & MT_AUXADC_RDY0) != 0),
				 MT_AUXADC_TIMEOUT_US);
	if (ret < 0) {
		pr_err("wait for channel[%d] data ready time out\n",
			channel);
		goto err_timeout;
	}

	/* read data */
	*data = readl(reg_channel) & MT_AUXADC_DAT_MASK;

	mutex_unlock(&priv->lock);

	return 0;

err_timeout:

	mutex_unlock(&priv->lock);

	return -ETIMEDOUT;
}

int mt_auxadc_start_channel(struct udevice *dev, int channel)
{
	struct mt_auxadc_priv *priv = dev_get_priv(dev);

	if (channel < 0 || channel >= CHANNEL_NUMS) {
		pr_err("Requested channel is invalid!");
		return -EINVAL;
	}
	mt_auxadc_mod_reg(priv->regs + MT_AUXADC_MISC,
					  MT_AUXADC_PDN_EN, 0);
	priv->active_channel = channel;

	return 0;
}

int mt_auxadc_stop(struct udevice *dev)
{
	struct mt_auxadc_priv *priv = dev_get_priv(dev);

	mt_auxadc_mod_reg(priv->regs + MT_AUXADC_MISC,
					  0, MT_AUXADC_PDN_EN);
	priv->active_channel = -1;

	return 0;
}

int mt_auxadc_probe(struct udevice *dev)
{
	struct mt_auxadc_priv *priv = dev_get_priv(dev);
	int ret;

	priv->regs = dev_read_addr_ptr(dev);
	if (priv->regs == (void *)FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &priv->adc_clk);
	if (ret)
		return ret;

	ret = clk_enable(&priv->adc_clk);
	if (ret)
		return ret;

	mutex_init(&priv->lock);

	mt_auxadc_mod_reg(priv->regs + MT_AUXADC_MISC,
					  MT_AUXADC_PDN_EN, 0);
	mdelay(MT_AUXADC_POWER_READY_MS);

	priv->active_channel = -1;

	return 0;
}

int mt_auxadc_ofdata_to_platdata(struct udevice *dev)
{
	struct adc_uclass_platdata *uc_pdata = dev_get_uclass_platdata(dev);

	uc_pdata->data_mask = (1 << BIT_NUMS) - 1;
	uc_pdata->data_format = ADC_DATA_FORMAT_BIN;
	uc_pdata->data_timeout_us = 0;

	uc_pdata->channel_mask = (1 << CHANNEL_NUMS) - 1;

	return 0;
}

static const struct adc_ops mt_auxadc_ops = {
	.start_channel = mt_auxadc_start_channel,
	.channel_data = mt_auxadc_channel_data,
	.stop = mt_auxadc_stop,
};

static const struct udevice_id mt_auxadc_ids[] = {
	{ .compatible = "mediatek,auxadc" },
	{ }
};

U_BOOT_DRIVER(mt_auxadc) = {
	.name		= "mt-auxadc",
	.id		= UCLASS_ADC,
	.of_match	= mt_auxadc_ids,
	.ops		= &mt_auxadc_ops,
	.probe		= mt_auxadc_probe,
	.ofdata_to_platdata = mt_auxadc_ofdata_to_platdata,
	.priv_auto_alloc_size = sizeof(struct mt_auxadc_priv),
};

// SPDX-License-Identifier: GPL-2.0
/*
 * Watchdog driver for MediaTek SoCs
 *
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Ryder Lee <ryder.lee@mediatek.com>
 */

#include <common.h>
#include <dm.h>
#include <wdt.h>
#include <asm/io.h>

#define MTK_WDT_MODE			0x00
#define MTK_WDT_LENGTH			0x04
#define MTK_WDT_RESTART			0x08
#define MTK_WDT_STATUS			0x0c
#define MTK_WDT_INTERVAL		0x10
#define MTK_WDT_SWRST			0x14
#define MTK_WDT_NONRST_REG		0x20
#define MTK_WDT_NONRST_REG2		0x24
#define MTK_WDT_REQ_MODE		0x30
#define MTK_WDT_DEBUG_CTL		0x40

#define WDT_MODE_KEY			(0x22 << 24)
#define WDT_MODE_EN			BIT(0)
#define WDT_MODE_EXTPOL			BIT(1)
#define WDT_MODE_EXTEN			BIT(2)
#define WDT_MODE_IRQ_EN			BIT(3)
#define WDT_MODE_DUAL_EN		BIT(6)

#define WDT_LENGTH_KEY			0x8
#define WDT_LENGTH_TIMEOUT(n)		((n) << 5)

#define WDT_RESTART_KEY			0x1971
#define WDT_SWRST_KEY			0x1209

struct mtk_wdt_priv {
	void __iomem *base;
};

static void __iomem *toprgu_base;

static bool mtk_wd_CheckNonResetReg2(unsigned int offset)
{
	u32 tmp;

	tmp = readl(toprgu_base + MTK_WDT_NONRST_REG2);
	if (tmp & (1U << offset))
		return true;
	else
		return false;
}

static void mtk_wd_SetNonResetReg2(unsigned int offset, bool value)
{
	u32 reg;

	reg = readl(toprgu_base + MTK_WDT_NONRST_REG2);
	if (value)
		reg |= (1U << offset);
	else
		reg &= ~(1U << offset);

	writel(reg, toprgu_base + MTK_WDT_NONRST_REG2);
}

void set_clr_rpmbpk_mode(bool flag)
{
	mtk_wd_SetNonResetReg2(0x0, !!flag);

	debug("Enter %s\n", __func__);
}

bool check_rpmbpk_mode(void)
{
	return mtk_wd_CheckNonResetReg2(0x0);
}

void set_clr_fastboot_mode(bool flag)
{
	if (flag)
		mtk_wd_SetNonResetReg2(0x2, 1);
	else
		mtk_wd_SetNonResetReg2(0x2, 0);

	debug("Enter %s\n", __func__);
}

bool check_fastboot_mode(void)
{
#if !(CFG_FPGA_PLATFORM)
	return mtk_wd_CheckNonResetReg2(0x2);
#else
	return false;
#endif
}

static int mtk_wdt_reset(struct udevice *dev)
{
	struct mtk_wdt_priv *priv = dev_get_priv(dev);

	/* Reload watchdog duration */
	writel(WDT_RESTART_KEY, priv->base + MTK_WDT_RESTART);

	return 0;
}

static int mtk_wdt_stop(struct udevice *dev)
{
	struct mtk_wdt_priv *priv = dev_get_priv(dev);

	clrsetbits_le32(priv->base + MTK_WDT_MODE, WDT_MODE_EN, WDT_MODE_KEY);

	return 0;
}

static int mtk_wdt_expire_now(struct udevice *dev, ulong flags)
{
	struct mtk_wdt_priv *priv = dev_get_priv(dev);

	/* Kick watchdog to prevent counter == 0 */
	writel(WDT_RESTART_KEY, priv->base + MTK_WDT_RESTART);

	/* Reset */
	writel(WDT_SWRST_KEY, priv->base + MTK_WDT_SWRST);
	hang();

	return 0;
}

static void mtk_wdt_set_timeout(struct udevice *dev, unsigned int timeout)
{
	struct mtk_wdt_priv *priv = dev_get_priv(dev);

	/*
	 * One bit is the value of 512 ticks
	 * The clock has 32 KHz
	 */
	timeout = WDT_LENGTH_TIMEOUT(timeout << 6) | WDT_LENGTH_KEY;
	writel(timeout, priv->base + MTK_WDT_LENGTH);

	mtk_wdt_reset(dev);
}

static int mtk_wdt_start(struct udevice *dev, u64 timeout, ulong flags)
{
	struct mtk_wdt_priv *priv = dev_get_priv(dev);

	mtk_wdt_set_timeout(dev, timeout);

	/* Enable watchdog reset signal */
	setbits_le32(priv->base + MTK_WDT_MODE,
		     WDT_MODE_EN | WDT_MODE_KEY | WDT_MODE_EXTEN);

	return 0;
}

static int mtk_wdt_probe(struct udevice *dev)
{
	struct mtk_wdt_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	toprgu_base = priv->base;
	if (!priv->base)
		return -ENOENT;

	/* Clear status */
	clrsetbits_le32(priv->base + MTK_WDT_MODE,
			WDT_MODE_IRQ_EN | WDT_MODE_EXTPOL, WDT_MODE_KEY);

	return mtk_wdt_stop(dev);
}

static const struct wdt_ops mtk_wdt_ops = {
	.start = mtk_wdt_start,
	.reset = mtk_wdt_reset,
	.stop = mtk_wdt_stop,
	.expire_now = mtk_wdt_expire_now,
};

static const struct udevice_id mtk_wdt_ids[] = {
	{ .compatible = "mediatek,wdt"},
	{}
};

U_BOOT_DRIVER(mtk_wdt) = {
	.name = "mtk_wdt",
	.id = UCLASS_WDT,
	.of_match = mtk_wdt_ids,
	.priv_auto_alloc_size = sizeof(struct mtk_wdt_priv),
	.probe = mtk_wdt_probe,
	.ops = &mtk_wdt_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

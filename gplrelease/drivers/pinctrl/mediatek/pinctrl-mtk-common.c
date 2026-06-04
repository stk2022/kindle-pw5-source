// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Author: Ryder Lee <ryder.lee@mediatek.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/pinctrl.h>
#include <asm/io.h>
#include <asm-generic/gpio.h>
#include <linux/io.h>
#include "pinctrl-mtk-common.h"

/**
 * struct mtk_drive_desc - the structure that holds the information
 *			    of the driving current
 * @min:	the minimum current of this group
 * @max:	the maximum current of this group
 * @step:	the step current of this group
 * @scal:	the weight factor
 *
 * formula: output = ((input) / step - 1) * scal
 */
struct mtk_drive_desc {
	u8 min;
	u8 max;
	u8 step;
	u8 scal;
};

/* The groups of drive strength */
static const struct mtk_drive_desc mtk_drive[] = {
	[DRV_GRP0] = { 4, 16, 4, 1 },
	[DRV_GRP1] = { 4, 16, 4, 2 },
	[DRV_GRP2] = { 2, 8, 2, 1 },
	[DRV_GRP3] = { 2, 8, 2, 2 },
	[DRV_GRP4] = { 2, 16, 2, 1 },
};

static const char *mtk_pinctrl_dummy_name = "_dummy";

static void mtk_w32(struct udevice *dev, u32 reg, u32 val)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	__raw_writel(val, priv->base + reg);
}

static u32 mtk_r32(struct udevice *dev, u32 reg)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	return __raw_readl(priv->base + reg);
}

static inline int get_count_order(unsigned int count)
{
	int order;

	order = fls(count) - 1;
	if (count & (count - 1))
		order++;
	return order;
}

void mtk_rmw(struct udevice *dev, u32 reg, u32 mask, u32 set)
{
	u32 val;

	val = mtk_r32(dev, reg);
	val &= ~mask;
	val |= set;
	mtk_w32(dev, reg, val);
}

static int mtk_hw_pin_field_lookup(struct udevice *dev, int pin,
				   const struct mtk_pin_reg_calc *rc,
				   struct mtk_pin_field *pfd)
{
	const struct mtk_pin_field_calc *c, *e;
	u32 bits;

	c = rc->range;
	e = c + rc->nranges;

	while (c < e) {
		if (pin >= c->s_pin && pin <= c->e_pin)
			break;
		c++;
	}

	if (c >= e)
		return -EINVAL;

	/* Calculated bits as the overall offset the pin is located at,
	 * if c->fixed is held, that determines the all the pins in the
	 * range use the same field with the s_pin.
	 */
	bits = c->fixed ? c->s_bit : c->s_bit + (pin - c->s_pin) * (c->x_bits);

	/* Fill pfd from bits. For example 32-bit register applied is assumed
	 * when c->sz_reg is equal to 32.
	 */
	pfd->offset = c->s_addr + c->x_addrs * (bits / c->sz_reg);
	pfd->bitpos = bits % c->sz_reg;
	pfd->mask = (1 << c->x_bits) - 1;

	/* pfd->next is used for indicating that bit wrapping-around happens
	 * which requires the manipulation for bit 0 starting in the next
	 * register to form the complete field read/write.
	 */
	pfd->next = pfd->bitpos + c->x_bits > c->sz_reg ? c->x_addrs : 0;

	return 0;
}

static int mtk_hw_pin_field_get(struct udevice *dev, int pin,
				int field, struct mtk_pin_field *pfd)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);
	const struct mtk_pin_reg_calc *rc;

	if (field < 0 || field >= PINCTRL_PIN_REG_MAX)
		return -EINVAL;

	if (priv->soc->reg_cal && priv->soc->reg_cal[field].range)
		rc = &priv->soc->reg_cal[field];
	else
		return -EINVAL;

	return mtk_hw_pin_field_lookup(dev, pin, rc, pfd);
}

static void mtk_hw_bits_part(struct mtk_pin_field *pf, int *h, int *l)
{
	*l = 32 - pf->bitpos;
	*h = get_count_order(pf->mask) - *l;
}

static void mtk_hw_write_cross_field(struct udevice *dev,
				     struct mtk_pin_field *pf, int value)
{
	int nbits_l, nbits_h;

	mtk_hw_bits_part(pf, &nbits_h, &nbits_l);

	mtk_rmw(dev, pf->offset, pf->mask << pf->bitpos,
		(value & pf->mask) << pf->bitpos);

	mtk_rmw(dev, pf->offset + pf->next, BIT(nbits_h) - 1,
		(value & pf->mask) >> nbits_l);
}

static void mtk_hw_read_cross_field(struct udevice *dev,
				    struct mtk_pin_field *pf, int *value)
{
	int nbits_l, nbits_h, h, l;

	mtk_hw_bits_part(pf, &nbits_h, &nbits_l);

	l  = (mtk_r32(dev, pf->offset) >> pf->bitpos) & (BIT(nbits_l) - 1);
	h  = (mtk_r32(dev, pf->offset + pf->next)) & (BIT(nbits_h) - 1);

	*value = (h << nbits_l) | l;
}

static int mtk_hw_set_value(struct udevice *dev, int pin, int field,
			    int value)
{
	struct mtk_pin_field pf;
	int err;

	err = mtk_hw_pin_field_get(dev, pin, field, &pf);
	if (err)
		return err;

	if (!pf.next)
		mtk_rmw(dev, pf.offset, pf.mask << pf.bitpos,
			(value & pf.mask) << pf.bitpos);
	else
		mtk_hw_write_cross_field(dev, &pf, value);

	return 0;
}

static int mtk_hw_get_value(struct udevice *dev, int pin, int field,
			    int *value)
{
	struct mtk_pin_field pf;
	int err;

	err = mtk_hw_pin_field_get(dev, pin, field, &pf);
	if (err)
		return err;

	if (!pf.next)
		*value = (mtk_r32(dev, pf.offset) >> pf.bitpos) & pf.mask;
	else
		mtk_hw_read_cross_field(dev, &pf, value);

	return 0;
}

static int mtk_get_groups_count(struct udevice *dev)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	return priv->soc->ngrps;
}

static const char *mtk_get_pin_name(struct udevice *dev,
				    unsigned int selector)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	if (!priv->soc->grps[selector].name)
		return mtk_pinctrl_dummy_name;

	return priv->soc->pins[selector].name;
}

static int mtk_get_pins_count(struct udevice *dev)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	return priv->soc->npins;
}

static const char *mtk_get_group_name(struct udevice *dev,
				      unsigned int selector)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	if (!priv->soc->grps[selector].name)
		return mtk_pinctrl_dummy_name;

	return priv->soc->grps[selector].name;
}

static int mtk_get_functions_count(struct udevice *dev)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	return priv->soc->nfuncs;
}

static const char *mtk_get_function_name(struct udevice *dev,
					 unsigned int selector)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	if (!priv->soc->funcs[selector].name)
		return mtk_pinctrl_dummy_name;

	return priv->soc->funcs[selector].name;
}

static int mtk_pinmux_group_set(struct udevice *dev,
				unsigned int group_selector,
				unsigned int func_selector)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);
	const struct mtk_group_desc *grp =
			&priv->soc->grps[group_selector];
	int i;

	for (i = 0; i < grp->num_pins; i++) {
		int *pin_modes = grp->data;

		mtk_hw_set_value(dev, grp->pins[i], PINCTRL_PIN_REG_MODE,
				 pin_modes[i]);
	}

	return 0;
}

#if CONFIG_IS_ENABLED(PINCONF)
static const struct pinconf_param mtk_conf_params[] = {
	{ "bias-disable", PIN_CONFIG_BIAS_DISABLE, 0 },
	{ "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 1 },
	{ "bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 1 },
	{ "input-schmitt-enable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 1 },
	{ "input-schmitt-disable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 0 },
	{ "input-enable", PIN_CONFIG_INPUT_ENABLE, 1 },
	{ "input-disable", PIN_CONFIG_INPUT_ENABLE, 0 },
	{ "output-enable", PIN_CONFIG_OUTPUT_ENABLE, 1 },
	{ "output-high", PIN_CONFIG_OUTPUT, 1, },
	{ "output-low", PIN_CONFIG_OUTPUT, 0, },
	{ "drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 0 },
};

int mtk_pinconf_drive_set(struct udevice *dev, u32 pin, u32 arg)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);
	const struct mtk_pin_desc *desc = &priv->soc->pins[pin];
	const struct mtk_drive_desc *tb;
	int err = -ENOTSUPP;

	tb = &mtk_drive[desc->drv_n];
	/* 4mA when (e8, e4) = (0, 0)
	 * 8mA when (e8, e4) = (0, 1)
	 * 12mA when (e8, e4) = (1, 0)
	 * 16mA when (e8, e4) = (1, 1)
	 */
	if ((arg >= tb->min && arg <= tb->max) && !(arg % tb->step)) {
		arg = (arg / tb->step - 1) * tb->scal;

		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_DRV, arg);
		if (err)
			return err;
	}
	return 0;
}

int mtk_pinconf_bias_set_pullsel_pullen(struct udevice *dev, u32 pin, u32 arg)
{
	int err = -EINVAL;

	if (arg & 2) {
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_PULLEN, 1);
		if (err)
			return err;
	} else {
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_PULLEN, 0);
		if (err)
			return err;
	}
	if (arg == 3) {
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_PULLSEL, 1);
		if (err)
			return err;
	} else {
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_PULLSEL, 0);
		if (err)
			return err;
	}

	return 0;
}

int mtk_pinconf_bias_set_pupd_r1_r0(struct udevice *dev, u32 pin, u32 arg)
{
	int err = -EINVAL;
	int r0, r1, pull;

	if (arg == MTK_DISABLE) {
		r0 = 0;
		r1 = 0;
	} else {
		r0 = 1;
		r1 = 0;
	}

	err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_R0, r0);
	if (err)
		return err;

	err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_R1, r1);
	if (err)
		return err;

	pull = (arg == 3) ? 0 : 1;

	err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_PUPD, pull);
	if (err)
		return err;

	return 0;
}

int mtk_pinconf_bias_set_combo(struct udevice *dev, u32 pin, u32 arg)
{
	int err;

	err = mtk_pinconf_bias_set_pupd_r1_r0(dev, pin, arg);
	if (!err)
		goto out;
	err = mtk_pinconf_bias_set_pullsel_pullen(dev, pin, arg);

out:
	return err;
}

static int mtk_pinconf_set(struct udevice *dev, unsigned int pin,
			   unsigned int param, unsigned int arg)
{
	int err = 0;
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
		arg = (param == PIN_CONFIG_BIAS_DISABLE) ? 0 :
			(param == PIN_CONFIG_BIAS_PULL_UP) ? 3 : 2;
		if (priv->soc->bias_set_combo) {
			err = priv->soc->bias_set_combo(dev, pin, arg);
			if (!err)
				return err;
		}
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_PULLSEL,
				       arg & 1);
		if (err)
			goto err;

		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_PULLEN,
				       !!(arg & 2));
		if (err)
			goto err;
		break;
	case PIN_CONFIG_OUTPUT_ENABLE:
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_SMT, 0);
		if (err)
			goto err;
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_DIR, 1);
		if (err)
			goto err;
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_IES, !!arg);
		if (err)
			goto err;
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_DIR, 0);
		if (err)
			goto err;
		break;
	case PIN_CONFIG_OUTPUT:
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_DIR, 1);
		if (err)
			goto err;

		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_DO, arg);
		if (err)
			goto err;
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		/* arg = 1: Input mode & SMT enable ;
		 * arg = 0: Output mode & SMT disable
		 */
		arg = arg ? 2 : 1;
		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_DIR,
				       arg & 1);
		if (err)
			goto err;

		err = mtk_hw_set_value(dev, pin, PINCTRL_PIN_REG_SMT,
				       !!(arg & 2));
		if (err)
			goto err;
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		err = mtk_pinconf_drive_set(dev, pin, arg);
		if (err)
			goto err;
		break;

	default:
		err = -ENOTSUPP;
	}

err:

	return err;
}

static int mtk_pinconf_group_set(struct udevice *dev,
				 unsigned int group_selector,
				 unsigned int param, unsigned int arg)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);
	const struct mtk_group_desc *grp =
			&priv->soc->grps[group_selector];
	int i, ret;

	for (i = 0; i < grp->num_pins; i++) {
		ret = mtk_pinconf_set(dev, grp->pins[i], param, arg);
		if (ret)
			return ret;
	}

	return 0;
}
#endif

const struct pinctrl_ops mtk_pinctrl_ops = {
	.get_pins_count = mtk_get_pins_count,
	.get_pin_name = mtk_get_pin_name,
	.get_groups_count = mtk_get_groups_count,
	.get_group_name = mtk_get_group_name,
	.get_functions_count = mtk_get_functions_count,
	.get_function_name = mtk_get_function_name,
	.pinmux_group_set = mtk_pinmux_group_set,
#if CONFIG_IS_ENABLED(PINCONF)
	.pinconf_num_params = ARRAY_SIZE(mtk_conf_params),
	.pinconf_params = mtk_conf_params,
	.pinconf_set = mtk_pinconf_set,
	.pinconf_group_set = mtk_pinconf_group_set,
#endif
	.set_state = pinctrl_generic_set_state,
};

static int mtk_gpio_get(struct udevice *dev, unsigned int off)
{
	int val, err;

	err = mtk_hw_get_value(dev->parent, off, PINCTRL_PIN_REG_DI, &val);
	if (err)
		return err;

	return !!val;
}

static int mtk_gpio_set(struct udevice *dev, unsigned int off, int val)
{
	return mtk_hw_set_value(dev->parent, off, PINCTRL_PIN_REG_DO, !!val);
}

static int mtk_gpio_get_direction(struct udevice *dev, unsigned int off)
{
	int val, err;

	err = mtk_hw_get_value(dev->parent, off, PINCTRL_PIN_REG_DIR, &val);
	if (err)
		return err;

	return val ? GPIOF_OUTPUT : GPIOF_INPUT;
}

static int mtk_gpio_direction_input(struct udevice *dev, unsigned int off)
{
	return mtk_hw_set_value(dev->parent, off, PINCTRL_PIN_REG_DIR, 0);
}

static int mtk_gpio_direction_output(struct udevice *dev,
				     unsigned int off, int val)
{
	mtk_gpio_set(dev, off, val);

	/* And set the requested value */
	return mtk_hw_set_value(dev->parent, off, PINCTRL_PIN_REG_DIR, 1);
}

static int mtk_gpio_request(struct udevice *dev, unsigned int off,
			    const char *label)
{
	return mtk_hw_set_value(dev->parent, off, PINCTRL_PIN_REG_MODE, 0);
}

static int mtk_gpio_probe(struct udevice *dev)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev->parent);
	struct gpio_dev_priv *uc_priv;

	uc_priv = dev_get_uclass_priv(dev);
	uc_priv->bank_name = priv->soc->name;
	uc_priv->gpio_count = priv->soc->npins;

	return 0;
}

static const struct dm_gpio_ops mtk_gpio_ops = {
	.request = mtk_gpio_request,
	.set_value = mtk_gpio_set,
	.get_value = mtk_gpio_get,
	.get_function = mtk_gpio_get_direction,
	.direction_input = mtk_gpio_direction_input,
	.direction_output = mtk_gpio_direction_output,
};

static struct driver mtk_gpio_driver = {
	.name = "mediatek_gpio",
	.id	= UCLASS_GPIO,
	.probe = mtk_gpio_probe,
	.ops = &mtk_gpio_ops,
};

static int mtk_gpiochip_register(struct udevice *parent)
{
	struct uclass_driver *drv;
	struct udevice *dev;
	int ret = 0;
	ofnode node;

	drv = lists_uclass_lookup(UCLASS_GPIO);
	if (!drv)
		return -ENOENT;

	dev_for_each_subnode(node, parent)
		if (ofnode_read_bool(node, "gpio-controller")) {
			ret = 0;
			break;
		}

	if (ret)
		return ret;

	ret = device_bind_with_driver_data(parent, &mtk_gpio_driver,
					   "mediatek_gpio", 0, node,
					   &dev);
	if (ret)
		return ret;

	return 0;
}
#define write_gpio(val, reg)  (*((volatile unsigned int *)(reg)) = (val))
#define read_gpio(reg)  (*((volatile unsigned int *)(reg)))
#define GPIO_MODE_BITS                 3
#define MAX_GPIO_MODE_PER_REG          10
#define MAX_GPIO_REG_BITS              32


void __iomem *mode_base = NULL;
void __iomem *dir_base = NULL;
void __iomem *dout_base = NULL;
void __iomem *din_base = NULL;
void __iomem *pullen_base = NULL;
void __iomem *pullsel_base = NULL;
void __iomem *ies_base = NULL;
void __iomem *smt_base = NULL;
void __iomem *pupd1_base = NULL;
void __iomem *pupd2_base = NULL;
void __iomem *pupd3_base = NULL;

typedef struct {
    int addr;
}PIN_addr;

typedef struct {
    int offset;
}PIN_offset;
#define GPIO_BASE 0x10005000
PIN_addr PUPD_addr[] = {
  /* 0 */ {GPIO_BASE+0x00F0},
  /* 1 */ {GPIO_BASE+0x00F0},
  /* 2 */ {GPIO_BASE+0x00F0},
  /* 3 */ {GPIO_BASE+0x00F0},
  /* 4 */ {GPIO_BASE+0x00F0},
  /* 5 */ {GPIO_BASE+0x00F0},
  /* 6 */ {GPIO_BASE+0x0300},
  /* 7 */ {GPIO_BASE+0x0300},
  /* 8 */ {GPIO_BASE+0x0300},
  /* 9 */ {GPIO_BASE+0x0300},
  /* 10 */ {GPIO_BASE+0x0300},
  /* 11 */ {GPIO_BASE+0x0300},
  /* 12 */ {-1},
  /* 13 */ {-1},
  /* 14 */ {-1},
  /* 15 */ {-1},
  /* 16 */ {-1},
  /* 17 */ {-1},
  /* 18 */ {-1},
  /* 19 */ {-1},
  /* 20 */ {-1},
  /* 21 */ {-1},
  /* 22 */ {-1},
  /* 23 */ {-1},
  /* 24 */ {-1},
  /* 25 */ {-1},
  /* 26 */ {-1},
  /* 27 */ {-1},
  /* 28 */ {-1},
  /* 29 */ {-1},
  /* 30 */ {-1},
  /* 31 */ {-1},
  /* 32 */ {GPIO_BASE+0x0300},
  /* 33 */ {-1},
  /* 34 */ {-1},
  /* 35 */ {-1},
  /* 36 */ {-1},
  /* 37 */ {-1},
  /* 38 */ {-1},
  /* 39 */ {-1},
  /* 40 */ {GPIO_BASE+0x0070},
  /* 41 */ {GPIO_BASE+0x0070},
  /* 42 */ {GPIO_BASE+0x0070},
  /* 43 */ {GPIO_BASE+0x0070},
  /* 44 */ {GPIO_BASE+0x0300},
  /* 45 */ {GPIO_BASE+0x0300},
  /* 46 */ {GPIO_BASE+0x0300},
  /* 47 */ {GPIO_BASE+0x0310},
  /* 48 */ {-1},
  /* 49 */ {-1},
  /* 50 */ {-1},
  /* 51 */ {-1},
  /* 52 */ {-1},
  /* 53 */ {-1},
  /* 54 */ {-1},
  /* 55 */ {-1},
  /* 56 */ {-1},
  /* 57 */ {-1},
  /* 58 */ {-1},
  /* 59 */ {-1},
  /* 60 */ {-1},
  /* 61 */ {-1},
  /* 62 */ {-1},
  /* 63 */ {-1},
  /* 64 */ {-1},
  /* 65 */ {-1},
  /* 66 */ {-1},
  /* 67 */ {-1},
  /* 68 */ {-1},
  /* 69 */ {-1},
  /* 70 */ {GPIO_BASE+0x0080},
  /* 71 */ {GPIO_BASE+0x0080},
  /* 72 */ {GPIO_BASE+0x0080},
  /* 73 */ {GPIO_BASE+0x0080},
  /* 74 */ {GPIO_BASE+0x0080},
  /* 75 */ {GPIO_BASE+0x0080},
  /* 76 */ {GPIO_BASE+0x0080},
  /* 77 */ {GPIO_BASE+0x0080},
  /* 78 */ {GPIO_BASE+0x0080},
  /* 79 */ {GPIO_BASE+0x0090},
  /* 80 */ {GPIO_BASE+0x0090},
  /* 81 */ {GPIO_BASE+0x0090},
  /* 82 */ {GPIO_BASE+0x0090},
  /* 83 */ {GPIO_BASE+0x0090},
  /* 84 */ {GPIO_BASE+0x0090},
  /* 85 */ {GPIO_BASE+0x0090},
  /* 86 */ {GPIO_BASE+0x0090},
  /* 87 */ {GPIO_BASE+0x0090},
  /* 88 */ {-1},
  /* 89 */ {-1},
  /* 90 */ {-1},
  /* 91 */ {-1},
  /* 92 */ {GPIO_BASE+0x0310},
  /* 93 */ {GPIO_BASE+0x0310},
  /* 94 */ {GPIO_BASE+0x0310},
  /* 95 */ {GPIO_BASE+0x0310},
  /* 96 */ {GPIO_BASE+0x0320},
  /* 97 */ {GPIO_BASE+0x0320},
  /* 98 */ {GPIO_BASE+0x0320},
  /* 99 */ {GPIO_BASE+0x0320},
  /* 100 */ {GPIO_BASE+0x0320},
  /* 101 */ {GPIO_BASE+0x0320},
  /* 102 */ {GPIO_BASE+0x0320},
  /* 103 */ {GPIO_BASE+0x0320},
  /* 104 */ {GPIO_BASE+0x0320},
  /* 105 */ {GPIO_BASE+0x0320},
  /* 106 */ {GPIO_BASE+0x0330},
  /* 107 */ {GPIO_BASE+0x0330},
  /* 108 */ {GPIO_BASE+0x0330},
  /* 109 */ {GPIO_BASE+0x0330},
  /* 110 */ {GPIO_BASE+0x0330},
  /* 111 */ {GPIO_BASE+0x0330},
  /* 112 */ {-1},
  /* 113 */ {-1},
  /* 114 */ {-1},
  /* 115 */ {-1}};

PIN_offset PUPD_offset[] = {
  /* 0 */ {14},
  /* 1 */ {17},
  /* 2 */ {20},
  /* 3 */ {23},
  /* 4 */ {26},
  /* 5 */ {29},
  /* 6 */ {2},
  /* 7 */ {5},
  /* 8 */ {8},
  /* 9 */ {11},
  /* 10 */ {14},
  /* 11 */ {17},
  /* 12 */ {-1},
  /* 13 */ {-1},
  /* 14 */ {-1},
  /* 15 */ {-1},
  /* 16 */ {-1},
  /* 17 */ {-1},
  /* 18 */ {-1},
  /* 19 */ {-1},
  /* 20 */ {-1},
  /* 21 */ {-1},
  /* 22 */ {-1},
  /* 23 */ {-1},
  /* 24 */ {-1},
  /* 25 */ {-1},
  /* 26 */ {-1},
  /* 27 */ {-1},
  /* 28 */ {-1},
  /* 29 */ {-1},
  /* 30 */ {-1},
  /* 31 */ {-1},
  /* 32 */ {20},
  /* 33 */ {-1},
  /* 34 */ {-1},
  /* 35 */ {-1},
  /* 36 */ {-1},
  /* 37 */ {-1},
  /* 38 */ {-1},
  /* 39 */ {-1},
  /* 40 */ {2},
  /* 41 */ {5},
  /* 42 */ {8},
  /* 43 */ {11},
  /* 44 */ {23},
  /* 45 */ {26},
  /* 46 */ {29},
  /* 47 */ {2},
  /* 48 */ {-1},
  /* 49 */ {-1},
  /* 50 */ {-1},
  /* 51 */ {-1},
  /* 52 */ {-1},
  /* 53 */ {-1},
  /* 54 */ {-1},
  /* 55 */ {-1},
  /* 56 */ {-1},
  /* 57 */ {-1},
  /* 58 */ {-1},
  /* 59 */ {-1},
  /* 60 */ {-1},
  /* 61 */ {-1},
  /* 62 */ {-1},
  /* 63 */ {-1},
  /* 64 */ {-1},
  /* 65 */ {-1},
  /* 66 */ {-1},
  /* 67 */ {-1},
  /* 68 */ {-1},
  /* 69 */ {-1},
  /* 70 */ {5},
  /* 71 */ {8},
  /* 72 */ {11},
  /* 73 */ {14},
  /* 74 */ {17},
  /* 75 */ {20},
  /* 76 */ {23},
  /* 77 */ {26},
  /* 78 */ {29},
  /* 79 */ {2},
  /* 80 */ {5},
  /* 81 */ {8},
  /* 82 */ {11},
  /* 83 */ {14},
  /* 84 */ {17},
  /* 85 */ {20},
  /* 86 */ {23},
  /* 87 */ {5},
  /* 88 */ {-1},
  /* 89 */ {-1},
  /* 90 */ {-1},
  /* 91 */ {-1},
  /* 92 */ {20},
  /* 93 */ {23},
  /* 94 */ {26},
  /* 95 */ {29},
  /* 96 */ {2},
  /* 97 */ {5},
  /* 98 */ {8},
  /* 99 */ {11},
  /* 100 */ {14},
  /* 101 */ {17},
  /* 102 */ {20},
  /* 103 */ {23},
  /* 104 */ {26},
  /* 105 */ {29},
  /* 106 */ {2},
  /* 107 */ {5},
  /* 108 */ {8},
  /* 109 */ {11},
  /* 110 */ {14},
  /* 111 */ {17},
  /* 112 */ {-1},
  /* 113 */ {-1},
  /* 114 */ {-1},
  /* 115 */ {-1}};

PIN_offset R0_offset[] = {
  /* 0 */ {12},
  /* 1 */ {15},
  /* 2 */ {18},
  /* 3 */ {21},
  /* 4 */ {24},
  /* 5 */ {27},
  /* 6 */ {0},
  /* 7 */ {3},
  /* 8 */ {6},
  /* 9 */ {9},
  /* 10 */ {12},
  /* 11 */ {15},
  /* 12 */ {-1},
  /* 13 */ {-1},
  /* 14 */ {-1},
  /* 15 */ {-1},
  /* 16 */ {-1},
  /* 17 */ {-1},
  /* 18 */ {-1},
  /* 19 */ {-1},
  /* 20 */ {-1},
  /* 21 */ {-1},
  /* 22 */ {-1},
  /* 23 */ {-1},
  /* 24 */ {-1},
  /* 25 */ {-1},
  /* 26 */ {-1},
  /* 27 */ {-1},
  /* 28 */ {-1},
  /* 29 */ {-1},
  /* 30 */ {-1},
  /* 31 */ {-1},
  /* 32 */ {18},
  /* 33 */ {-1},
  /* 34 */ {-1},
  /* 35 */ {-1},
  /* 36 */ {-1},
  /* 37 */ {-1},
  /* 38 */ {-1},
  /* 39 */ {-1},
  /* 40 */ {0},
  /* 41 */ {3},
  /* 42 */ {6},
  /* 43 */ {9},
  /* 44 */ {21},
  /* 45 */ {24},
  /* 46 */ {27},
  /* 47 */ {0},
  /* 48 */ {-1},
  /* 49 */ {-1},
  /* 50 */ {-1},
  /* 51 */ {-1},
  /* 52 */ {-1},
  /* 53 */ {-1},
  /* 54 */ {-1},
  /* 55 */ {-1},
  /* 56 */ {-1},
  /* 57 */ {-1},
  /* 58 */ {-1},
  /* 59 */ {-1},
  /* 60 */ {-1},
  /* 61 */ {-1},
  /* 62 */ {-1},
  /* 63 */ {-1},
  /* 64 */ {-1},
  /* 65 */ {-1},
  /* 66 */ {-1},
  /* 67 */ {-1},
  /* 68 */ {-1},
  /* 69 */ {-1},
  /* 70 */ {3},
  /* 71 */ {6},
  /* 72 */ {9},
  /* 73 */ {12},
  /* 74 */ {15},
  /* 75 */ {18},
  /* 76 */ {21},
  /* 77 */ {24},
  /* 78 */ {27},
  /* 79 */ {0},
  /* 80 */ {3},
  /* 81 */ {6},
  /* 82 */ {9},
  /* 83 */ {12},
  /* 84 */ {15},
  /* 85 */ {18},
  /* 86 */ {21},
  /* 87 */ {3},
  /* 88 */ {-1},
  /* 89 */ {-1},
  /* 90 */ {-1},
  /* 91 */ {-1},
  /* 92 */ {18},
  /* 93 */ {21},
  /* 94 */ {24},
  /* 95 */ {27},
  /* 96 */ {0},
  /* 97 */ {3},
  /* 98 */ {6},
  /* 99 */ {9},
  /* 100 */ {12},
  /* 101 */ {15},
  /* 102 */ {18},
  /* 103 */ {21},
  /* 104 */ {24},
  /* 105 */ {27},
  /* 106 */ {0},
  /* 107 */ {3},
  /* 108 */ {6},
  /* 109 */ {9},
  /* 110 */ {12},
  /* 111 */ {15},
  /* 112 */ {-1},
  /* 113 */ {-1},
  /* 114 */ {-1},
  /* 115 */ {-1}};

PIN_offset R1_offset[] = {
 /* 0 */ {13},
  /* 1 */ {16},
  /* 2 */ {19},
  /* 3 */ {22},
  /* 4 */ {25},
  /* 5 */ {28},
  /* 6 */ {1},
  /* 7 */ {4},
  /* 8 */ {7},
  /* 9 */ {10},
  /* 10 */ {13},
  /* 11 */ {16},
  /* 12 */ {-1},
  /* 13 */ {-1},
  /* 14 */ {-1},
  /* 15 */ {-1},
  /* 16 */ {-1},
  /* 17 */ {-1},
  /* 18 */ {-1},
  /* 19 */ {-1},
  /* 20 */ {-1},
  /* 21 */ {-1},
  /* 22 */ {-1},
  /* 23 */ {-1},
  /* 24 */ {-1},
  /* 25 */ {-1},
  /* 26 */ {-1},
  /* 27 */ {-1},
  /* 28 */ {-1},
  /* 29 */ {-1},
  /* 30 */ {-1},
  /* 31 */ {-1},
  /* 32 */ {19},
  /* 33 */ {-1},
  /* 34 */ {-1},
  /* 35 */ {-1},
  /* 36 */ {-1},
  /* 37 */ {-1},
  /* 38 */ {-1},
  /* 39 */ {-1},
  /* 40 */ {1},
  /* 41 */ {4},
  /* 42 */ {7},
  /* 43 */ {10},
  /* 44 */ {22},
  /* 45 */ {25},
  /* 46 */ {28},
  /* 47 */ {1},
  /* 48 */ {-1},
  /* 49 */ {-1},
  /* 50 */ {-1},
  /* 51 */ {-1},
  /* 52 */ {-1},
  /* 53 */ {-1},
  /* 54 */ {-1},
  /* 55 */ {-1},
  /* 56 */ {-1},
  /* 57 */ {-1},
  /* 58 */ {-1},
  /* 59 */ {-1},
  /* 60 */ {-1},
  /* 61 */ {-1},
  /* 62 */ {-1},
  /* 63 */ {-1},
  /* 64 */ {-1},
  /* 65 */ {-1},
  /* 66 */ {-1},
  /* 67 */ {-1},
  /* 68 */ {-1},
  /* 69 */ {-1},
  /* 70 */ {4},
  /* 71 */ {7},
  /* 72 */ {10},
  /* 73 */ {13},
  /* 74 */ {16},
  /* 75 */ {19},
  /* 76 */ {22},
  /* 77 */ {25},
  /* 78 */ {28},
  /* 79 */ {1},
  /* 80 */ {4},
  /* 81 */ {7},
  /* 82 */ {10},
  /* 83 */ {13},
  /* 84 */ {16},
  /* 85 */ {19},
  /* 86 */ {22},
  /* 87 */ {4},
  /* 88 */ {-1},
  /* 89 */ {-1},
  /* 90 */ {-1},
  /* 91 */ {-1},
  /* 92 */ {19},
  /* 93 */ {22},
  /* 94 */ {25},
  /* 95 */ {28},
  /* 96 */ {1},
  /* 97 */ {4},
  /* 98 */ {7},
  /* 99 */ {10},
  /* 100 */ {13},
  /* 101 */ {16},
  /* 102 */ {19},
  /* 103 */ {22},
  /* 104 */ {25},
  /* 105 */ {28},
  /* 106 */ {1},
  /* 107 */ {4},
  /* 108 */ {7},
  /* 109 */ {10},
  /* 110 */ {13},
  /* 111 */ {16},
  /* 112 */ {-1},
  /* 113 */ {-1},
  /* 114 */ {-1},
  /* 115 */ {-1}};


struct mtk_pin_ies_smt_grp {
	uint16_t pin;
	uint16_t index;
	uint16_t bit;
};

static struct mtk_pin_ies_smt_grp mt8512_pin_ies_smt[] = {
	{0, 0, 0},
	{1, 0, 0},
	{2, 0, 0},
	{3, 0, 1},
	{4, 0, 1},
	{5, 0, 1},
	{6, 0, 2},
	{7, 0, 2},
	{8, 0, 3},
	{9, 0, 3},
	{10, 0, 3},
	{11, 0, 3},
	{12, 0, 4},
	{13, 0, 4},
	{14, 0, 4},
	{15, 0, 4},
	{16, 0, 5},
	{17, 0, 5},
	{18, 0, 5},
	{19, 0, 5},
	{20, 0, 6},
	{21, 0, 7},
	{22, 0, 7},
	{23, 0, 7},
	{24, 0, 7},
	{25, 0, 7},
	{26, 0, 8},
	{27, 0, 8},
	{28, 0, 9},
	{29, 0, 9},
	{30, 0, 9},
	{31, 0, 9},
	{32, 0, 10},
	{33, 0, 11},
	{34, 0, 11},
	{35, 0, 11},
	{36, 0, 11},
	{37, 0, 11},
	{38, 0, 11},
	{39, 0, 11},
	{40, 0, 12},
	{41, 0, 13},
	{42, 0, 13},
	{43, 0, 13},
	{44, 0, 14},
	{45, 0, 14},
	{46, 0, 14},
	{47, 0, 14},
	{48, 0, 15},
	{49, 0, 15},
	{50, 0, 15},
	{51, 0, 15},
	{52, 0, 16},
	{53, 0, 16},
	{54, 0, 17},
	{55, 0, 17},
	{56, 0, 17},
	{57, 0, 17},
	{58, 0, 18},
	{59, 0, 18},
	{60, 0, 18},
	{61, 0, 18},
	{62, 0, 18},
	{63, 0, 18},
	{64, 0, 19},
	{65, 0, 19},
	{66, 0, 20},
	{67, 0, 20},
	{68, 0, 21},
	{69, 0, 21},
	{70, 0, 22},
	{71, 0, 23},
	{72, 0, 24},
	{73, 0, 25},
	{74, 0, 26},
	{75, 0, 27},
	{76, 0, 28},
	{77, 0, 29},
	{78, 0, 30},
	{79, 0, 31},
	{80, 1, 0},
	{81, 1, 1},
	{82, 1, 2},
	{83, 1, 3},
	{84, 1, 4},
	{85, 1, 5},
	{86, 1, 5},
	{87, 1, 6},
	{88, 1, 7},
	{89, 1, 7},
	{90, 1, 7},
	{91, 1, 7},
	{92, 1, 8},
	{93, 1, 8},
	{94, 1, 8},
	{95, 1, 8},
	{96, 1, 8},
	{97, 1, 8},
	{98, 1, 8},
	{99, 1, 9},
	{100, 1, 9},
	{101, 1, 9},
	{102, 1, 10},
	{103, 1, 10},
	{104, 1, 10},
	{105, 1, 11},
	{106, 1, 11},
	{107, 1, 11},
	{108, 1, 11},
	{109, 1, 11},
	{110, 1, 11},
	{111, 1, 11},
	{112, 1, 12},
	{113, 1, 12},
	{114, 1, 12},
	{115, 1, 12}
};

#define ARRAY_SIZE(a)       (sizeof(a) / sizeof(a[0]))

int mt_set_gpio_ies_enable_chip(unsigned int pin, int enable)
{
	const struct mtk_pin_ies_smt_grp *ies_ctrl;
	ies_base = ioremap(0x10005410, 0x20);
	ies_ctrl = &mt8512_pin_ies_smt[pin];

	if (enable == 0) {
		write_gpio(1 << (ies_ctrl->bit), ies_base + (ies_ctrl->index * 0x10) + 0x8);
	} else {
		write_gpio(1 << (ies_ctrl->bit), ies_base + (ies_ctrl->index * 0x10) + 0x8);
	}
	return 0;
}



int mt_set_gpio_dir_chip(unsigned int pin, int dir)
{
	unsigned int pos;
	unsigned int bit;

	dir_base = ioremap(0x10005140, 0x40);

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;


		if (dir == 0) {
			write_gpio(1 << bit, dir_base + 0x8 + pos * 0x10);
		} else {
			write_gpio(1 << bit, dir_base + 0x4 + pos * 0x10);
		}
	return 0;
}


/*---------------------------------------------------------------------------*/
int mt_get_gpio_dir_chip(unsigned int pin)
{
	unsigned int pos;
	unsigned int bit;
	unsigned int reg;
	dir_base = ioremap(0x10005140, 0x40);

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	/* To reduce memory usage, we don't use DIR_addr[] array*/
	reg = read_gpio(dir_base + pos * 0x10);
	return (((reg & (1L << bit)) != 0)? 1: 0);
}
/*---------------------------------------------------------------------------*/


int mt_set_gpio_out_chip(unsigned int pin, int out)
{
	unsigned int pos;
	unsigned int bit;

	dout_base = ioremap(0x100050a0, 0x40);

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;


		if (out == 0) {
			write_gpio(1 << bit, dout_base + 0x8 + pos * 0x10);
		} else {
			write_gpio(1 << bit, dout_base + 0x4 + pos * 0x10);
		}
	return 0;
}

/*---------------------------------------------------------------------------*/
int mt_get_gpio_out_chip(unsigned int pin)
{
	unsigned int pos;
	unsigned int bit;
	unsigned int reg;
	dout_base = ioremap(0x100050a0, 0x40);

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	/* To reduce memory usage, we don't use DATAOUT_addr[] array*/
	reg = read_gpio(dout_base + pos * 0x10);
	return (((reg & (1L << bit)) != 0)? 1: 0);
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_in_chip(unsigned int pin)
{
	unsigned int pos;
	unsigned int bit;
	unsigned int reg;
	din_base = ioremap(0x10005000, 0x10);

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	/* To reduce memory usage, we don't use DIN_addr[] array*/
	reg = read_gpio(din_base + pos * 0x10);
	return (((reg & (1L << bit)) != 0)? 1: 0);
}
/*---------------------------------------------------------------------------*/
int mt_set_gpio_mode_chip(unsigned pin, int mode)
{
	unsigned long pos;
	unsigned long bit;
	unsigned long data;
	unsigned int mask;
	mode_base = ioremap(0x100051e0, 0x120);
	mask = (1L << GPIO_MODE_BITS) - 1;

	pos = pin / MAX_GPIO_MODE_PER_REG;
	bit = pin % MAX_GPIO_MODE_PER_REG;


		data = read_gpio(mode_base + pos * 0x10);
		data &= ~(mask << (GPIO_MODE_BITS*bit));
		data |= (mode << (GPIO_MODE_BITS*bit));

		write_gpio(data, mode_base + pos * 0x10);

	return 0;
}


/*---------------------------------------------------------------------------*/
int mt_get_gpio_mode_chip(unsigned int pin)
{
	unsigned long pos;
	unsigned long bit;
	unsigned long data;
	unsigned int mask;
	mode_base = ioremap(0x100051e0, 0x120);
	mask = (1L << GPIO_MODE_BITS) - 1;

	pos = pin / MAX_GPIO_MODE_PER_REG;
	bit = (pin % MAX_GPIO_MODE_PER_REG) * GPIO_MODE_BITS;

	data = read_gpio(mode_base + pos * 0x10);
	return (data >> bit) & mask;
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void mt_get_gpio_ies_smt_chip(void)
{
	int i = 0;
	ies_base = ioremap(0x10005410, 0x20);
	smt_base = ioremap(0x10005470, 0x20);

	pupd1_base = ioremap(0x10005070, 0x30);
	pupd2_base = ioremap(0x100050f0, 0x10);
	pupd3_base = ioremap(0x10005300, 0x40);
	for (i = 0; i < 2; i++)
		printf("ies[%d] = 0x%x\n", i, read_gpio(ies_base + i * 0x10));
	for (i = 0; i < 2; i++)
		printf("smt[%d] = 0x%x\n", i, read_gpio(smt_base + i * 0x10));
	for (i = 0; i < 3; i++)
		printf("pupd1[%d] = 0x%x\n", i, read_gpio(pupd1_base + i * 0x10));
	printf("pupd2 = 0x%x\n", read_gpio(pupd2_base));
	for (i = 0; i < 4; i++)
		printf("pupd3[%d] = 0x%x\n", i, read_gpio(pupd3_base + i * 0x10));
}

int mt_set_gpio_pull_enable_chip(unsigned int pin, int enable)
{
	unsigned int pos;
	unsigned int bit;
	int reg = 0;

	/*for special pin pupd*/
	if (-1 != PUPD_offset[pin].offset) {
		reg = read_gpio(ioremap(PUPD_addr[pin].addr, 0x10));
		if (0 == enable) {
			reg &= (~(1 << (R1_offset[pin].offset)));
			reg &= (~(1 << (R0_offset[pin].offset)));
		} else {
			reg &= (~(1 << (R1_offset[pin].offset)));
			reg |= (1 << (R0_offset[pin].offset));
		}
		write_gpio(reg, ioremap(PUPD_addr[pin].addr, 0x10));
	} else {

		pullen_base = ioremap(0x10005860, 0x40);

		pos = pin / MAX_GPIO_REG_BITS;
		bit = pin % MAX_GPIO_REG_BITS;

		if (enable == 0)
			write_gpio( (1L << bit), pullen_base + pos * 0x10 + 0x8);
		else
			write_gpio( (1L << bit), pullen_base + pos * 0x10 + 0x4);
	}
	return 0;
}


/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_select_chip(unsigned int pin, int select)
{
	u32 pos;
	u32 bit;
	u32 reg = 0;

	/*for special msdc pupd*/
	if (-1 != PUPD_offset[pin].offset) {
		reg = read_gpio(ioremap(PUPD_addr[pin].addr, 0x10));
		reg &= (~(1 << (R1_offset[pin].offset)));
		reg |= (1 << (R0_offset[pin].offset));
		if (select == 1) {
			reg &= (~(1 << (PUPD_offset[pin].offset)));
		} else if (select == 0) {
			reg |= (1 << (PUPD_offset[pin].offset));
		}
		write_gpio(reg, ioremap(PUPD_addr[pin].addr, 0x10));
	} else {
		pos = pin / MAX_GPIO_REG_BITS;
		bit = pin % MAX_GPIO_REG_BITS;
		pullsel_base = ioremap(0x10005900, 0x10);
		if (select == 0)
			write_gpio( (1L << bit), pullsel_base + pos * 0x10 + 0x8);
		else
			write_gpio( (1L << bit), pullsel_base + pos * 0x10 + 0x4);
	}

	return 0;
}



/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_enable(unsigned int pin)
{
	unsigned int pos;
	unsigned int bit;
	unsigned int reg;
	pullen_base = ioremap(0x10005860, 0x10);

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	/* To reduce memory usage, we don't use DIN_addr[] array*/
	reg = read_gpio(pullen_base + pos * 0x10);
	return (((reg & (1L << bit)) != 0)? 1: 0);

}


/*---------------------------------------------------------------------------*/
/* get pull-up or pull-down, regardless of resistor value */
s32 mt_get_gpio_pull_select(unsigned int pin)
{
	unsigned int pos;
	unsigned int bit;
	unsigned int reg;
	pullsel_base = ioremap(0x10005900, 0x10);

	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;

	/* To reduce memory usage, we don't use DIN_addr[] array*/
	reg = read_gpio(pullsel_base + pos * 0x10);
	return (((reg & (1L << bit)) != 0)? 1: 0);

}

static int mt_get_gpio_pull_select_chip(unsigned int pin)
{
	unsigned long data;
	unsigned long data1;
	unsigned long r0;
	unsigned long r1;
	unsigned long pupd;

	if (-1 != PUPD_offset[pin].offset) {
		data = read_gpio(ioremap(PUPD_addr[pin].addr, 0x10));
		pupd = (data >> (PUPD_offset[pin].offset)) & 0x1;
		r0 = (data >> (R0_offset[pin].offset)) & 0x1;
		r1 = (data >> (R1_offset[pin].offset)) & 0x1;
		if (r0 ==0 && r1 == 0)
			return 2;	/*High Z(no pull) */
		else if (pupd == 0)
			return 1;	/* pull up */
		else if (pupd == 1)
			return 0;	/* pull down */
		else
			return -1;
	} else {
		return mt_get_gpio_pull_select(pin);
	}
}



/*---------------------------------------------------------------------------*/
static int mt_get_gpio_pull_enable_chip(unsigned int pin)
{
	int res = 0;
	if (-1 != PUPD_offset[pin].offset) {
		res = mt_get_gpio_pull_select_chip(pin);
		if (2 == res)
			return 0;	/*disable */
		if (1 == res || 0 == res)
			return 1;	/*enable */
		if (-1 == res)
			return -1;
	} else {
	return mt_get_gpio_pull_enable(pin);
	}
}

void gpio_show_pins_info(void)
{
	int i = 0;

	printf("PIN: (MODE)(DIR)(DOUT)(DIN)(PULL_EN)(PULL_SEL)\n");
	for (i = 0; i < 116; i++) {
			printf("%03d: %1d%1d%1d%1d%1d%1d",
					i,
					mt_get_gpio_mode_chip(i),
					mt_get_gpio_dir_chip(i),
					mt_get_gpio_out_chip(i),
					mt_get_gpio_in_chip(i),
					mt_get_gpio_pull_enable_chip(i),
					mt_get_gpio_pull_select_chip(i));

			printf("\n");
		}
	mt_get_gpio_ies_smt_chip();
}

void gpio_set_same_to_kernel(void)
{
	int mode0_attr[] = {40, 42, 44, 45, 46, 47, 48, 64, 65};
	int mode1_attr[] = {54, 55, 56, 57};
	int hz_attr[] = {0, 1, 14, 15, 16, 17, 29, 31, 68, 69, 100};
	int input_attr[] = {44};
	int outputlow_attr[] = {9, 12, 45, 47};
	int outputhigh_attr[] = {64, 65};
	int pullup_attr[] = {32, 34, 35, 42, 54, 57, 80,81,82, 83, 84, 85, 86};
	int pulldown_attr[] = {10, 18, 19, 20, 40, 41, 43, 44, 45, 46, 47, 48, 87, 102, 103, 104};
	int i;

	for(i = 0; i < ARRAY_SIZE(mode0_attr); i++)
		mt_set_gpio_mode_chip(mode0_attr[i], 0);
	for(i = 0; i < ARRAY_SIZE(mode1_attr); i++)
		mt_set_gpio_mode_chip(mode1_attr[i], 1);
	for(i = 0; i < ARRAY_SIZE(input_attr); i++)
		mt_set_gpio_dir_chip(input_attr[i], 0);
	for(i = 0; i < ARRAY_SIZE(outputhigh_attr); i++) {
		mt_set_gpio_dir_chip(outputhigh_attr[i], 1);
		mt_set_gpio_out_chip(outputhigh_attr[i], 1);
	}
	for(i = 0; i < ARRAY_SIZE(outputlow_attr); i++)
		mt_set_gpio_dir_chip(outputlow_attr[i], 1);
	for(i = 0; i < ARRAY_SIZE(pulldown_attr); i++) {
		mt_set_gpio_pull_enable_chip(pulldown_attr[i], 1);
		mt_set_gpio_pull_select_chip(pulldown_attr[i], 0);
	}
	for(i = 0; i < ARRAY_SIZE(pullup_attr); i++) {
		mt_set_gpio_pull_enable_chip(pullup_attr[i], 1);
		mt_set_gpio_pull_select_chip(pullup_attr[i], 1);
	}
	for(i = 0; i < ARRAY_SIZE(hz_attr); i++)
		mt_set_gpio_pull_enable_chip(hz_attr[i], 0);
}





int mtk_pinctrl_common_probe(struct udevice *dev,
			     struct mtk_pinctrl_soc *soc)
{
	struct mtk_pinctrl_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = dev_read_addr_ptr(dev);
	if (priv->base == (void *)FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->soc = soc;

	ret = mtk_gpiochip_register(dev);
	if (ret)
		return ret;

	return 0;
}

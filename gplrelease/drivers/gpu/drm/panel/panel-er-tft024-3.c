/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dbi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/backlight.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

struct ili9341 {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct regulator_bulk_data supplies[1];
	struct gpio_desc *reset_gpio;
	u32 power_on_delay;
	u32 reset_delay;
	u32 init_delay;

	u8 version;
	u8 id;
	bool prepared;
	bool enabled;

	int error;
};

#define FRAME_WIDTH	(240)
#define FRAME_HEIGHT	(240)

#define ili9341_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	ili9341_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define ili9341_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	ili9341_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct ili9341 *panel_to_ili9341(struct drm_panel *panel)
{
	return container_of(panel, struct ili9341, panel);
}

static int ili9341_clear_error(struct ili9341 *ctx)
{
	int ret = ctx->error;

	ctx->error = 0;
	return ret;
}

static void ili9341_dcs_write(struct ili9341 *ctx, const void *data,
				size_t len)
{
	/*struct mipi_dbi_device *dbi = to_mipi_dbi_device(ctx->dev); */
	struct mipi_dbi_device *dbi = to_mipi_dbi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dbi_dcs_write_buffer(dbi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing dcs seq: %*ph\n",
			ret, (int)len, data);
		ctx->error = ret;
	}
}

static void ili9341_panel_init(struct ili9341 *ctx)
{
	DRM_INFO("%s enter\n", __func__);

	ili9341_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);

	ili9341_dcs_write_seq_static(ctx, 0xcf, 0x00, 0xc3, 0x30);
	ili9341_dcs_write_seq_static(ctx, 0xed, 0x64, 0x03, 0x12, 0x81);
	ili9341_dcs_write_seq_static(ctx, 0xe8, 0x85, 0x10, 0x79);
	ili9341_dcs_write_seq_static(ctx, 0xcb, 0x39, 0x2c, 0x00, 0x34, 0x02);
	ili9341_dcs_write_seq_static(ctx, 0xf7, 0x20);
	ili9341_dcs_write_seq_static(ctx, 0xea, 0x00, 0x00);
	ili9341_dcs_write_seq_static(ctx, 0xc0, 0x22);
	ili9341_dcs_write_seq_static(ctx, 0xc1, 0x11);
	ili9341_dcs_write_seq_static(ctx, 0xc5, 0x3d, 0x20);
	ili9341_dcs_write_seq_static(ctx, 0xc7, 0xaa);
	ili9341_dcs_write_seq_static(ctx, 0x36, 0x08);
	ili9341_dcs_write_seq_static(ctx, 0x3a, 0x55);
	ili9341_dcs_write_seq_static(ctx, 0xb1, 0x00, 0x13);
	ili9341_dcs_write_seq_static(ctx, 0xb6, 0x0a, 0xa2);
	ili9341_dcs_write_seq_static(ctx, 0xf6, 0x01, 0x30);
	ili9341_dcs_write_seq_static(ctx, 0xf2, 0x00);
	ili9341_dcs_write_seq_static(ctx, 0x26, 0x01);

	ili9341_dcs_write_seq_static(ctx, 0xe0, 0x0f, 0x3f, 0x2f, 0x0c, 0x10,
		0x0a, 0x53, 0xd5, 0x40, 0x0a, 0x13, 0x03, 0x08, 0x03, 0x00);
	ili9341_dcs_write_seq_static(ctx, 0xe1, 0x00, 0x00, 0x10, 0x03, 0x0f,
		0x05, 0x2c, 0xa2, 0x3f, 0x05, 0x0e, 0x0c, 0x37, 0x3c, 0x0f);

	ili9341_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);

	ili9341_dcs_write_seq_static(ctx, 0x29);
	mdelay(50);
}

static void ili9341_set_sequence(struct ili9341 *ctx)
{
	ili9341_panel_init(ctx);
}

static int ili9341_power_on(struct ili9341 *ctx)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		dev_err(ctx->dev, "failed to enable regulators: %d\n", ret);

	mdelay(10);

	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(20);

	return ret;
}

static int ili9341_power_off(struct ili9341 *ctx)
{
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(20);

	return regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
}

static int ili9341_disable(struct drm_panel *panel)
{
	struct ili9341 *ctx = panel_to_ili9341(panel);
	int ret = 0;

	if (!ctx->enabled)
		return 0;

	ret = ili9341_power_off(ctx);

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return ret;
}

static int ili9341_unprepare(struct drm_panel *panel)
{
	struct ili9341 *ctx = panel_to_ili9341(panel);

	if (!ctx->prepared)
		return 0;

	ili9341_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	ili9341_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	ili9341_clear_error(ctx);

	ctx->prepared = false;

	return 0;
}

static int ili9341_prepare(struct drm_panel *panel)
{
	struct ili9341 *ctx = panel_to_ili9341(panel);
	int ret = 0;

	DRM_INFO("%s enter\n", __func__);

	if (ctx->prepared)
		return ret;

	if (ret < 0)
		return ret;

	ili9341_set_sequence(ctx);

	ret = ctx->error;
	if (ret < 0)
		ili9341_unprepare(panel);

	ctx->prepared = true;

	return ret;
}

static int ili9341_enable(struct drm_panel *panel)
{
	struct ili9341 *ctx = panel_to_ili9341(panel);
	int ret = 0;

	if (ctx->enabled)
		return 0;

	ret = ili9341_power_on(ctx);

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return ret;
}

static struct drm_display_mode default_mode = {
	.clock = 7000,
	.hdisplay = 240,
	/* hdisplay + front porch */
	.hsync_start = 240 + 10,
	/* hdisplay + front porch + sync width */
	.hsync_end = 240 + 10 + 10,
	/* hdisplay + front porch + sync width + back porch */
	.htotal = 240 + 10 + 10 + 20,
	.vdisplay = 320,
	/* vdisplay + front porch */
	.vsync_start = 320 + 4,
	/*vdisplay + front porch + sync width */
	.vsync_end = 320 + 4 + 2,
	/* vdisplay + front porch + sync width + back porch */
	.vtotal = 320 + 4 + 2 + 2,
	.vrefresh = 60,
};

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int ili9341_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	drm_mode_probed_add(panel->connector, mode);

	panel->connector->display_info.width_mm = 36;
	panel->connector->display_info.height_mm = 48;

	return 1;
}

static const struct drm_panel_funcs ili9341_drm_funcs = {
	.disable = ili9341_disable,
	.unprepare = ili9341_unprepare,
	.prepare = ili9341_prepare,
	.enable = ili9341_enable,
	.get_modes = ili9341_get_modes,
};

static int ili9341_probe(struct mipi_dbi_device *dbi)
{
	struct device *dev = &dbi->dev;
	struct ili9341 *ctx;
	struct device_node *backlight;
	int ret = 0;

	DRM_INFO("ili9341_probe\n");

	ctx = devm_kzalloc(dev, sizeof(struct ili9341), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dbi_set_drvdata(dbi, ctx);

	ctx->dev = dev;

	ctx->supplies[0].supply = "vdd33";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		dev_err(dev, "failed to get regulators: %d\n", ret);

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight) {
			dev_err(dev, "failed to get backlight:%p, %p\n",
				backlight, ctx->backlight);
			return -EPROBE_DEFER;
		}
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
/*		return PTR_ERR(ctx->reset_gpio); */
	} else {
		ret = gpiod_direction_output(ctx->reset_gpio, 1);
		if (ret < 0) {
			dev_err(dev, "cannot configure reset-gpios %d\n", ret);
	/*		return ret; */
		}
	}
	/* 8, 16, 32 bits */
	dbi->bus_width = MIPI_DBI_BUS_WIDTH_8_BITS;

	/* 8BITS = 0, 9BITS = 1, 16BITS = 2 */
	/* 18BITS = 3, 24BITS = 4, 32BITS = 5 */
	dbi->data_width = MIPI_DBI_DATA_WIDTH_8BITS;

	/* RGB332 = 0, RGB444 = 1, RGB565 = 2, RGB666 = 3, RGB888 = 4 */
	dbi->format = MIPI_DBI_FORMAT_RGB565;

	ctx->prepared = false;
	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &ili9341_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dbi_attach(dbi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	return ret;
}

static int ili9341_remove(struct mipi_dbi_device *dbi)
{
	struct ili9341 *ctx = mipi_dbi_get_drvdata(dbi);

	mipi_dbi_detach(dbi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id ilitek_of_match[] = {
	{ .compatible = "ilitek,ili9341", },
	{ }
};

MODULE_DEVICE_TABLE(of, sitronix_of_match);

static struct mipi_dbi_driver ili9341_driver = {
	.probe = ili9341_probe,
	.remove = ili9341_remove,
	.driver = {
		.name = "panel-er-tft024-3",
		.owner = THIS_MODULE,
		.of_match_table = ilitek_of_match,
	},
};

module_mipi_dbi_driver(ili9341_driver);

MODULE_AUTHOR("shaoming chen <shaoming.chen@mediatek.com>");
MODULE_DESCRIPTION("ILI9341 LCD Panel Driver");
MODULE_LICENSE("GPL v2");

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

struct st7789h2 {
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

#define st7789h2_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	st7789h2_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define st7789h2_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	st7789h2_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct st7789h2 *panel_to_st7789h2(struct drm_panel *panel)
{
	return container_of(panel, struct st7789h2, panel);
}

static int st7789h2_clear_error(struct st7789h2 *ctx)
{
	int ret = ctx->error;

	ctx->error = 0;
	return ret;
}

static void st7789h2_dcs_write(struct st7789h2 *ctx, const void *data,
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

static void st7789h2_panel_init(struct st7789h2 *ctx)
{
	DRM_INFO("%s enter\n", __func__);

	st7789h2_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);

	st7789h2_dcs_write_seq_static(ctx, 0x36, 0x00);
	st7789h2_dcs_write_seq_static(ctx, 0x35, 0x00);
	st7789h2_dcs_write_seq_static(ctx, 0x2a, 0x00, 0x00, 0x00, 0xef);
	st7789h2_dcs_write_seq_static(ctx, 0x2b, 0x00, 0x00, 0x00, 0xef);
	st7789h2_dcs_write_seq_static(ctx, 0x3a, 0x55);
	st7789h2_dcs_write_seq_static(ctx, 0xB2, 0x1c, 0x1c, 0x01, 0xff, 0x33);
	st7789h2_dcs_write_seq_static(ctx, 0xB3, 0x10, 0xff, 0x0f);
	st7789h2_dcs_write_seq_static(ctx, 0xb4, 0x0b);
	st7789h2_dcs_write_seq_static(ctx, 0xb5, 0x9f);
	st7789h2_dcs_write_seq_static(ctx, 0xb7, 0x35);
	st7789h2_dcs_write_seq_static(ctx, 0xbb, 0x28);
	st7789h2_dcs_write_seq_static(ctx, 0xbc, 0xec);
	st7789h2_dcs_write_seq_static(ctx, 0xbd, 0xfe);
	st7789h2_dcs_write_seq_static(ctx, 0xc0, 0x2c);
	st7789h2_dcs_write_seq_static(ctx, 0xc2, 0x01);
	st7789h2_dcs_write_seq_static(ctx, 0xc3, 0x1e);
	st7789h2_dcs_write_seq_static(ctx, 0xc4, 0x20);
	st7789h2_dcs_write_seq_static(ctx, 0xc6, 0x0f);
	st7789h2_dcs_write_seq_static(ctx, 0xd0, 0xa4, 0xa1);

	st7789h2_dcs_write_seq_static(ctx, 0xe0, 0xd0, 0x00, 0x00, 0x08,
		    0x07, 0x05, 0x29, 0x54, 0x41, 0x3c, 0x17, 0x15, 0x1a, 0x20);
	st7789h2_dcs_write_seq_static(ctx, 0xe1, 0xd0, 0x00, 0x00, 0x08,
		    0x07, 0x04, 0x29, 0x44, 0x42, 0x3b, 0x16, 0x15, 0x1b, 0x1f);

	st7789h2_dcs_write_seq_static(ctx, 0x2c);
	st7789h2_dcs_write_seq_static(ctx, 0xe7, 0x00);

	st7789h2_dcs_write_seq_static(ctx, 0x2a, 0x00, 0x00,
		((FRAME_HEIGHT - 1) & 0xff00) >> 8,
		((FRAME_HEIGHT - 1) & 0xff));
	st7789h2_dcs_write_seq_static(ctx, 0x2b, 0x00, 0x00,
		((FRAME_WIDTH - 1) & 0xff00) >> 8,
		((FRAME_WIDTH - 1) & 0xff));

	st7789h2_dcs_write_seq_static(ctx, 0xe7, 0x00);
	st7789h2_dcs_write_seq_static(ctx, 0x29);
	mdelay(20);

	st7789h2_dcs_write_seq_static(ctx, 0x2c);
}

static void st7789h2_set_sequence(struct st7789h2 *ctx)
{
	st7789h2_panel_init(ctx);
}

static int st7789h2_power_on(struct st7789h2 *ctx)
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

static int st7789h2_power_off(struct st7789h2 *ctx)
{
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(20);

	return regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
}

static int st7789h2_disable(struct drm_panel *panel)
{
	struct st7789h2 *ctx = panel_to_st7789h2(panel);
	int ret = 0;

	if (!ctx->enabled)
		return 0;

	ret = st7789h2_power_off(ctx);

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return ret;
}

static int st7789h2_unprepare(struct drm_panel *panel)
{
	struct st7789h2 *ctx = panel_to_st7789h2(panel);

	if (!ctx->prepared)
		return 0;

	st7789h2_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	st7789h2_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	st7789h2_clear_error(ctx);

	ctx->prepared = false;

	return 0;
}

static int st7789h2_prepare(struct drm_panel *panel)
{
	struct st7789h2 *ctx = panel_to_st7789h2(panel);
	int ret = 0;

	DRM_INFO("%s enter\n", __func__);

	if (ctx->prepared)
		return ret;

	if (ret < 0)
		return ret;

	st7789h2_set_sequence(ctx);

	ret = ctx->error;
	if (ret < 0)
		st7789h2_unprepare(panel);

	ctx->prepared = true;

	return ret;
}

static int st7789h2_enable(struct drm_panel *panel)
{
	struct st7789h2 *ctx = panel_to_st7789h2(panel);
	int ret = 0;

	if (ctx->enabled)
		return 0;

	ret = st7789h2_power_on(ctx);

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
	.hsync_start = 240 + 38,
	/* hdisplay + front porch + sync width */
	.hsync_end = 240 + 38 + 10,
	/* hdisplay + front porch + sync width + back porch */
	.htotal = 240 + 38 + 10 + 10,
	.vdisplay = 240,
	/* vdisplay + front porch */
	.vsync_start = 240 + 8,
	/*vdisplay + front porch + sync width */
	.vsync_end = 240 + 8 + 4,
	/* vdisplay + front porch + sync width + back porch */
	.vtotal = 240 + 8 + 4 + 4,
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

static int st7789h2_get_modes(struct drm_panel *panel)
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

	panel->connector->display_info.width_mm = 67;
	panel->connector->display_info.height_mm = 125;

	return 1;
}

static const struct drm_panel_funcs st7789h2_drm_funcs = {
	.disable = st7789h2_disable,
	.unprepare = st7789h2_unprepare,
	.prepare = st7789h2_prepare,
	.enable = st7789h2_enable,
	.get_modes = st7789h2_get_modes,
};

static int st7789h2_probe(struct mipi_dbi_device *dbi)
{
	struct device *dev = &dbi->dev;
	struct st7789h2 *ctx;
	struct device_node *backlight;
	int ret = 0;

	DRM_INFO("st7789h2_probe\n");

	ctx = devm_kzalloc(dev, sizeof(struct st7789h2), GFP_KERNEL);
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
	ctx->panel.funcs = &st7789h2_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dbi_attach(dbi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	return ret;
}

static int st7789h2_remove(struct mipi_dbi_device *dbi)
{
	struct st7789h2 *ctx = mipi_dbi_get_drvdata(dbi);

	mipi_dbi_detach(dbi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id sitronix_of_match[] = {
	{ .compatible = "sitronix,st7789h2", },
	{ }
};

MODULE_DEVICE_TABLE(of, sitronix_of_match);

static struct mipi_dbi_driver st7789h2_driver = {
	.probe = st7789h2_probe,
	.remove = st7789h2_remove,
	.driver = {
		.name = "panel-st7789h2",
		.owner = THIS_MODULE,
		.of_match_table = sitronix_of_match,
	},
};

module_mipi_dbi_driver(st7789h2_driver);

MODULE_AUTHOR("shaoming chen <shaoming.chen@mediatek.com>");
MODULE_DESCRIPTION("ST7789H2 LCD Panel Driver");
MODULE_LICENSE("GPL v2");

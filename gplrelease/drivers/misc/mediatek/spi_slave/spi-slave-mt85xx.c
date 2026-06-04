/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Laichun Tao <sin_laichuntao@mediatek.com>
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

struct mtk_spi_slave {
	struct clk *spis_clk;
};

static const struct of_device_id mtk_spi_slave_of_match[] = {
	{ .compatible = "mediatek,mt8518-spi-slave"},
	{}
};

MODULE_DEVICE_TABLE(of, mtk_spi_slave_of_match);

static int mtk_spi_slave_probe(struct platform_device *pdev)
{
	struct mtk_spi_slave *mdata;
	int ret;

	mdata = kzalloc(sizeof(*mdata), GFP_KERNEL);

	mdata->spis_clk = devm_clk_get(&pdev->dev, "spis-clk");
	if (IS_ERR(mdata->spis_clk)) {
		ret = PTR_ERR(mdata->spis_clk);
		pr_debug("failed to get spis-clk: %d\n", ret);
		goto error_probe;
	}

	if (of_property_match_string
		(pdev->dev.of_node, "clk-cg", "enable") >= 0) {
		ret = clk_prepare_enable(mdata->spis_clk);
		if (ret < 0) {
			pr_debug("failed to enable spis_clk (%d)\n", ret);
			goto error_probe;
		}
	}

	if (of_property_match_string
		(pdev->dev.of_node, "clk-cg", "disable") >= 0)
		clk_disable_unprepare(mdata->spis_clk);

	platform_set_drvdata(pdev, mdata);

	return 0;

error_probe:
	kfree(mdata);

	return ret;

}

static int mtk_spi_slave_remove(struct platform_device *pdev)
{
	struct mtk_spi_slave *mdata = dev_get_drvdata(&pdev->dev);

	clk_disable_unprepare(mdata->spis_clk);
	kfree(mdata);

	return 0;
}

static struct platform_driver mtk_spi_slave_driver = {
	.driver = {
		.name = "mtk-spi-slave",
		.of_match_table = mtk_spi_slave_of_match,
	},
	.probe = mtk_spi_slave_probe,
	.remove = mtk_spi_slave_remove,
};

module_platform_driver(mtk_spi_slave_driver);

MODULE_DESCRIPTION("MTK SPI Slave Controller driver");
MODULE_AUTHOR("Laichun Tao <sin_laichuntao@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:mtk-spi-slave");

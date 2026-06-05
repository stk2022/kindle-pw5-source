/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/falcon_storage.h>
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp-mt8512.h>

#include <linux/clk.h>

#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/mmc/host.h>

#define SECTOR_SIZE	512

#define DEV_NODE	"/falcon"

struct clk *qb_source;
struct clk *qb_hclk;
struct clk *qb_source_cg;
static int probe_done;
struct clk *qb_uart_clk;
struct clk *qb_uart_pclk;

/*
 *  Block device Host controller parameters
 */
static struct falcon_blk_host_param falcon_blk_param = {
	.name = "mt8110a-falcon-mmc",
#if 0	 /*use Multisector I/O */
	.max_seg_size	= 0xfe00,
	.max_hw_segs	= 128,
	.max_phys_segs	= 128,
	.max_req_size	= 0xfe00 * 128,
	.max_blk_size	= SECTOR_SIZE,
	.max_blk_count	= 0xfe00 * 128 / SECTOR_SIZE,
#else	/* use Bounce Buffer */
	.max_seg_size	= 0xfe00 * 128,
	.max_hw_segs	= 1,
	.max_phys_segs	= 128,
	.max_req_size	= 0xfe00 * 128,
	.max_blk_size	= SECTOR_SIZE,
	.max_blk_count	= 0xfe00 * 128 / SECTOR_SIZE,
#endif
	.irq		= -1,
	.dma_mask	= 0xffffffff,

	.heads	= 4,			/* same as the Linux SD driver */
	.sectors = 16,			/* same as the Linux SD driver */
};

static struct falcon_nand_host_param falcon_nand_param = {};

#ifdef CONFIG_FALCON_MTD_NAND
/*
 *  NAND Host controller parameters
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
static struct of_device_id falcon_nand_host_ids[] = {
	{ .compatible = "fsl,imx6q-gpmi-nand", },
	{},
};
#endif

static const char * const part_probes[] = { "cmdlinepart", NULL };

/* OOB placement block for use with hardware ecc generation */
static struct nand_ecclayout nand_oob_layout = {
	.eccbytes = 32,
	.eccpos = {
		   192, 193, 194, 195, 196, 197, 198, 199,
		   200, 201, 202, 203, 204, 205, 206, 207,
		   208, 209, 210, 211, 212, 213, 214, 215,
		   216, 217, 218, 219, 220, 221, 222, 223},
	.oobfree = {
		{.offset = 2,
		 .length = 190} }
};
#endif

/**
 * Get block dev host controller parameter
 *
 * @return    addr of host param structure
 */
struct falcon_blk_host_param *falcon_blk_get_hostinfo(void)
{
		return &falcon_blk_param;
}

/**
 * Get NAND controller parameter
 *
 * @return    addr of NAND controller param structure
 */
struct falcon_nand_host_param *falcon_nand_get_hostinfo(void)
{
	return &falcon_nand_param;
}

/**
 * Do platform depending operations
 * This is called before real HW access is done.
 */
void falcon_blk_platform_pre(void)
{
}

/**
 * Do platform depending operations
 * This is called after real HW access is done.
 */
void falcon_blk_platform_post(void)
{
}

/**
 * Initialization for platform depending operations
 * This is called once when falcon block wrapper driver is initalized.
 */
static int clock_enabled = 0;
static struct pm_qos_request vcore_req;
void falcon_blk_platform_init(void)
{
//	int i
	int irq;
	struct device_node *np;
	struct device_node *storage_np;
	const __be32 *hdl;
//	struct clk *clk;
printk("%s()\n", __func__);

	if (probe_done)
		return;

	np = of_find_node_by_path(DEV_NODE);

	if (!np) {
		pr_err("of_find_node_by_path error\n");
		return;
	}
	// params
	{
		int ret = 0;
		const char *str;

		ret = of_property_read_string(np, "storage-name", &str);
		if (0 != ret) {
			pr_err("of_get_property(storage-name) error\n");
		} else {
#ifndef CONFIG_FALCON_MTD_NAND
			falcon_blk_param.name = str;
#else
			falcon_nand_param.name = str;
#endif
		}
	}
#ifndef CONFIG_FALCON_MTD_NAND
	GET_FALCON_PROP_U32(falcon_blk_param, max_seg_size);
	GET_FALCON_PROP_U16(falcon_blk_param, max_hw_segs);
	GET_FALCON_PROP_U16(falcon_blk_param, max_phys_segs);
	GET_FALCON_PROP_U32(falcon_blk_param, max_req_size);
	GET_FALCON_PROP_U32(falcon_blk_param, max_blk_size);
	GET_FALCON_PROP_U32(falcon_blk_param, max_blk_count);

	GET_FALCON_PROP_S32(falcon_blk_param, irq);
	GET_FALCON_PROP_U64(falcon_blk_param, dma_mask);

	GET_FALCON_PROP_U8(falcon_blk_param, heads);
	GET_FALCON_PROP_U8(falcon_blk_param, sectors);

	GET_FALCON_PROP_U32(falcon_blk_param, max_discard_sectors);
	GET_FALCON_PROP_U32(falcon_blk_param, discard_granularity);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
	falcon_nand_param.of_mtable = falcon_nand_host_ids;
#endif
	falcon_nand_param.nand_oob_layout = &nand_oob_layout;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
	GET_FALCON_PROP_U8(falcon_nand_param, ppdata_flag);
#endif
	GET_FALCON_PROP_U16(falcon_nand_param, pagesize);
	GET_FALCON_PROP_U16(falcon_nand_param, sparesize);
	GET_FALCON_PROP_U8(falcon_nand_param, ecc_strength);
	GET_FALCON_PROP_U8(falcon_nand_param, irq);
#endif

	hdl = of_get_property(np, "storage", NULL);

	if (!hdl) {
		pr_err("of_get_property errror (storage)\n");
		return;
	}

	storage_np = of_find_node_by_phandle(be32_to_cpup(hdl));

	if (!storage_np) {
		pr_err("of_find_node_by_phandle error\n");
		return;
	}

	pm_qos_add_request(&vcore_req, PM_QOS_VCORE_OPP, VCORE_OPP_0);

	/* Set clk */
	if (of_get_property(storage_np, "clock-names", NULL)) {
		qb_source = of_clk_get_by_name(storage_np, "source");

		if (IS_ERR(qb_source))
			pr_err("of_clk_get_by_name error: source\n");
		else {
			clk_prepare_enable(qb_source);
			pr_info("qbblk: Enabled source\n");
		}

		qb_hclk = of_clk_get_by_name(storage_np, "hclk");

		if (IS_ERR(qb_hclk))
			pr_err("of_clk_get_by_name error: hclk\n");
		else {
			clk_prepare_enable(qb_hclk);
			pr_info("qbblk: Enabled hclk\n");
		}

		qb_source_cg = of_clk_get_by_name(storage_np, "source_cg");

		if (IS_ERR(qb_source_cg))
			pr_err("of_clk_get_by_name error: source_cg\n");
		else {
			clk_prepare_enable(qb_source_cg);
			pr_info("qbblk: Enabled source_cg\n");
		}
	} else {
		pr_err("of_get_property errror (clock-names)\n");
		return;
	}

	// irq
	irq = irq_of_parse_and_map(storage_np, 0);
	if (irq != 0)
		falcon_blk_param.irq = irq;


#if 0
	// uart
	hdl = of_get_property(np, "uart0", NULL);

	if (!hdl) {
		pr_err("of_get_property errror (uart)\n");
		return;
	}

	// clock
	storage_np = of_find_node_by_phandle(be32_to_cpup(hdl));

	if (!storage_np) {
		pr_err("of_find_node_by_phandle error\n");
		return;
	}

	qb_uart_clk = of_clk_get_by_name(storage_np, "uartclk");

	if (IS_ERR(qb_uart_clk))
		pr_err("of_clk_get_by_name error: uart clk\n");
	else {
		clk_prepare_enable(qb_uart_clk);
		pr_info("qbblk: Enabled uart clk\n");
	}

	qb_uart_pclk = of_clk_get_by_name(storage_np, "apb_pclk");

	if (IS_ERR(qb_uart_pclk))
		pr_err("of_clk_get_by_name error: apb_pclk\n");
	else {
		clk_prepare_enable(qb_uart_pclk);
		pr_info("qbblk: Enabled uart pclk\n");
	}
#endif

	probe_done = 1;
	clock_enabled = 1;
printk("%s() end\n", __func__);
}

#if !defined(CONFIG_FALCON_BLK) && !defined(CONFIG_FALCON_MTD_NAND) && !defined(CONFIG_FALCON_MTD_NOR)
EXPORT_SYMBOL(falcon_blk_platform_init);
#endif

/**
 * Do platform depended operations
 * This function is called in suspend
 */
void falcon_blk_platform_suspend(void)
{
	if (!clock_enabled) return;

	clk_disable_unprepare(qb_hclk);
	clk_disable_unprepare(qb_source);
	clk_disable_unprepare(qb_source_cg);
	pm_qos_update_request(&vcore_req, PM_QOS_VCORE_OPP_DEFAULT_VALUE);
	clock_enabled = 0;
}

/**
 * Do platform depended operations
 * This function is called in resume.
 */
void falcon_blk_platform_resume(void)
{
	if (clock_enabled) return;

	pm_qos_update_request(&vcore_req, VCORE_OPP_0);
	clk_prepare_enable(qb_source_cg);
	clk_prepare_enable(qb_source);
	clk_prepare_enable(qb_hclk);
	clock_enabled = 1;
}


static int falcon_blk_platform_probe(struct platform_device *pdev)
{
#if 0
	int ret;
	int i, irq;
	struct clk *clk;
	struct pinctrl *pinctrl;
	struct regulator *vmmc;
#endif

	if (probe_done)
		return 0;
#if 0
	/* Set clk */
	if (!qb_hclk) {
		qb_clk = devm_clk_get(&pdev->dev, "mmc_hclk");
		if (IS_ERR(qb_clk))
			pr_err("of_clk_get error\n");
		else {
			clk_prepare_enable(qb_hclk);
			pr_err("qbblk: Enabled clock\n");
		}
	}

	if (!qb_fclk) {
		qb_clk = devm_clk_get(&pdev->dev, "mmc_fclk");
		if (IS_ERR(qb_clk))
			pr_err("of_clk_get error\n");
		else {
			clk_prepare_enable(qb_fclk);
			pr_err("qbblk: Enabled clock\n");
		}
	}
#if 0
	/* Set pinctrl */
	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		pr_err("failed to get pinctrl\n");
		return ret;
	}
	ret = pinctrl_select_state(pinctrl,
				   pinctrl_lookup_state(pinctrl,
							"falcon_default"));
	if (ret) {
		pr_err("failed to activate default pinctrl state\n");
		return ret;
	}
	pr_info("qbblk(storage): Set pinctrl\n");
#endif

	/* Set regulator */
	vmmc = devm_regulator_get_optional(&pdev->dev, "vmmc");
	if (IS_ERR(vmmc))
		pr_info("qbblk(storage): Don't get regulator\n");
	else {
		ret = regulator_set_voltage(vmmc, 3300 * 1000, 3300 * 1000);
		if (ret) {
			pr_err("failed to regulator set voltage\n");
			return ret;
		}
		ret = regulator_enable(vmmc);
		if (ret) {
			pr_err("failed to enable regulator\n");
			return ret;
		}
		pr_info("qbblk(storage): Set regulator\n");
	}

	/* Get irq */
	irq = platform_get_irq(pdev, 0);
	if (irq != 0)
		falcon_blk_param.irq = irq;

	probe_done = 1;
#endif
	return 0;
}


static const struct of_device_id falcon_blk_dt_ids[] = {
	{ .compatible = "falcon_blk" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, falcon_blk_dt_ids);

static struct platform_driver falcon_blk_driver = {
	.driver		= {
		.name	= "falcon_blk",
		.of_match_table = falcon_blk_dt_ids,
	},
	.probe		= falcon_blk_platform_probe,
};
module_platform_driver(falcon_blk_driver);


/*
 * Local Variables:
 * mode: c
 * c-file-style: "K&R"
 * tab-width: 8
 * indent-tabs-mode: t
 * c-basic-offset: 8
 * End:
 */

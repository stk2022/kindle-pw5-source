/*
 * Copyright (C) 2019 MediaTek Inc.
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


#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#include <linux/compat.h>
#endif

#include "gfx_manager.h"
#include "gfx_drv.h"
#include "gfx_if.h"

static dev_t gfx_manager_devno;
static struct cdev *gfx_manager_cdev;
static struct class *gfx_manager_class;
struct gfx_device *gGfxDev;

static int gfx_manager_open(struct inode *inode, struct file *file);
static int gfx_manager_release(struct inode *inode, struct file *file);
static int gfx_manager_mmap(struct file *file,
	struct vm_area_struct *vma);
static long gfx_manager_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg);
static int gfx_manager_probe(struct platform_device *pdev);
static int gfx_manager_remove(struct platform_device *pdev);
static void gfx_manager_shutdown(struct platform_device *pdev);
static void gfx_manager_device_release(struct device *dev);
static int __init gfx_manager_init(void);
static void __exit gfx_manager_exit(void);

static int gfx_manager_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int gfx_manager_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int gfx_manager_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static long gfx_manager_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	int i4Ret = 0;

	switch (cmd) {
	case GFX_BUFF_TYPE_FLUSH:
		i4Ret = Gfx_FlushCmdbuffer((GFX_CMD_CONFIG *) arg);
		break;
	case GFX_BUFF_TYPE_GET_CMDBUFFER:
		i4Ret = Gfx_GetFreeCmdbuffer((GFX_CMD_CONFIG *) arg);
		break;
	case GFX_BUFF_TYPE_RELEASE_CMDBUFFER:
		i4Ret = Gfx_ReleaseCmdbuffer((GFX_CMD_CONFIG *) arg);
		break;
	case GFX_BUFF_TYPE_FILL_RECT:
	case GFX_BUFF_TYPE_BITBLT:
	case GFX_BUFF_TYPE_ALPHA_COMPOSITION:
		i4Ret = Gfx_SendCmdtoBufList((GFX_CMD_CONFIG *) arg);
		break;
	default:
		pr_debug("default\n");
		break;
	}

	return i4Ret;
}

static int gfx_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	i4_gfx_suspend();
	pr_debug("gfx backup in suspend\n");
	return 0;
}

static int gfx_resume(struct platform_device *pdev)
{
	i4_gfx_resume();
	pr_debug("gfx restore in resume\n");
	return 0;
}

#ifdef CONFIG_COMPAT
static long gfx_manager_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case GFX_BUFF_TYPE_GET_CMDBUFFER:
	case GFX_BUFF_TYPE_RELEASE_CMDBUFFER:
	case GFX_BUFF_TYPE_FILL_RECT:
	case GFX_BUFF_TYPE_BITBLT:
	case GFX_BUFF_TYPE_ALPHA_COMPOSITION:
		file->f_op->unlocked_ioctl(file, cmd,
					(unsigned long)compat_ptr(arg));
		break;
	default:
		break;
	}

	return 0;
}
#endif

static const struct file_operations gfx_manager_fops = {
			.owner	=	THIS_MODULE,
			.open	=	gfx_manager_open,
			.mmap	=	gfx_manager_mmap,
			.unlocked_ioctl	=	gfx_manager_ioctl,
#ifdef CONFIG_COMPAT
			.compat_ioctl	=	gfx_manager_compat_ioctl,
#endif
			.release	=	gfx_manager_release,
};

static int gfx_manager_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	int err;

	pr_debug("gfx_manager_probe 0\n");
	if (pdev->dev.of_node) {
	err = of_property_read_u32(node, "mediatek,mt8518-2DGFX0",
		&pdev->id);
		if (err)
			pr_debug("[DTS] get 2dgfx id fail!!\n");
	}

	pr_debug("2dgfx, pdev id = %d name = %s\n",
		pdev->id, pdev->name);

	gGfxDev->GFX0_DIST_BASE = of_iomap(node, 0);
	gGfxDev->irq_gfx0 = irq_of_parse_and_map(node, 0);
	pr_debug("2dgfx0, gfx0 = 0x%p irq0 = 0x%x\n",
		gGfxDev->GFX0_DIST_BASE, gGfxDev->irq_gfx0);

	if (pdev->dev.of_node) {
	err = of_property_read_u32(node,
		"mediatek,mt8518-2DGFX1", &pdev->id);
		if (err)
			pr_debug("[DTS] get 2dgfx platform_device id fail!!\n");
	}
	pr_debug("2dgfx1, pdev id = %d name = %s\n", pdev->id, pdev->name);

	gGfxDev->GFX1_DIST_BASE = of_iomap(node, 0);
	gGfxDev->irq_gfx1 = irq_of_parse_and_map(node, 0);
	pr_debug("2dgfx, gfx1 = 0x%p irq1 = 0x%x\n",
		gGfxDev->GFX0_DIST_BASE, gGfxDev->irq_gfx1);

	alloc_chrdev_region(&gfx_manager_devno, 0, 1, GFX_SESSION_DEVICE);
	gfx_manager_cdev = cdev_alloc();
	gfx_manager_cdev->owner = THIS_MODULE;
	gfx_manager_cdev->ops = &gfx_manager_fops;
	cdev_add(gfx_manager_cdev, gfx_manager_devno, 1);
	gfx_manager_class = class_create(THIS_MODULE, GFX_SESSION_DEVICE);
	device_create(gfx_manager_class, NULL, gfx_manager_devno,
		NULL, GFX_SESSION_DEVICE);
	ret = gfx_drv_init(gGfxDev->irq_gfx0, gGfxDev->irq_gfx1);

	return ret;
}

static int gfx_manager_remove(struct platform_device *pdev)
{
	int ret = 0;

	cdev_del(gfx_manager_cdev);
	unregister_chrdev_region(gfx_manager_devno, 1);
	device_destroy(gfx_manager_class, gfx_manager_devno);
	class_destroy(gfx_manager_class);
	ret = gfx_drv_uninit();

	return ret;
}

static void gfx_manager_shutdown(struct platform_device *pdev)
{

}

static const struct of_device_id mgr_of_ids[] = {
			{.compatible = "mediatek,mt8518-2DGFX0",},
			{.compatible = "mediatek,mt8518-2DGFX1",}
};

static struct platform_driver gfx_manager_driver = {
		.probe	=	gfx_manager_probe,
		.remove	=	gfx_manager_remove,
		.shutdown	=	gfx_manager_shutdown,
		.suspend	=	gfx_suspend,
		.resume	=	gfx_resume,
		.driver	=	{
		.name	=	GFX_SESSION_DEVICE,
		.owner	=	THIS_MODULE,
		.of_match_table	=	mgr_of_ids,
	},
};

static void gfx_manager_device_release(struct device *dev)
{

}

static u64 gfx_manager_device_dmamask = ~(u32) 0;

static struct platform_device gfx_manager_device = {
		.name	=	GFX_SESSION_DEVICE,
		.id		=	0,
		.dev	=	{
		.release	=	gfx_manager_device_release,
		.dma_mask	=	&gfx_manager_device_dmamask,
		.coherent_dma_mask	=	0xffffffff,
					},
		.num_resources = 0,
};

static int __init gfx_manager_init(void)
{
	pr_debug("gfx_manager_init in\n");

	if (platform_device_register(&gfx_manager_device))
		return -ENODEV;

	if (platform_driver_register(&gfx_manager_driver)) {
		platform_device_unregister(&gfx_manager_device);
		return -ENODEV;
	}
	pr_debug("gfx_manager_init out\n");

	return 0;
}

static void __exit gfx_manager_exit(void)
{
	cdev_del(gfx_manager_cdev);
	unregister_chrdev_region(gfx_manager_devno, 1);
	platform_driver_unregister(&gfx_manager_driver);
	device_destroy(gfx_manager_class, gfx_manager_devno);
	class_destroy(gfx_manager_class);
}

module_init(gfx_manager_init);
module_exit(gfx_manager_exit);
MODULE_DESCRIPTION("mediatek 2dgfx manager");
MODULE_LICENSE("GPL");

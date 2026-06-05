/*
 *****************************************************************************
 * Copyright 2017 by ams AG                                                  *
 * All rights are reserved.                                                  *
 *                                                                           *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
 * THE SOFTWARE.                                                             *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.  *
 * USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY    *
 * EXCLUDED.                                                                 *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
 *****************************************************************************
 */

/*! \file
 * \brief Device driver for monitoring ambient light intensity in (lux)
 * functionality within the AMS-TAOS TSL2572 family of devices.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>

#include <linux/init.h>

#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/irq.h>

#include <linux/i2c/ams_tsl2572.h>
#include "ams_i2c.h"
#include "ams_tsl2572_als.h"

#ifdef CONFIG_LAB126
#include <linux/miscdevice.h>
#include <linux/lab126_als.h>
#define TSL2572_ALS_DEV_MINOR		166
#endif

#ifdef CONFIG_QUALCOMM_AP
#include <linux/sensors.h>
static struct sensors_classdev als_sensors_cdev = {
    .name = "tls2572-als",
    .vendor = "AMS",
    .version = 1,
    .handle = 0,
    .type = 1,
    .max_range = "1",
    .resolution = "1",
    .sensor_power = "1",
    .min_delay = 10000,
    .max_delay = 10000,
    .fifo_reserved_event_count = 0,
    .fifo_max_event_count = 0,
    .enabled = 0,
    .delay_msec = 0,
    .sensors_enable = NULL,
};
#endif

/* TSL2572 Identifiers */
static u8 const tsl2572_ids[] = {
    /* ID */
    0x34, 0x3D};    /* TSL2572 */

/* TSL2572 Device Names */
static char const *tsl2572_names[] = { "tsl25721", "tsl25723"};

/* Registers to restore */
static u8 const restorable_regs[] = {
    TSL2572_REG_PERS, TSL2572_REG_CFG,
    TSL2572_REG_CTRL, TSL2572_REG_ATIME, TSL2572_REG_WTIME};

#ifdef CONFIG_LAB126
static struct miscdevice als_tsl2572_misc_device;

void tsl2572_xthresh_uev(int lux) {
    char buf[32];
    char *envp[] = {"ALS=xthreshold", buf, NULL};

    snprintf(buf, 31, "LUX=%d", lux);
    kobject_uevent_env(&als_tsl2572_misc_device.this_device->kobj, KOBJ_CHANGE, envp);
}

static long als_tsl2572_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int __user *argp = (int __user *)arg;
    int ret = 0;
    int lux = 0;
    int count = 0;
    ALS_REGS als_reg;
    int automode;
    struct device *this_dev = als_tsl2572_misc_device.this_device;
    struct tsl2572_chip *chip = dev_get_drvdata(als_tsl2572_misc_device.parent);

    switch (cmd) {
        case ALS_IOCTL_GET_LUX:
            dev_dbg(this_dev, "%s:%d:ALS_IOCTL_GET_LUX", __func__, __LINE__);

            AMS_MUTEX_LOCK(&chip->lock);
            ret = tsl2572_read_als(chip);
            ret |= tsl2572_get_lux(chip);
            lux = chip->als_inf.lux;
            AMS_MUTEX_UNLOCK(&chip->lock);

            if (ret == 0) {
                if (put_user(lux, argp) == 0) {
                    dev_dbg(this_dev, "%s:%d: lux:%d", __func__, __LINE__, lux);
                } else {
                    dev_err(this_dev, "ALS_IOCTL_GET_LUX: put_user FAILED\n");
                    ret = -EFAULT;
                }
            }else{
                dev_err(this_dev, "ALS_IOCTL_GET_LUX: tsl2572_get_lux FAILED\n");
                ret = -EFAULT;
            }
            break;

        case ALS_IOCTL_GET_COUNT:
            dev_dbg(this_dev, "%s:%d:ALS_IOCTL_GET_COUNT", __func__, __LINE__);

            AMS_MUTEX_LOCK(&chip->lock);
            ret = tsl2572_read_als(chip);
            count = chip->als_inf.als_ch0;
            AMS_MUTEX_UNLOCK(&chip->lock);

            if (ret == 0) {
                if (put_user(count, argp) == 0) {
                    dev_dbg(this_dev, "%s:%d: lux:%d", __func__, __LINE__, count);
                } else {
                    dev_err(this_dev, "ALS_IOCTL_GET_COUNT: put_user FAILED\n");
                    ret = -EFAULT;
                }
            } else {
                dev_err(this_dev, "ALS_IOCTL_GET_COUNT: tsl2572_read_als FAILED\n");
                ret = -EFAULT;
            }
            break;

        case ALS_IOCTL_READ_LUX_REGS:
            dev_dbg(this_dev, "%s:%d:ALS_IOCTL_READ_REGS", __func__, __LINE__);

            if (copy_from_user(&als_reg, argp, sizeof(ALS_REGS)) == 0) {
                if (ams_i2c_blk_read(chip->client, TSL2572_REG_C0DATAL, &als_reg.value[0], 4 * sizeof(u8)) >= 0) {
                    if (copy_to_user(argp, &als_reg, sizeof(ALS_REGS)) != 0)	{
                        dev_err(this_dev, "ALS_IOCTL_READ_LUX_REGS: copy_to_user FAILED\n");
                        ret = -EFAULT;
                    }
                } else {
                    dev_err(this_dev, "ALS_IOCTL_READ_LUX_REGS: tsl2572_read_lux_regs FAILED\n");
                    ret = -EFAULT;
                }
            } else {
                dev_err(this_dev, "ALS_IOCTL_READ_LUX_REGS copy_from_user FAILED\n");
                ret = -EFAULT;
            }
            break;

        case ALS_IOCTL_READ_REG:
            dev_dbg(this_dev, "%s:%d:ALS_IOCTL_READ_REG", __func__, __LINE__);
            ret = -EFAULT;

            if (copy_from_user(&als_reg, argp, sizeof(ALS_REGS)) == 0) {
                if (ams_i2c_read(chip->client, als_reg.addr, &als_reg.value[0]) >= 0) {
                    if (copy_to_user(argp, &als_reg, sizeof(ALS_REGS)) == 0) {
                        ret = 0;
                    }
                }
            }
            break;

        case ALS_IOCTL_WRITE_REG:
            dev_dbg(this_dev, "\n%s:%d:ALS_IOCTL_WRITE_REG", __func__, __LINE__);
            ret = -EFAULT;

            if (copy_from_user(&als_reg, argp, sizeof(ALS_REGS)) == 0) {
                if (ams_i2c_write(chip->client, chip->shadow, als_reg.addr, als_reg.value[0]) >= 0) {
                    ret = 0;
                }
            }
            break;

        case ALS_IOCTL_AUTOMODE_EN:
            dev_dbg(this_dev, "%s:%d:ALS_IOCTL_AUTOMODE_EN", __func__, __LINE__);

            if(get_user(automode, argp)) {
                dev_err(this_dev, "%s:%d get_user failed\n", __func__, __LINE__);
                ret = -EFAULT;
                break;
            }

            if (automode) {
                if(tsl2572_configure_als_mode(chip, 1)) {
                    dev_err(this_dev, "%s:%d Could not initialize automode\n", __func__, __LINE__);
                    ret = -EFAULT;
                }
            } else {
                if(tsl2572_configure_als_mode(chip, 0)) {
                    dev_err(this_dev, "%s:%d Could not initialize manual mode\n", __func__, __LINE__);
                    ret = -EFAULT;
                }
            }
            break;

        default:
            dev_err(this_dev, "ALS_IOCTL_CMD: unknown command: %d\n", cmd);
            ret = -EINVAL;
            break;
    }

    return ret;
}

static ssize_t als_tsl2572_misc_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *pos)
{
    return 0;
}

static ssize_t als_tsl2572_misc_read(struct file *file, char __user *buf,
                                size_t count, loff_t *pos)
{
    return 0;
}

static const struct file_operations als_tsl2572_misc_fops =
{
    .owner = THIS_MODULE,
    .read  = als_tsl2572_misc_read,
    .write = als_tsl2572_misc_write,
    .unlocked_ioctl = als_tsl2572_ioctl,
};

static struct miscdevice als_tsl2572_misc_device =
{
    .minor = TSL2572_ALS_DEV_MINOR,
    .name  = ALS_MISC_DEV_NAME,
    .fops  = &als_tsl2572_misc_fops,
};
#endif

static int tsl2572_irq_handler(struct tsl2572_chip *chip)
{
    u8 status;
    int ret;

    AMS_MUTEX_LOCK(&chip->lock);
    ret = ams_i2c_read(chip->client, TSL2572_REG_STATUS,
            &chip->shadow[TSL2572_REG_STATUS]);
    status = chip->shadow[TSL2572_REG_STATUS];

    if (status == 0) {
        AMS_MUTEX_UNLOCK(&chip->lock);
        return 0; /* not our interrupt */
    }

    /* Clear the interrupts we'll process */
    ams_i2c_write_direct(chip->client, TSL2572_REG_AINTCLR, status);

    /*
     * ALS
     */
    if (status & TSL2572_ST_ALS_IRQ &&
        status & TSL2572_ST_ALS_VALID) {
        tsl2572_read_als(chip);
        tsl2572_report_als(chip);
    }

    AMS_MUTEX_UNLOCK(&chip->lock);

    return 1; /* we handled the interrupt */

}

static irqreturn_t tsl2572_irq(int irq, void *handle)
{
    struct tsl2572_chip *chip = handle;
    struct device *dev = &chip->client->dev;
    int ret;

    if (chip->in_suspend) {
        dev_info(dev, "%s: in suspend\n", __func__);
        ret = 0;
        goto bypass;
    }
    ret = tsl2572_irq_handler(chip);

bypass:
    return ret ? IRQ_HANDLED : IRQ_NONE;
}

static int tsl2572_flush_regs(struct tsl2572_chip *chip)
{
    int i;
    int rc;
    u8 reg;

    for (i = 0; i < ARRAY_SIZE(restorable_regs); i++) {
        reg = restorable_regs[i];
        rc = ams_i2c_write(chip->client, chip->shadow, reg,
                chip->shadow[reg]);
        if (rc) {
            dev_err(&chip->client->dev, "%s: err on reg 0x%02x\n",
                    __func__, reg);
            break;
        }
    }

    return rc;
}

static int tsl2572_pltf_power_on(struct tsl2572_chip *chip)
{
    int rc = 0;

    if (chip->pdata->platform_power) {
        rc = chip->pdata->platform_power(&chip->client->dev, POWER_ON);
        mdelay(10);
    }
    chip->unpowered = rc != 0;
    dev_info(&chip->client->dev, "%s: unpowered=%d\n", __func__,
            chip->unpowered);
    return rc;
}

static int tsl2572_pltf_power_off(struct tsl2572_chip *chip)
{
    int rc = 0;

    if (chip->pdata->platform_power) {
        rc = chip->pdata->platform_power(&chip->client->dev, POWER_OFF);
        chip->unpowered = rc == 0;
    } else {
        chip->unpowered = false;
    }
    dev_info(&chip->client->dev, "%s: unpowered=%d\n", __func__,
            chip->unpowered);
    return rc;
}

static void tsl2572_set_defaults(struct tsl2572_chip *chip)
{
    u8 *sh = chip->shadow;
    struct device *dev = &chip->client->dev;

    /* Clear the register shadow area */
    memset(chip->shadow, 0x00, sizeof(chip->shadow));

    /* If there is platform data use it */
    if (chip->pdata) {
        dev_info(dev, "%s: Loading pltform data\n", __func__);
        chip->params.persist = chip->pdata->parameters.persist;
        chip->params.als_gain = chip->pdata->parameters.als_gain;
        chip->params.als_gain_auto = chip->pdata->parameters.als_gain_auto;
        chip->params.als_deltap = chip->pdata->parameters.als_deltap;
        chip->params.als_time = chip->pdata->parameters.als_time;
        chip->params.d_factor = chip->pdata->parameters.d_factor;
        chip->params.lux_segment[0].ch0_coef =
            chip->pdata->parameters.lux_segment[0].ch0_coef;
        chip->params.lux_segment[0].ch1_coef =
            chip->pdata->parameters.lux_segment[0].ch1_coef;
        chip->params.lux_segment[1].ch0_coef =
            chip->pdata->parameters.lux_segment[1].ch0_coef;
        chip->params.lux_segment[1].ch1_coef =
            chip->pdata->parameters.lux_segment[1].ch1_coef;
    } else {
        dev_info(dev, "%s: use defaults\n", __func__);
        chip->params.persist = 2;
        chip->params.als_gain = AGAIN_1;
        chip->params.als_deltap = 10;
        chip->params.als_time = AW_TIME_MS(100);
        chip->params.d_factor = 60;
        chip->params.lux_segment[0].ch0_coef = 1000;
        chip->params.lux_segment[0].ch1_coef = -1870;
        chip->params.lux_segment[1].ch0_coef = 630;
        chip->params.lux_segment[1].ch1_coef = -1000;
        chip->params.als_gain_auto = 1;
    }

    /* Copy the default values into the register shadow area */
    if (!chip->params.als_gain_auto)
        sh[TSL2572_REG_CTRL] = chip->params.als_gain;
    sh[TSL2572_REG_ATIME] = chip->params.als_time;
    tsl2572_flush_regs(chip);
}

static int tsl2572_add_sysfs_interfaces(struct device *dev,
        struct device_attribute *a, int size) {
    int i;

    for (i = 0; i < size; i++)
        if (device_create_file(dev, a + i))
            goto undo;
    return 0;
undo:
    for (; i >= 0; i--)
        device_remove_file(dev, a + i);
    dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
    return -ENODEV;
}

static void tsl2572_remove_sysfs_interfaces(struct device *dev,
        struct device_attribute *a, int size)
{
    int i;

    for (i = 0; i < size; i++)
        device_remove_file(dev, a + i);
}

static int tsl2572_get_id(struct tsl2572_chip *chip, u8 *id)
{
    ams_i2c_read(chip->client, TSL2572_REG_ID, id);
    return 0;
}

static int tsl2572_power_on(struct tsl2572_chip *chip)
{
    int rc;

    rc = tsl2572_pltf_power_on(chip);
    if (rc)
        return rc;
    dev_info(&chip->client->dev, "%s: chip was off, restoring regs\n",
            __func__);
    return tsl2572_flush_regs(chip);
}

static int tsl2572_als_idev_open(struct input_dev *idev)
{
    struct tsl2572_chip *chip = dev_get_drvdata(&idev->dev);
    int rc = 0;

    dev_info(&idev->dev, "%s\n", __func__);
    AMS_MUTEX_LOCK(&chip->lock);
    if (chip->unpowered) {
        rc = tsl2572_power_on(chip);
        if (rc)
            goto chip_on_err;
    }
    rc = tsl2572_configure_als_mode(chip, 1);
    if (rc)
        tsl2572_pltf_power_off(chip);
chip_on_err:
    AMS_MUTEX_UNLOCK(&chip->lock);
    return 0;
}

static void tsl2572_als_idev_close(struct input_dev *idev)
{
    struct tsl2572_chip *chip = dev_get_drvdata(&idev->dev);

    dev_info(&idev->dev, "%s\n", __func__);
    AMS_MUTEX_LOCK(&chip->lock);
    tsl2572_configure_als_mode(chip, 0);
    tsl2572_pltf_power_off(chip);
    AMS_MUTEX_UNLOCK(&chip->lock);
}

#ifdef CONFIG_QUALCOMM_AP
static int tsl2572_als_set_enable(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
    struct tsl2572_chip *chip = container_of(sensors_cdev, struct tsl2572_chip, als_cdev);

    if (enable) {
        chip->a_idev->open(chip->a_idev);
    } else {
        chip->a_idev->close(chip->a_idev);
    }
    return 0;
}
#endif

#ifdef CONFIG_OF
static int tsl2572_init_dt(struct tsl2572_i2c_platform_data *pdata)
{
    struct device_node *np = pdata->of_node;
    const char *str;
    u32 val;

    if (!pdata->of_node)
        return 0;

    if (!of_property_read_string(np, "als_name", &str))
        pdata->als_name = str;

    if (!of_property_read_u32(np, "persist", &val))
        pdata->parameters.persist = val;

    //Stored as Q1 fixed point
    if (!of_property_read_u32(np, "als_gain", &val))
        pdata->parameters.als_gain = val;

    if (!of_property_read_u32(np, "als_auto_gain", &val))
        pdata->parameters.als_gain_auto = val;

    if (!of_property_read_u32(np, "als_deltap", &val))
        pdata->parameters.als_deltap = val;

    if (!of_property_read_u32(np, "als_time", &val))
        pdata->parameters.als_time = val;

    if (!of_property_read_u32(np, "d_factor", &val))
        pdata->parameters.d_factor = val;

    if (!of_property_read_u32(np, "ch0_coef0", &val))
        pdata->parameters.lux_segment[0].ch0_coef = val;

    if (!of_property_read_u32(np, "ch1_coef0", &val))
        pdata->parameters.lux_segment[0].ch1_coef = val;

    if (!of_property_read_u32(np, "ch0_coef1", &val))
        pdata->parameters.lux_segment[1].ch0_coef = val;

    if (!of_property_read_u32(np, "ch1_coef1", &val))
        pdata->parameters.lux_segment[1].ch1_coef = val;

    if (!of_property_read_u32(np, "als_can_wake", &val))
        pdata->als_can_wake = (val == 0) ? false : true;

    return 0;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id tsl2572_i2c_dt_ids[] = {
    {.compatible = "ams,tsl2572"},
    {}
};
MODULE_DEVICE_TABLE(of, tsl2572_i2c_dt_ids);
#endif

static int tsl2572_probe(struct i2c_client *client,
        const struct i2c_device_id *idp) {
    int i, ret;
    u8 id;
    struct device *dev = &client->dev;
    static struct tsl2572_chip *chip;
    struct tsl2572_i2c_platform_data *pdata = dev->platform_data;
    static unsigned int irq_number;
    unsigned long default_irq_trigger = 0;
    bool powered = 0;

    pr_info("\nTSL2572: probe()\n");

#ifdef CONFIG_OF
    if (!pdata) {
        pdata = kzalloc(sizeof(struct tsl2572_i2c_platform_data),
                GFP_KERNEL);
        if (!pdata)
            return -ENOMEM;

        if (of_match_device(tsl2572_i2c_dt_ids, &client->dev)) {
            pdata->of_node = client->dev.of_node;
            ret = tsl2572_init_dt(pdata);
            if (ret)
                return ret;
        }
    }
#endif

    /*
     * Validate bus and device registration
     */

    dev_info(dev, "%s: client->irq = %d\n", __func__, client->irq);
    if (!i2c_check_functionality(client->adapter,
                I2C_FUNC_SMBUS_BYTE_DATA)) {
        dev_err(dev, "%s: i2c smbus byte data unsupported\n", __func__);
        ret = -EOPNOTSUPP;
        goto init_failed;
    }
    if (!pdata) {
        dev_err(dev, "%s: platform data required\n", __func__);
        ret = -EINVAL;
        goto init_failed;
    }

    if (!(pdata->als_name) || client->irq < 0) {
        dev_err(dev, "%s: no reason to run.\n", __func__);
        ret = -EINVAL;
        goto init_failed;
    }

    if (pdata->platform_init) {
        ret = pdata->platform_init();
        if (ret)
            goto init_failed;
    }
    if (pdata->platform_power) {
        ret = pdata->platform_power(dev, POWER_ON);
        if (ret) {
            dev_err(dev, "%s: pltf power on failed\n", __func__);
            goto pon_failed;
        }
        powered = true;
        mdelay(10);
    }

    chip = kzalloc(sizeof(struct tsl2572_chip), GFP_KERNEL);
    if (!chip) {
        ret = -ENOMEM;
        goto malloc_failed;
    }

    mutex_init(&chip->lock);
    chip->client = client;
    chip->pdata = pdata;
    i2c_set_clientdata(client, chip);

    /*
     * Validate the appropriate ams device is available for this driver
     */

    ret = tsl2572_get_id(chip, &id);

    dev_info(dev, "%s: device id: %02x \n", __func__, id);

    for (i = 0; i < ARRAY_SIZE(tsl2572_ids); i++) {
        if (id == (tsl2572_ids[i]))
            break;
    }

    if (i < ARRAY_SIZE(tsl2572_names)) {
        dev_info(dev, "%s: '%s ID. 0x%x' detected\n", __func__,
                tsl2572_names[i], id);
        chip->device_index = i;
    } else {
        dev_err(dev, "%s: not supported chip id\n", __func__);
        ret = -EOPNOTSUPP;
        goto id_failed;
    }
    /*
     * Set chip defaults
     */
    tsl2572_set_defaults(chip);
    if (pdata->platform_power) {
        pdata->platform_power(dev, POWER_OFF);
        powered = false;
        chip->unpowered = true;
    }
    /*
     * Initialize ALS
     */
    if (!pdata->als_name)
        goto bypass_als_idev;
	chip->wake_irq = pdata->als_can_wake;
    chip->a_idev = input_allocate_device();
    if (!chip->a_idev) {
        dev_err(dev, "%s: no memory for input_dev '%s'\n", __func__,
                pdata->als_name);
        ret = -ENODEV;
        goto input_a_alloc_failed;
    }
    chip->a_idev->name = pdata->als_name;
    chip->a_idev->id.bustype = BUS_I2C;
    set_bit(EV_ABS, chip->a_idev->evbit);
    set_bit(ABS_MISC, chip->a_idev->absbit);
    input_set_abs_params(chip->a_idev, ABS_MISC, 0, 65535, 0, 0);
    chip->a_idev->open = tsl2572_als_idev_open;
    chip->a_idev->close = tsl2572_als_idev_close;
    dev_set_drvdata(&chip->a_idev->dev, chip);

    ret = input_register_device(chip->a_idev);
    if (ret) {
        input_free_device(chip->a_idev);
        dev_err(dev, "%s: cant register input '%s'\n", __func__,
                pdata->als_name);
        goto input_a_alloc_failed;
    }
    ret = tsl2572_add_sysfs_interfaces(&chip->a_idev->dev,
            tsl2572_als_attrs, tsl2572_als_attrs_size);
    if (ret)
        goto input_a_sysfs_failed;
#ifdef CONFIG_QUALCOMM_AP
    chip->als_cdev = als_sensors_cdev;
    chip->als_cdev.sensors_enable = tsl2572_als_set_enable;
    if (sensors_classdev_register(&chip->a_idev->dev, &chip->als_cdev)) {
        dev_err(dev, "sensors class register failed.\n");
    }
#endif

#ifdef CONFIG_LAB126
    if (misc_register(&als_tsl2572_misc_device)) {
        dev_err(dev, "%s Couldn't register device %d \n",__func__, TSL2572_ALS_DEV_MINOR);
        ret = -EBUSY;
        goto irq_register_fail;
    }
    als_tsl2572_misc_device.parent = dev;
#endif

bypass_als_idev:
    /* Initialize IRQ & Handler */

    /* If this is a DTS build the following
     * variable will be overwritten.
     */
    irq_number = client->irq;

    default_irq_trigger =
    irqd_get_trigger_type(irq_get_irq_data(client->irq));
    ret = devm_request_threaded_irq(dev, client->irq,
            NULL, &tsl2572_irq,
            default_irq_trigger |
            IRQF_SHARED         |
            IRQF_ONESHOT,
            dev_name(dev), chip);
	if (ret) {
	    dev_err(dev, "Failed to request irq %d\n", client->irq);
#ifdef CONFIG_LAB126
	    goto misc_register_fail;
#else
	    goto irq_register_fail;
#endif
	}

	/* Power up device */
	ams_i2c_write(chip->client, chip->shadow, TSL2572_REG_ENABLE, 0x01);

	dev_info(dev, "Probe ok.\n");

	return 0;

	/*
	 * This must be unwound in the correct order, reverse
	 * from initialization above
	 */
#ifdef CONFIG_LAB126
misc_register_fail:
	misc_deregister(&als_tsl2572_misc_device);
#endif
irq_register_fail:
	if (chip->a_idev)
	    tsl2572_remove_sysfs_interfaces(&chip->a_idev->dev,
	            tsl2572_als_attrs, tsl2572_als_attrs_size);
input_a_sysfs_failed:
	if (chip->a_idev)
	    input_unregister_device(chip->a_idev);

input_a_alloc_failed:
	/*
	 * Exit points for general device initialization failures
	 */

id_failed:
	i2c_set_clientdata(client, NULL);
malloc_failed:
	if (powered && pdata->platform_power)
	    pdata->platform_power(dev, POWER_OFF);
pon_failed:
	if (pdata->platform_teardown)
	    pdata->platform_teardown(dev);
init_failed:
	kfree(pdata);
	kfree(chip);
	dev_err(dev, "Probe failed.\n");
	return ret;
}

static int tsl2572_suspend(struct device *dev)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    pr_info("\nTSL2572: suspend()\n");
    AMS_MUTEX_LOCK(&chip->lock);
    chip->in_suspend = 1;

    if (chip->wake_irq) {
        irq_set_irq_wake(chip->client->irq, 1);
    } else if (!chip->unpowered) {
        dev_info(dev, "powering off\n");
        if (chip->als_enabled)
            ams_i2c_modify(chip->client, chip->shadow, TSL2572_REG_ENABLE, TSL2572_PON, 0);
        tsl2572_pltf_power_off(chip);
    }
    AMS_MUTEX_UNLOCK(&chip->lock);

    return 0;
}

static int tsl2572_resume(struct device *dev)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    pr_info("\nTSL2572: resume()\n");
    AMS_MUTEX_LOCK(&chip->lock);
    chip->in_suspend = 0;

    dev_info(dev, "%s: powerd %d, als: enabled %d", __func__,
            !chip->unpowered, chip->als_enabled);

    if (chip->wake_irq) {
        irq_set_irq_wake(chip->client->irq, 0);
    } else if (chip->unpowered) {
        dev_info(dev, "powering on\n");
        tsl2572_pltf_power_on(chip);
        if (chip->als_enabled)
            ams_i2c_modify(chip->client, chip->shadow, TSL2572_REG_ENABLE, TSL2572_PON, 1);
    }
    AMS_MUTEX_UNLOCK(&chip->lock);

    return 0;
}

static int tsl2572_remove(struct i2c_client *client)
{
    struct tsl2572_chip *chip = i2c_get_clientdata(client);

    dev_info(&client->dev, "%s\n", __func__);
    devm_free_irq(&client->dev, client->irq, chip);
    tsl2572_configure_als_mode(chip, 0);
    tsl2572_pltf_power_off(chip);
    if (chip->a_idev) {
        tsl2572_remove_sysfs_interfaces(&chip->a_idev->dev,
                tsl2572_als_attrs, tsl2572_als_attrs_size);
        input_unregister_device(chip->a_idev);
    }
#ifdef CONFIG_LAB126
    misc_deregister(&als_tsl2572_misc_device);
#endif
#ifdef CONFIG_QUALCOMM_AP
    sensors_classdev_unregister(&chip->als_cdev);
#endif
    if (chip->pdata->platform_teardown)
        chip->pdata->platform_teardown(&client->dev);
    i2c_set_clientdata(client, NULL);
#ifdef CONFIG_OF
    kfree(chip->pdata);
#endif
    kfree(chip);
    return 0;
}

static struct i2c_device_id tsl2572_idtable[] = {{"tsl2572", 0}, {} };
MODULE_DEVICE_TABLE(i2c, tsl2572_idtable);

static const struct dev_pm_ops tsl2572_pm_ops = {.suspend = tsl2572_suspend,
    .resume = tsl2572_resume,};

static struct i2c_driver tsl2572_driver = {
    .driver = {.name = "tsl2572",
        .pm = &tsl2572_pm_ops,},
    .id_table = tsl2572_idtable,
    .probe = tsl2572_probe,
    .remove = tsl2572_remove,};

module_i2c_driver(tsl2572_driver);

MODULE_DESCRIPTION("AMS-TAOS tsl2572 ALS sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.2");

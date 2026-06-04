/*
 * HDMI support
 *
 * Copyright (C) 2013 ITE Tech. Inc.
 * Author: Hermes Wu <hermes.wu@ite.com.tw>
 *
 * HDMI TX driver for IT66121
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/* #include "hdmitx.h" */
#ifdef SUPPORT_CEC
#include "hdmitx_cec.h"
#endif
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>

#include "debug_hdmi.h"
#include "hdmitx_drv.h"
#include "hdmitx_sys.h"
#include "itx_typedef.h"
#include "inc/hdmi_drv.h"
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
/* #include <linux/earlysuspend.h> */
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
/* #include <linux/rtpm_prio.h> */
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/types.h>
/*#include <mt-plat/mt_gpio.h>*/

#include "hdmitx.h"
#include "IT662X/IT662X_drv.h"
/*#include "IT662X/IT662X_typedef.h"*/

/* #include <cust_eint.h> */
/* #include "cust_gpio_usage.h" */
/* #include "mach/eint.h" */
/* #include "mach/irqs.h" */

/* #include <mach/devs.h> */
/* #include <mach/mt_typedefs.h> */
/* #include <mach/mt_gpio.h> */
/* #include <mach/mt_pm_ldo.h> */

unsigned char real_edid[256];

unsigned char timer_on = 1;
unsigned char IT66121_LOG_on;

/*#define ITE66121_DRM_SUPPORT*/
#ifdef ITE66121_DRM_SUPPORT
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_edid.h>

struct it66121 {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct drm_bridge bridge;
	struct drm_connector connector;
	struct gpio_desc *reset_gpio;
};

#endif

#define FALLING_EDGE_TRIGGER

#define MSCOUNT 1000
#define LOADING_UPDATE_TIMEOUT (3000/32)	/* 3sec */
/* unsigned short u8msTimer = 0 ; */
/* unsigned short TimerServF = TRUE ; */

/* //////////////////////////////////////////////////////////////////// */
/* Authentication status */
/* //////////////////////////////////////////////////////////////////// */

/* #define TIMEOUT_WAIT_AUTH MS(2000) */

/* I2C Relate Definitions */
static struct i2c_client *it66121_i2c_client;
static struct i2c_client *it6620_basic_client;
static struct i2c_client *it6620_cec_client;
static struct i2c_client *it6620_cap_client;

static struct timer_list r_hdmi_timer;
static struct timer_list r_it6620_timer;

#ifdef PCADR
#define IT66121_plus 0x02
/* Define it66121's I2c slave Address . */
#else
#define IT66121_plus 0x00
/* Define it66121's I2c Address of all pages by the status of PCADR pin. */
#endif

/* I2C address */
#define _80MHz 80000000
#define HDMI_TX_I2C_SLAVE_ADDR 0x98
#define CEC_I2C_SLAVE_ADDR 0x9C

/*I2C Device name */
#define DEVICE_NAME "it66121"

#define MAX_TRANSACTION_LENGTH 8


static int hdmi_ite_probe(struct i2c_client *client,
	const struct i2c_device_id *id);

struct HDMI_UTIL_FUNCS hdmi_util = { 0 };
unsigned char hdmi_powerenable = 0xff;

static struct pinctrl *hdmi_pinctrl;
static struct pinctrl_state *pins_hdmi_func;
static struct pinctrl_state *pins_hdmi_gpio;

static const struct i2c_device_id hdmi_ite_id[] = {
	{DEVICE_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, hdmi_ite_id);

static const struct of_device_id hdmi_ite_of_match[] = {
	{.compatible = "ite,it66121-i2c"},
	{.compatible = "ite,it6620-basic-i2c"},
	{.compatible = "ite,it6620-cec-i2c"},
	{.compatible = "ite,it6620-cap-i2c"},
	{},
};
MODULE_DEVICE_TABLE(of, hdmi_ite_of_match);

static struct i2c_driver hdmi_ite_i2c_driver = {
	.probe = hdmi_ite_probe,
	.remove = NULL,
	.driver = {	.name = DEVICE_NAME,
				.of_match_table = hdmi_ite_of_match,
				.owner = THIS_MODULE,
	},
	.id_table = hdmi_ite_id,
};




/* static struct it66121_i2c_data *obj_i2c_data = NULL; */

/*Declare and definition for a hdmi kthread, */
/*this thread is used to check the HDMI Status */
static struct task_struct *hdmi_timer_task;
wait_queue_head_t hdmi_timer_wq;
atomic_t hdmi_timer_event = ATOMIC_INIT(0);

/* For it6620 earc int status check*/
static struct task_struct *it6620_timer_task;
wait_queue_head_t it6620_timer_wq;
atomic_t it6620_timer_event = ATOMIC_INIT(0);

#if 0
static int match_id(const struct i2c_device_id *id,
const struct i2c_client *client)
{
	if (strcmp(client->name, id->name) == 0)
		return true;

	return false;
}
#endif

void HDMI_reset(void)
{

	struct device_node *dn;
	int bus_switch_pin;

	IT66121_LOG("hdmi_ite66121 %s\n", __func__);

	IT66121_LOG(">>HDMI_Reset\n");

#if defined(GPIO_HDMI_9024_RESET)

	IT66121_LOG(">>Pull Down Reset Pin\n");
	mt_set_gpio_mode(GPIO_HDMI_9024_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_9024_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_9024_RESET, GPIO_OUT_ZERO);

	msleep(100);


	mt_set_gpio_mode(GPIO_HDMI_9024_RESET, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO_HDMI_9024_RESET, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_HDMI_9024_RESET, GPIO_OUT_ONE);

	IT66121_LOG("<<Pull Up Reset Pin\n");
#else

	dn = of_find_compatible_node(NULL, NULL, "mediatek,it66121-hdmitx");
	if (dn == NULL)
		IT66121_LOG("dn == NULL");
	bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
	gpio_direction_output(bus_switch_pin, 0);

	msleep(20);

	gpio_direction_output(bus_switch_pin, 1);
#endif
	IT66121_LOG("<<HDMI_Reset\n");
}

/*I2c functin for it6620 start*/
int it6620_i2c_read_byte(u8 Addr, u8 u8Offset, u8 *data)
{
	u8 buf;
	int ret = 0;
	struct i2c_client *client = it6620_basic_client;

	if (Addr == 0x9C)       /* #define eARCRxAddr      0x9C */
		client = it6620_basic_client;
	else if (Addr == 0xCA)     /* #define CEC_ADR		(0xCA)*/
		client = it6620_cec_client;
	else if (Addr == 0xc6)   /* #define RxCapAddr         0xc6*/
		client = it6620_cap_client;
	else {
		IT66121_LOG("I2C read Addr is error\n");
		return -EFAULT;
	}

	buf = u8Offset;
	ret = i2c_master_send(client, (const char *)&buf, 1);
	if (ret < 0) {
		IT66121_LOG("send command error!!\n");
		return -EFAULT;
	}
	ret = i2c_master_recv(client, (char *)&buf, 1);
	if (ret < 0) {
		IT66121_LOG("reads data error!!\n");
		return -EFAULT;
	}
#if defined(HDMI_I2C_DEBUG)
	else
		IT66121_LOG("%s(0x%02X) = %02X\n", __func__, addr, buf);
#endif
	*data = buf;
	return 0;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it6620_i2c_read_byte);
/*----------------------------------------------------------------------------*/

int it6620_i2c_write_byte(u8 Addr, u8 u8Offset, u8 data)
{
	struct i2c_client *client = it6620_basic_client;
	u8 buf[] = { u8Offset, data };
	int ret = 0;

	if (Addr == 0x9C)       /* #define eARCRxAddr      0x9C */
		client = it6620_basic_client;
	else if (Addr == 0xCA)     /* #define CEC_ADR		(0xCA)*/
		client = it6620_cec_client;
	else if (Addr == 0xc6)   /* #define RxCapAddr         0xc6*/
		client = it6620_cap_client;
	else {
		IT66121_LOG("I2C write Addr is error\n");
		return -EFAULT;
	}

	ret = i2c_master_send(client, (const char *)buf, sizeof(buf));
	if (ret < 0) {
		IT66121_LOG("send command error!!\n");
		return -EFAULT;
	}
#if defined(HDMI_I2C_DEBUG)
	else
		IT66121_LOG("%s(0x%02X)= %02X\n", __func__, u8Offset, data);
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it6620_i2c_write_byte);
/*----------------------------------------------------------------------------*/


/*I2c functin for it6620 end*/


int it66121_i2c_read_byte(u8 addr, u8 *data)
{
	u8 buf;
	int ret = 0;
	struct i2c_client *client = it66121_i2c_client;

	if (hdmi_powerenable == 1) {
		buf = addr;
		ret = i2c_master_send(client, (const char *)&buf, 1);
		if (ret < 0) {
			IT66121_LOG("send command error!!\n");
			return -EFAULT;
		}
		ret = i2c_master_recv(client, (char *)&buf, 1);
		if (ret < 0) {
			IT66121_LOG("reads data error!!\n");
			return -EFAULT;
		}
#if defined(HDMI_I2C_DEBUG)
		else
			IT66121_LOG("%s(0x%02X) = %02X\n", __func__, addr, buf);
#endif
		*data = buf;
		return 0;
	} else {
		return 0;
	}
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it66121_i2c_read_byte);
/*----------------------------------------------------------------------------*/

int it66121_i2c_write_byte(u8 addr, u8 data)
{
	struct i2c_client *client = it66121_i2c_client;
	u8 buf[] = { addr, data };
	int ret = 0;

	if (hdmi_powerenable == 1) {
		ret = i2c_master_send(client, (const char *)buf, sizeof(buf));
		if (ret < 0) {
			IT66121_LOG("send command error!!\n");
			return -EFAULT;
		}
#if defined(HDMI_I2C_DEBUG)
		else
			IT66121_LOG("%s(0x%02X)= %02X\n", __func__, addr, data);
#endif
		return 0;
	} else {
		return 0;
	}
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it66121_i2c_write_byte);
/*----------------------------------------------------------------------------*/

int it66121_i2c_read_block(u8 addr, u8 *data, int len)
{
	struct i2c_client *client = it66121_i2c_client;
	u8 beg = addr;
	struct i2c_msg msgs[2] = {
		{
		 .addr = client->addr, .flags = 0,
		 .len = 1, .buf = &beg},
		{
		 .addr = client->addr, .flags = I2C_M_RD,
		 .len = len, .buf = data,
		}
	};
	int err;

	if (len == 1)
		return it66121_i2c_read_byte(addr, data);

	if (!client) {
		return -EINVAL;
	} else if (len > MAX_TRANSACTION_LENGTH) {
		IT66121_LOG(" length %d exceeds %d\n",
			len, MAX_TRANSACTION_LENGTH);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		IT66121_LOG("i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;	/*no error */
	}
	return err;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it66121_i2c_read_block);
/*----------------------------------------------------------------------------*/

int it66121_i2c_write_block(u8 addr, u8 *data, int len)
{
	/*because address also occupies one byte,*/
	/* the maximum length for write is 7 bytes */
	int err, idx, num;
	char buf[MAX_TRANSACTION_LENGTH];
	struct i2c_client *client = it66121_i2c_client;

	if (!client) {
		return -EINVAL;
	} else if (len >= MAX_TRANSACTION_LENGTH) {
		IT66121_LOG(" length %d exceeds %d\n",
			len, MAX_TRANSACTION_LENGTH);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		IT66121_LOG("send command error!!\n");
		return -EFAULT;
	}

	err = 0;	/*no error */

	return err;
}

/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(it66121_i2c_write_block);
/*----------------------------------------------------------------------------*/

/* /it66121 power on */
/* Description: */
/*  */
int it66121_power_on(void)
{
	IT66121_LOG(">>> %s\n", __func__);

	if (hdmi_powerenable == 1) {
		IT66121_LOG("[hdmi]already power on, return\n");
		return 0;
	}
	hdmi_powerenable = 1;
	/*ite66121_pmic_power_on();*/

	/********This leave for mt6592 to power on it66121 ************/
	/* To Do */
	/* Reset The it66121 IC */
	/*HDMI_reset();*/
	/*msleep(100);*/
	/* This leave for it66121 internal init function */
	InitHDMITX_Variable();
	InitHDMITX();
	HDMITX_ChangeDisplayOption(HDMI_720p60, HDMI_RGB444);
	add_timer(&r_hdmi_timer);

	/* Enable Interrupt of it66121 */
#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	mt_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif


	IT66121_LOG("<<< %s,\n", __func__);
	return 0;
}

enum HDMI_STATE it66121_get_state(void)
{
	IT66121_LOG(">>> %s,\n", __func__);

	if (HPDStatus)
		return HDMI_STATE_ACTIVE;
	else
		return HDMI_STATE_NO_DEVICE;

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);
}


void it66121_log_enable(bool enable)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);
}


void it66121_power_down(void)
{
	IT66121_LOG(">>> %s,\n", __func__);

	if (hdmi_powerenable == 0) {
		IT66121_LOG("[hdmi]already power off, return\n");
		return;
	}
	hdmi_powerenable = 0;

	del_timer(&r_hdmi_timer);

	/* leave for it66121 internal power down */
	/*ite66121_pmic_power_off();*/

	it66121_FUNC();
	/* HDMITX_DisableVideoOutput(); */
	/* HDMITX_PowerDown(); */


	/* Leave for mt6592 to power down it66121 */

	IT66121_LOG("<<< %s,\n", __func__);
}
#if 0
static void _it66121_irq_handler(void)
{
	IT66121_LOG("it66121 irq\n");
	#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
	mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);
    #endif

	/*Disable IT66121 HPD*/
	/*it66121_i2c_write_byte(0x09,0x01);*/

	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);
}
#endif
/* Just For Test */
void HDMITX_DevLoopProc_Test(void)
{

	IT66121_LOG(">> HDMITX_DevLoopProc_Test\n");
}

static int it6620_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 93 };
	/* RTPM_PRIO_SCRN_UPDATE */
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(it6620_timer_wq,
			atomic_read(&it6620_timer_event));
		atomic_set(&it6620_timer_event, 0);

		if (timer_on == 1)
			IT662x_eARC_Main();

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int hdmi_timer_kthread(void *data)
{
	struct sched_param param = {.sched_priority = 94 };
	/* RTPM_PRIO_SCRN_UPDATE */
	sched_setscheduler(current, SCHED_RR, &param);

	for (;;) {
		wait_event_interruptible(hdmi_timer_wq,
			atomic_read(&hdmi_timer_event));
		atomic_set(&hdmi_timer_event, 0);
		/* HDMITX_DevLoopProc_Test(); */
		if (hdmi_powerenable == 1)
			HDMITX_DevLoopProc();

#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)
		mt_eint_unmask(CUST_EINT_EINT_HDMI_HPD_NUM);
#endif
		HDMITX_WriteI2C_Byte(REG_TX_INT_MASK1, 0x03);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

void it66121_dump(void)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);
}

static int it66121_audio_enable(bool enable)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);

	return 0;
}


static int it66121_video_enable(bool enable)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* Leave for it66121  */
	IT66121_LOG("<<< %s,\n", __func__);

	return 0;
}


static int it66121_audio_config(enum HDMI_AUDIO_FORMAT aformat, int bitWidth)
{
	IT66121_LOG(">>> %s,\n", __func__);
	/* Leave for it66121  */
	dump_stack();
	IT66121_LOG("<<< %s,\n", __func__);

	return 0;
}



int it66121_video_config(enum HDMI_VIDEO_RESOLUTION vformat,
	enum HDMI_VIDEO_INPUT_FORMAT vin, int vout)
{

	HDMI_Video_Type it66121_video_type = HDMI_480i60_16x9;


	IT66121_LOG(">>> %s vformat:0x%x,\n", __func__, vformat);

	if (vformat == HDMI_VIDEO_720x480p_60Hz)
		it66121_video_type = HDMI_480p60;
	else if (vformat == HDMI_VIDEO_1280x720p_60Hz)
		it66121_video_type = HDMI_720p60;
	else if (vformat == HDMI_VIDEO_1920x1080p_30Hz)
		it66121_video_type = HDMI_1080p30;
	else {
		IT66121_LOG("error:it66121_video_config vformat=%d\n", vformat);
		it66121_video_type = HDMI_720p60;
	}

	HDMITX_ChangeDisplayOption(it66121_video_type, HDMI_RGB444);
	HDMITX_SetOutput();

	IT66121_LOG("<<< %s,\n", __func__);

	return 0;
}

static void it66121_suspend(void)
{

	IT66121_LOG(">>> %s,\n", __func__);
	/* leave for mt6592 operation */

	/*leave for it66121 operation */

	IT66121_LOG("<<< %s,\n", __func__);
}

static void it66121_resume(void)
{
	IT66121_LOG(">>> %s,\n", __func__);

	/* leave for mt6592 operation */

	/*leave for it66121 operation */

	IT66121_LOG("<<< %s,\n", __func__);
}


static void it66121_get_params(struct HDMI_PARAMS *params)
{
	enum HDMI_VIDEO_RESOLUTION input_resolution;

	input_resolution = params->init_config.vformat - 2;
	memset(params, 0, sizeof(struct HDMI_PARAMS));

	IT66121_LOG("it66121_get_params res = %d\n", input_resolution);

	switch (input_resolution) {
	case HDMI_VIDEO_720x480p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_RISING;
		params->vsync_pol = HDMI_POLARITY_RISING;
		params->hsync_pulse_width = 62;
		params->hsync_back_porch  = 60;
		params->hsync_front_porch = 16;
		params->vsync_pulse_width = 6;
		params->vsync_back_porch  = 30;
		params->vsync_front_porch = 9;
		params->width = 720;
		params->height = 480;
		params->input_clock = HDMI_VIDEO_720x480p_60Hz;
		params->init_config.vformat = HDMI_VIDEO_720x480p_60Hz;
		break;
	case HDMI_VIDEO_1280x720p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 40;
		params->hsync_back_porch  = 220;
		params->hsync_front_porch = 110;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch  = 20;
		params->vsync_front_porch = 5;
		params->width = 1280;
		params->height = 720;
		params->input_clock = HDMI_VIDEO_1280x720p_60Hz;
		params->init_config.vformat = HDMI_VIDEO_1280x720p_60Hz;
		break;
	case HDMI_VIDEO_1920x1080p_30Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 44;
		params->hsync_back_porch  = 148;
		params->hsync_front_porch = 88;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch  = 36;
		params->vsync_front_porch = 4;
		params->width = 1920;
		params->height = 1080;
		params->input_clock = HDMI_VIDEO_1920x1080p_30Hz;
		params->init_config.vformat = HDMI_VIDEO_1920x1080p_30Hz;
		break;
	case HDMI_VIDEO_1920x1080p_60Hz:
		params->clk_pol = HDMI_POLARITY_FALLING;
		params->de_pol = HDMI_POLARITY_RISING;
		params->hsync_pol = HDMI_POLARITY_FALLING;
		params->vsync_pol = HDMI_POLARITY_FALLING;
		params->hsync_pulse_width = 44;
		params->hsync_back_porch  = 148;
		params->hsync_front_porch = 88;
		params->vsync_pulse_width = 5;
		params->vsync_back_porch  = 36;
		params->vsync_front_porch = 4;
		params->width = 1920;
		params->height = 1080;
		params->input_clock = HDMI_VIDEO_1920x1080p_60Hz;
		params->init_config.vformat = HDMI_VIDEO_1920x1080p_60Hz;
		break;
	default:
		IT66121_LOG("Unknown support resolution\n");
		break;
	}


	params->init_config.aformat = HDMI_AUDIO_PCM_16bit_48000;
	params->rgb_order = HDMI_COLOR_ORDER_RGB;
	params->io_driving_current = IO_DRIVING_CURRENT_2MA;
	params->intermediat_buffer_num = 4;
	params->output_mode = HDMI_OUTPUT_MODE_LCD_MIRROR;
	params->is_force_awake = 1;
	params->is_force_landscape = 1;
}


static void it66121_set_util_funcs(const struct HDMI_UTIL_FUNCS *util)
{
	memcpy(&hdmi_util, util, sizeof(struct HDMI_UTIL_FUNCS));
}


static int it66121_init(void)
{
	int ret = 0;

	IT66121_LOG(">>> %s,\n", __func__);

	/* HDMI_reset(); */


/* This leave for MT6592 initialize it66121 */
/* register i2c device */



/* This leave for MT6592 internal initialization */

	IT66121_LOG("<<< %s,\n", __func__);
	return ret;
}


#if 0
static int hdmi_i2c_probe(struct i2c_client *client,
const struct i2c_device_id *id)
{
	int err = 0, ret = -1;
	u8 ids[4] = { 0 };
	struct it66121_i2c_data *obj;

	IT66121_LOG("MediaTek HDMI i2c probe\n");

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (obj == NULL) {
		ret = -ENOMEM;
		IT66121_LOG(DEVICE_NAME ": Allocate ts memory fail\n");
		return ret;
	}
	obj_i2c_data = obj;
	obj->client = client;
	it66121_i2c_client = obj->client;
	i2c_set_clientdata(client, obj);


	/* check if chip exist */
	it66121_i2c_read_byte(0x0, &ids[0]);
	it66121_i2c_read_byte(0x1, &ids[1]);
	it66121_i2c_read_byte(0x2, &ids[2]);
	it66121_i2c_read_byte(0x3, &ids[3]);
	IT66121_LOG("HDMITX ID: %x-%x-%x-%x\n",
		ids[0], ids[1], ids[2], ids[3]);

	/* 54-49-12-6 */
	if (ids[0] != 0x54 || ids[1] != 0x49) {
		/* || ids[2]!=0x12 || ids[3]!=0x06) */
		IT66121_LOG("chip ID incorrect: %x-%x-%x-%x !!\n",
		ids[0], ids[1], ids[2], ids[3]);
		/* it66121_power_off(); */
		return -1;
	}
	IT66121_LOG("MediaTek HDMI i2c probe success\n");

	IT66121_LOG("\n============================================\n");
	IT66121_LOG("IT66121 HDMI Version\n");
	IT66121_LOG("============================================\n");

	init_waitqueue_head(&hdmi_timer_wq);
	hdmi_timer_task = kthread_create(hdmi_timer_kthread, NULL,
		"hdmi_timer_kthread");
	wake_up_process(hdmi_timer_task);


#if defined(CUST_EINT_EINT_HDMI_HPD_NUM)

	IT66121_LOG(">>IT66121 Request IRQ\n");

	mt_eint_set_sens(CUST_EINT_EINT_HDMI_HPD_NUM, MT_LEVEL_SENSITIVE);
	mt_eint_registration(CUST_EINT_EINT_HDMI_HPD_NUM,
		EINTF_TRIGGER_LOW, &_it66121_irq_handler,
			     0);
	mt_eint_mask(CUST_EINT_EINT_HDMI_HPD_NUM);

	IT66121_LOG("<<IT66121 Request IRQ\n");
#endif



	return 0;
}
#endif

#ifdef ITE66121_DRM_SUPPORT
static inline struct it66121 *bridge_to_it66121(struct drm_bridge *bridge)
{
	return container_of(bridge, struct it66121, bridge);
}

static inline struct it66121 *connector_to_it66121(struct drm_connector *con)
{
	return container_of(con, struct it66121, connector);
}

static void it66121_reset(struct it66121 *it66121)
{
	IT66121_LOG("w-y %s %d\n", __func__, __LINE__);
}

static enum drm_connector_status
it66121_connector_detect(struct drm_connector *connector, bool force)
{
#if 0
	struct it66121 *it66121 = connector_to_it66121(connector);
	unsigned int status;

	regmap_read(it66121->regmap, IT66121_INT_STATUS, &status);

	return (status & IT66121_PLUGGED_STATUS) ?
	       connector_status_connected : connector_status_disconnected;
#endif
	it66121_power_on();

	IT66121_LOG("w-y %s tmp_HPD:%d\n", __func__, tmp_HPD);

	if (tmp_HPD == 1)
		return connector_status_connected;
	else
		return connector_status_disconnected;
}

static const struct drm_connector_funcs it66121_connector_funcs = {
	.dpms = drm_atomic_helper_connector_dpms,
	.detect = it66121_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = drm_connector_cleanup,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int it66121_get_modes(struct drm_connector *connector)
{
	struct it66121 *it66121 = connector_to_it66121(connector);
	u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	unsigned long timeout;
	unsigned int status;
	struct edid *edid;
	int num = 0;
	int ret;

	IT66121_LOG("%s %d\n", __func__, __LINE__);




	edid = drm_get_edid(connector, NULL);
	drm_mode_connector_update_edid_property(connector, edid);
	if (edid) {
		num = drm_add_edid_modes(connector, edid);
		kfree(edid);
	}
#if 0
	ret = drm_display_info_set_bus_formats(&connector->display_info,
					       &bus_format, 1);
	if (ret)
		return ret;
#endif

	return num;
}

static enum drm_mode_status it66121_mode_valid(
struct drm_connector *connector, struct drm_display_mode *mode)
{
	/* TODO: check mode */

	return MODE_OK;
}

static const struct drm_connector_helper_funcs
	it66121_connector_helper_funcs = {
	.get_modes = it66121_get_modes,
	.mode_valid = it66121_mode_valid,
};

static void it66121_bridge_disable(struct drm_bridge *bridge)
{
	IT66121_LOG("it66121_bridge_disable\n");
	it66121_power_down();
}

static void it66121_bridge_enable(struct drm_bridge *bridge)
{
	IT66121_LOG("it66121_bridge_enable\n");
	it66121_power_on();
}

void it66121_bridge_mode_set(struct drm_bridge *bridge,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adj)
{
	u16 h_display;
	u16 v_display;

	h_display = ((adj->hdisplay) | (adj->hdisplay >> 8)) & 0xffff;
	v_display = ((adj->vdisplay) | (adj->vdisplay >> 8)) & 0xffff;

	IT66121_LOG("%s h_display:%d, v_display:%d",
		__func__, h_display, v_display);

	/***1080P30***/
	if ((h_display >= 1920) && (h_display <= 1930) && (v_display >= 1080)
		&& (v_display <= 1090)) {
		it66121_video_config(HDMI_VIDEO_1920x1080p_30Hz,
			HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);
	} else if ((h_display >= 1280) && (h_display <= 1290)
	&& (v_display >= 720) && (v_display <= 730)) {
		it66121_video_config(HDMI_VIDEO_1280x720p_60Hz,
			HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);
	} else if ((h_display >= 720) && (h_display <= 730)
	&& (v_display >= 480) && (v_display <= 490)) {
		it66121_video_config(HDMI_VIDEO_720x480p_60Hz,
			HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);
	} else
		IT66121_LOG("no suitable hdmi resolution\n");

}

static int it66121_bridge_attach(struct drm_bridge *bridge)
{
	struct it66121 *it66121 = bridge_to_it66121(bridge);
	struct drm_device *drm = bridge->dev;
	int ret;

	drm_connector_helper_add(&it66121->connector,
				 &it66121_connector_helper_funcs);

	if (!drm_core_check_feature(drm, DRIVER_ATOMIC))
		IT66121_LOG("w-y %s %d\n", __func__, __LINE__);


	ret = drm_connector_init(drm, &it66121->connector,
				 &it66121_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret)
		IT66121_LOG("drm_connector_init fail\n");

	drm_mode_connector_attach_encoder(&it66121->connector, bridge->encoder);

	return 0;
}

static bool it66121_bridge_mode_fixup(struct drm_bridge *bridge,
				       const struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	IT66121_LOG("%s %d\n", __func__, __LINE__);
	return true;
}

static void it66121_bridge_post_disable(struct drm_bridge *bridge)
{
	IT66121_LOG("%s %d\n", __func__, __LINE__);
}

static void it66121_bridge_pre_enable(struct drm_bridge *bridge)
{
	IT66121_LOG("%s %d\n", __func__, __LINE__);
}

static const struct drm_bridge_funcs it66121_bridge_funcs = {
	.attach = it66121_bridge_attach,
	.mode_fixup = it66121_bridge_mode_fixup,
	.mode_set = it66121_bridge_mode_set,
	.disable = it66121_bridge_disable,
	.post_disable = it66121_bridge_post_disable,
	.pre_enable = it66121_bridge_pre_enable,
	.enable = it66121_bridge_enable,
};
#endif

/*----------------------------------------------------------------------------*/
unsigned char ite_basic_flag;
unsigned char ite_cec_flag;
unsigned char ite_cap_flag;
unsigned char ite_it66121_flag;
static int hdmi_ite_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	IT66121_LOG(">>hdmi_ite_probe\n");
	/* static struct mxc_lcd_platform_data *plat_data; */
	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_BYTE | I2C_FUNC_I2C))
		return -ENODEV;

	IT66121_LOG("w-y client->name:%s\n", client->name);
	IT66121_LOG("w-y device node:%s\n", client->dev.of_node->name);

	if (strcmp(client->name, "it66121-i2c") == 0) {

		IT66121_LOG(">>Match id Done\n");
		ite_it66121_flag = 1;
		it66121_i2c_client = client;

		memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
		r_hdmi_timer.expires = jiffies + 1000 / (1000 / HZ);
		/* wait 1s to stable */
		r_hdmi_timer.function = hdmi_poll_isr;
		r_hdmi_timer.data = 0;
		init_timer(&r_hdmi_timer);

		if (it66121_i2c_client != NULL) {
			IT66121_LOG("\n========================\n");
			IT66121_LOG("IT66121 HDMI Version\n");
			IT66121_LOG("==========================\n");

			init_waitqueue_head(&hdmi_timer_wq);
			hdmi_timer_task =
			    kthread_create(hdmi_timer_kthread,
			    NULL, "hdmi_timer_kthread");
			wake_up_process(hdmi_timer_task);
		}
	} else if (strcmp(client->name, "it6620-basic-i2c") == 0) {
		ite_basic_flag = 1;
		it6620_basic_client = client;
		IT66121_LOG("w-y client->name:%s\n", client->name);
		IT66121_LOG("w-y device node:%s\n", client->dev.of_node->name);
	} else if (strcmp(client->name, "it6620-cec-i2c") == 0) {
		ite_cec_flag = 1;
		it6620_cec_client = client;
		IT66121_LOG("w-y client->name:%s\n", client->name);
		IT66121_LOG("w-y device node:%s\n", client->dev.of_node->name);
	} else if (strcmp(client->name, "it6620-cap-i2c") == 0) {
		ite_cap_flag = 1;
		it6620_cap_client = client;
		IT66121_LOG("w-y client->name:%s\n", client->name);
		IT66121_LOG("w-y device node:%s\n", client->dev.of_node->name);
	} else
		IT66121_LOG("w-y client is invalid\n");

	if (ite_basic_flag && ite_cec_flag && ite_cap_flag) {
		IT66121_LOG("ite probe done ,init earc & add timer\n");

		ITE66121_DBG_Init();

	/*if dts config compatible = "ite,it66121-i2c" , it66121 should be on*/
		if (ite_it66121_flag == 1) {
			iTE_I2S4_GPIO_Reset();   /*reset it66121*/
			it66121_power_on();
		}

		IT662x_eARC_ini();

		memset((void *)&r_it6620_timer, 0, sizeof(r_it6620_timer));
		r_it6620_timer.expires = jiffies + 50 / (1000 / HZ);
		/* wait 50ms to stable */
		r_it6620_timer.function = it6620_poll_isr;
		r_it6620_timer.data = 0;
		init_timer(&r_it6620_timer);

		init_waitqueue_head(&it6620_timer_wq);
		it6620_timer_task =
			    kthread_create(it6620_timer_kthread, NULL,
			    "it6620_timer_kthread");
		wake_up_process(it6620_timer_task);

		add_timer(&r_it6620_timer);

		msleep(20);
		iTE_I2S5_GPIO_Enable();    /*enable hdmi 5V power*/

	}

	IT66121_LOG("<<hdmi_ite_probe\n");

	return ret;

}

#define HDMI_MAX_INSERT_CALLBACK   10
static CABLE_INSERT_CALLBACK hdmi_callback_table[HDMI_MAX_INSERT_CALLBACK];
void hdmi_register_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
	int i = 0;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == cb)
			break;
	}
	if (i < HDMI_MAX_INSERT_CALLBACK)
		return;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == NULL)
		break;
	}
	if (i == HDMI_MAX_INSERT_CALLBACK) {
		IT66121_LOG("not enough mhl callback entries for module\n");
		return;
	}

	hdmi_callback_table[i] = cb;
	IT66121_LOG("callback: %p,i: %d\n", hdmi_callback_table[i], i);
}

void hdmi_unregister_cable_insert_callback(CABLE_INSERT_CALLBACK cb)
{
	int i;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i] == cb) {

			hdmi_callback_table[i] = NULL;
			break;
		}
	}
	if (i == HDMI_MAX_INSERT_CALLBACK) {
		IT66121_LOG("Try to unregister callback function 0x%lx\n",
				(unsigned long int)cb);
		return;
	}
}

void hdmi_invoke_cable_callbacks(enum HDMI_STATE state)
{
	int i = 0, j = 0;

	for (i = 0; i < HDMI_MAX_INSERT_CALLBACK; i++) {
		if (hdmi_callback_table[i])
			j = i;
	}

	if (hdmi_callback_table[j]) {
		IT66121_LOG("callback: %p, state: %d, j: %d\n",
			hdmi_callback_table[j], state, j);
		hdmi_callback_table[j](state);
	}
}

const struct HDMI_DRIVER *HDMI_GetDriver(void)
{
	static const struct HDMI_DRIVER HDMI_DRV = {
		.set_util_funcs = it66121_set_util_funcs,	/*  */
		.get_params = it66121_get_params,	/*  */
		.init = it66121_init,	/* InitHDMITX */
		/* .enter          = it66121_enter, */
		/* .exit           = it66121_exit, */
		.suspend = it66121_suspend,
		.resume = it66121_resume,
		.video_config = it66121_video_config,
		/* it66121_video_config,HDMITX_SetOutput */
		.audio_config = it66121_audio_config,
		/* it66121_audio_config,HDMITX_SetAudioOutput */
		.video_enable = it66121_video_enable,
		/* HDMITX_EnableVideoOutput */
		.audio_enable = it66121_audio_enable,
		/* HDMITX_SetAudioOutput */
		.power_on = it66121_power_on,	/* HDMITX_PowerOn */
		.power_off = it66121_power_down,	/* HDMITX_PowerDown */
		/* .set_mode             = it66121_set_mode, */
		.dump = it66121_dump,	/* it66121_dump,DumpHDMITXReg */
		.getedid = ite66121_AppGetEdidInfo,
		/* .read           = it66121_read, */
		/* .write          = it66121_write, */
		.get_state = it66121_get_state,
		.log_enable = it66121_log_enable,
		.register_callback   = hdmi_register_cable_insert_callback,
		.unregister_callback = hdmi_unregister_cable_insert_callback,
	};

	return &HDMI_DRV;
}
EXPORT_SYMBOL(HDMI_GetDriver);

struct regulator *hdmi_vcn33, *hdmi_vcn18, *hdmi_vrf12;

int ite66121_pmic_power_on(void)
{
	int ret;

	pinctrl_select_state(hdmi_pinctrl, pins_hdmi_func);

	ret = regulator_enable(hdmi_vrf12);
	ret = regulator_enable(hdmi_vcn18);
	ret = regulator_enable(hdmi_vcn33);

	if (ret != 0)
		IT66121_LOG("hdmi regolator error\n");

	IT66121_LOG("ite66121_pmic_power_on\n");
	return 1;
}
int ite66121_pmic_power_off(void)
{
	struct device_node *dn;
	int bus_switch_pin;
	int ret;

	pinctrl_select_state(hdmi_pinctrl, pins_hdmi_gpio);
	ret = regulator_disable(hdmi_vcn33);
	ret = regulator_disable(hdmi_vcn18);
	ret = regulator_disable(hdmi_vrf12);

	if (ret != 0)
		IT66121_LOG("hdmi regolator error\n");

	dn = of_find_compatible_node(NULL, NULL, "mediatek,it66121-hdmitx");
	if (dn == NULL)
		IT66121_LOG("dn == NULL");
	bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
	gpio_direction_output(bus_switch_pin, 0);

	IT66121_LOG("ite66121_pmic_power_off\n");
	return 1;

}

static char debug_buffer[2048];

static void process_dbg_opt(const char *opt)
{
	unsigned int  vadr_regstart, val_temp;
	u8 val;
	int ret;
	struct device_node *dn;
	int bus_switch_pin;
	unsigned int res;
	u8 i2c_rb[5];
	u8 i2c_wb[5] = {0x50, 0x84, 0x40, 0x00, 0x05};
	int i;


	/*for it6620 I2c test start*/
	if (strncmp(opt, "add_timer:", 10) == 0) {
		ret = sscanf(opt + 10, "%x", &vadr_regstart);
		IT66121_LOG("r:0x%08x\n", vadr_regstart);
		if (vadr_regstart == 1)
			add_timer(&r_it6620_timer);
		else
			del_timer(&r_it6620_timer);

		IT66121_LOG("timer_on:0x%x\n", timer_on);
	}

	if (strncmp(opt, "timer_onoff:", 12) == 0) {
		ret = sscanf(opt + 12, "%x", &vadr_regstart);
		IT66121_LOG("r:0x%08x\n", vadr_regstart);
		if (vadr_regstart == 1)
			timer_on = 1;
		else
			timer_on = 0;

		IT66121_LOG("timer_on:0x%x\n", timer_on);
	}

	if (strncmp(opt, "it6620_init", 11) == 0) {
		IT662x_eARC_ini();
		IT66121_LOG("IT662x_eARC_RX_Ini\n");
	}

	if (strncmp(opt, "basic_r:", 8) == 0) {
		ret = sscanf(opt + 8, "%x", &vadr_regstart);
		IT66121_LOG("r:0x%08x\n", vadr_regstart);
		IT66121_LOG("0x%08x = 0x%x\n", vadr_regstart,
			iTE_I2C_ReadByte(0x9C, vadr_regstart));
	}
	if (strncmp(opt, "basic_w:", 8) == 0) {
		ret = sscanf(opt + 8, "%x=%x", &vadr_regstart, &val_temp);
		val = (u8)val_temp;
		IT66121_LOG("basic_w:0x%08x=0x%x\n", vadr_regstart, val);
		iTE_I2C_WriteByte(0x9C, vadr_regstart, val);
	}
	if (strncmp(opt, "basic_setb:", 11) == 0) {
		ret = sscanf(opt + 11, "%x=%x", &vadr_regstart, &val_temp);
		val = (u8)val_temp;
		IT66121_LOG("basic_setb:0x%08x=0x%x\n", vadr_regstart, val);
		iTE_I2C_SetByte(0x9C, vadr_regstart, 3, val);
		IT66121_LOG("0x%08x = 0x%x\n", vadr_regstart,
			iTE_I2C_ReadByte(0x9C, vadr_regstart));
	}
	if (strncmp(opt, "basic_rb:", 9) == 0) {
		ret = sscanf(opt + 9, "%x=%x", &vadr_regstart, &val_temp);
		val = (u8)val_temp;
		IT66121_LOG("basic_rb:0x%08x=0x%x\n", vadr_regstart, val);
		iTE_I2C_ReadBurst(0x9C, vadr_regstart, 5, &i2c_rb[0]);
		for (i = 0; i < 5 ; i++)
			IT66121_LOG("basic_rb: r[%d]:0x%x\n", i, i2c_rb[i]);

	}
	if (strncmp(opt, "basic_r_bank1:", 14) == 0) {
		ret = sscanf(opt + 14, "%x", &vadr_regstart);
		IT66121_LOG("basic_r_bank1:0x%08x\n", vadr_regstart);
		IT662x_eARC_RX_Bank(1);
		IT66121_LOG("0x%08x = 0x%x\n", vadr_regstart,
			iTE_I2C_ReadByte(0x9C, vadr_regstart));
		IT662x_eARC_RX_Bank(0);
	}
	if (strncmp(opt, "cec_r:", 6) == 0) {
		ret = sscanf(opt + 6, "%x", &vadr_regstart);
		IT66121_LOG("cec_r:0x%08x\n", vadr_regstart);
		IT66121_LOG("0x%08x = 0x%x\n", vadr_regstart,
			iTE_I2C_ReadByte(0xCA, vadr_regstart));
	}
	if (strncmp(opt, "cec_w:", 6) == 0) {
		ret = sscanf(opt + 6, "%x=%x", &vadr_regstart, &val_temp);
		val = (u8)val_temp;
		IT66121_LOG("cec_w:0x%08x=0x%x\n", vadr_regstart, val);
		iTE_I2C_WriteByte(0xCA, vadr_regstart, val);
	}
	if (strncmp(opt, "cec_wb:", 7) == 0) {
		ret = sscanf(opt + 7, "%x=%x", &vadr_regstart, &val_temp);
		val = (u8)val_temp;
		IT66121_LOG("cec_wb:0x%08x=0x%x\n", vadr_regstart, val);
		iTE_I2C_WriteBurst(0xCA, vadr_regstart, 5, &i2c_wb[0]);
		for (i = 0; i < 5 ; i++)
			IT66121_LOG("cec_wb: w[%d]:0x%x\n", i, i2c_wb[i]);

	}
	if (strncmp(opt, "test_msg:", 9) == 0) {
		ret = sscanf(opt + 9, "%x", &vadr_regstart);
		IT66121_LOG("test_msg:0x%08x\n", vadr_regstart);
		test_msg(vadr_regstart);
	}
	if (strncmp(opt, "reg_dump", 8) == 0) {
		IT66121_LOG("***** basic reg bank0 dump start *****\n");
		IT662x_eARC_RX_Bank(0);
		IT66121_LOG("|   00 01 02 03 04 05 06 07 08\n");
		for (i = 0; i <= 248; i = i+8) {
			IT66121_LOG("%x: %x %x %x %x %x %x %x %x\n",
				i, iTE_I2C_ReadByte(0x9C, i),
			iTE_I2C_ReadByte(0x9C, i+1),
			iTE_I2C_ReadByte(0x9C, i+2),
			iTE_I2C_ReadByte(0x9C, i+3),
			iTE_I2C_ReadByte(0x9C, i+4),
			iTE_I2C_ReadByte(0x9C, i+5),
			iTE_I2C_ReadByte(0x9C, i+6),
			iTE_I2C_ReadByte(0x9C, i+7));
		}
		IT66121_LOG("***** basic reg bank0 dump end *****\n");

		IT66121_LOG("***** basic reg bank1 dump start *****\n");
		IT662x_eARC_RX_Bank(1);
		IT66121_LOG("|   00 01 02 03 04 05 06 07 08\n");
		for (i = 0; i <= 248; i = i+8) {
			IT66121_LOG("%x: %x %x %x %x %x %x %x %x\n",
				i, iTE_I2C_ReadByte(0x9C, i),
			iTE_I2C_ReadByte(0x9C, i+1),
			iTE_I2C_ReadByte(0x9C, i+2),
			iTE_I2C_ReadByte(0x9C, i+3),
			iTE_I2C_ReadByte(0x9C, i+4),
			iTE_I2C_ReadByte(0x9C, i+5),
			iTE_I2C_ReadByte(0x9C, i+6),
			iTE_I2C_ReadByte(0x9C, i+7));
		}
		IT662x_eARC_RX_Bank(0);
		IT66121_LOG("***** basic reg bank1 dump end *****\n");
	}

	/*for it6620 I2c test end*/

	if (strncmp(opt, "log_on:", 7) == 0) {
		ret = sscanf(opt + 7, "%x", &res);
		IT66121_LOG_on = res;
		IT66121_LOG("IT66121_LOG_on: %d\n", IT66121_LOG_on);
	}

	if (strncmp(opt, "tx_send", 7) == 0)
		CecSys_TxHandler();

	if (strncmp(opt, "edid", 4) == 0)
		IT66121_LOG("resolution = 0x%x\n", sink_support_resolution);

	if (strncmp(opt, "res:", 4) == 0) {
		ret = sscanf(opt + 4, "%x", &res);
		IT66121_LOG("hdmi %d\n", res);
		it66121_video_config((enum HDMI_VIDEO_RESOLUTION)res,
			HDMI_VIN_FORMAT_RGB888, HDMI_VOUT_FORMAT_RGB888);
	}

	if (strncmp(opt, "disable", 7) == 0)
		ret = regulator_disable(hdmi_vrf12);

	if (strncmp(opt, "enable", 6) == 0)
		ret = regulator_enable(hdmi_vrf12);

	if (strncmp(opt, "on", 2) == 0) {
		dn = of_find_compatible_node(NULL, NULL,
			"mediatek,it66121-hdmitx");
		if (dn == NULL)
			IT66121_LOG("dn == NULL");
		bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
		gpio_direction_output(bus_switch_pin, 1);
	}

	if (strncmp(opt, "off", 3) == 0) {
		dn = of_find_compatible_node(NULL, NULL,
			"mediatek,it66121-hdmitx");
		if (dn == NULL)
			IT66121_LOG("dn == NULL");
		bus_switch_pin = of_get_named_gpio(dn, "hdmi_power_gpios", 0);
		gpio_direction_output(bus_switch_pin, 0);
	}
	if (strncmp(opt, "power_on", 8) == 0) {
		IT66121_LOG("hdmi power_on\n");
		ite66121_pmic_power_on();
	}
	if (strncmp(opt, "power_off", 9) == 0) {
		IT66121_LOG("hdmi power_off\n");
		ite66121_pmic_power_off();
	}
	if (strncmp(opt, "init", 4) == 0) {
		IT66121_LOG("hdmi it66121_init\n");
		it66121_init();
	}
	if (strncmp(opt, "itepower_on", 11) == 0) {
		IT66121_LOG("hdmi it66121_power_on\n");
		it66121_power_on();
	}
	if (strncmp(opt, "itepower_off", 12) == 0) {
		IT66121_LOG("hdmi it66121_power_off\n");
		it66121_power_down();
	}
	if (strncmp(opt, "read:", 5) == 0) {
		ret = sscanf(opt + 5, "%x", &vadr_regstart);
		IT66121_LOG("r:0x%08x\n", vadr_regstart);
		it66121_i2c_read_byte(vadr_regstart, &val);
		IT66121_LOG("0x%08x = 0x%x\n", vadr_regstart, val);
	}
		if (strncmp(opt, "write:", 6) == 0) {
		ret = sscanf(opt + 6, "%x=%x", &vadr_regstart, &val_temp);
		val = (u8)val_temp;
		IT66121_LOG("w:0x%08x=0x%x\n", vadr_regstart, val);
		it66121_i2c_write_byte(vadr_regstart, val);
	}

}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	pr_debug("[extd] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debug_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	process_dbg_cmd(debug_buffer);

	return ret;
}
static const char STR_HELP[] =
	"\n"
	"USAGE\n"
	"HDMI power on:\n"
	"		echo power_on>hdmi_test"


	"\n";

static ssize_t debug_read(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(debug_buffer) - 1;
	int n = 0;

	n += scnprintf(debug_buffer + n, debug_bufmax - n, STR_HELP);
	debug_buffer[n++] = 0;

	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static const struct file_operations debug_fops = {
	.read = debug_read,
	.write = debug_write,
	.open = debug_open,
};
struct dentry *ite66121_dbgfs;

void ITE66121_DBG_Init(void)
{
	ite66121_dbgfs = debugfs_create_file("hdmi_test",
		S_IFREG | 444, NULL, (void *)0, &debug_fops);
}

void it6620_poll_isr(unsigned long n)
{
	atomic_set(&it6620_timer_event, 1);
	wake_up_interruptible(&it6620_timer_wq);
	mod_timer(&r_it6620_timer, jiffies + 50 / (1000 / HZ));

}

void hdmi_poll_isr(unsigned long n)
{
	atomic_set(&hdmi_timer_event, 1);
	wake_up_interruptible(&hdmi_timer_wq);
	mod_timer(&r_hdmi_timer, jiffies + 1000 / (1000 / HZ));

}

#if 0
void vGet_Pinctrl_Mode(struct platform_device *pdev)
{
	int ret = 0;

	if (pdev == NULL)
		IT66121_LOG("vGet_DDC_Mode Error, Invalid device pointer\n");

	hdmi_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(hdmi_pinctrl)) {
		ret = PTR_ERR(hdmi_pinctrl);
		IT66121_LOG("HDMI pins, failure of setting\n");
	} else {
		pins_hdmi_func = pinctrl_lookup_state(hdmi_pinctrl,
			"hdmi_poweron");
		if (IS_ERR(pins_hdmi_func)) {
			ret = PTR_ERR(pins_hdmi_func);
			IT66121_LOG("cannot find pins_pinctrl hdmi_poweron\n");
		}

		pins_hdmi_gpio = pinctrl_lookup_state(hdmi_pinctrl,
			"hdmi_poweroff");
		if (IS_ERR(pins_hdmi_gpio)) {
			ret = PTR_ERR(pins_hdmi_gpio);
			IT66121_LOG("cannot find pins_pinctrl hdmi_poweroff\n");
		}
	}
}

int hdmi_internal_probe(struct platform_device *pdev)
{

#ifdef ITE66121_DRM_SUPPORT
		struct device *dev = &pdev->dev;
		struct it66121 *it66121;
#endif

	IT66121_LOG(">>> %s,\n", __func__);

		/* HDMI_reset(); */
#if 0
	ret = i2c_add_driver(&hdmi_ite_i2c_driver);
	/* This leave for MT6592 initialize it66121 */
	/* register i2c device */
#endif
#ifdef ITE66121_DRM_SUPPORT
		it66121 = devm_kzalloc(dev, sizeof(*it66121), GFP_KERNEL);

		it66121->bridge.funcs = &it66121_bridge_funcs;
		it66121->bridge.of_node = dev->of_node;
		ret = drm_bridge_add(&it66121->bridge);
#endif
#if 0
	vGet_Pinctrl_Mode(pdev);
	hdmi_vcn33 = devm_regulator_get(&pdev->dev, "vcn33");
	hdmi_vcn18 = devm_regulator_get(&pdev->dev, "vcn18");
	hdmi_vrf12 = devm_regulator_get(&pdev->dev, "vrf12");
	if (IS_ERR(hdmi_vcn33))
		IT66121_LOG("hdmi hdmi_vcn33 error\n");
	if (IS_ERR(hdmi_vcn18))
		IT66121_LOG("hdmi hdmi_vcn18 error\n");
	if (IS_ERR(hdmi_vrf12))
		IT66121_LOG("hdmi hdmi_vrf12 error\n");
#endif

	memset((void *)&r_hdmi_timer, 0, sizeof(r_hdmi_timer));
	r_hdmi_timer.expires = jiffies + 1000 / (1000 / HZ);
	/* wait 1s to stable */
	r_hdmi_timer.function = hdmi_poll_isr;
	r_hdmi_timer.data = 0;
	init_timer(&r_hdmi_timer);

	ITE66121_DBG_Init();
	return 0;
}
static int hdmi_internal_remove(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id hdmi_of_ids[] = {
	{.compatible = "mediatek,it66121-hdmitx",},
	{}
};

static struct platform_driver hdmi_of_driver = {
	.probe = hdmi_internal_probe,
	.remove = hdmi_internal_remove,
	.driver = {
		   .name = "mtkhdmi",
		   .of_match_table = hdmi_of_ids,
	}
};

static int __init mtk_hdmitx_init(void)
{
	int ret;

	IT66121_LOG("mtk_hdmitx_init\n");
	if (platform_driver_register(&hdmi_of_driver)) {
		IT66121_LOG("failed to register disp driver\n");
		ret = -1;
	}
	return 0;
}
static void __exit mtk_hdmitx_exit(void)
{
	IT66121_LOG("mtk_hdmitx_exit\n");
}
#endif

/*-----------------------------------*/


static int __init ite66121_it6620_i2c_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&hdmi_ite_i2c_driver);
	if (ret)
		IT66121_LOG("%s: failed to add it66121 i2c driver\n", __func__);

#if SOFTWARE_I2C
	vHdmiDDCGetGpio();
	ddc_DBG_Init();
#endif

	return ret;
}
/*--------------------------------------*/
/*core_initcall(ite66121_i2c_board_init);*/
module_init(ite66121_it6620_i2c_init);
/*module_init(mtk_hdmitx_init);*/
/*module_exit(mtk_hdmitx_exit);*/
MODULE_LICENSE("GPL v2");



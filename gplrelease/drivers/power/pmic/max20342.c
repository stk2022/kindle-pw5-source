// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Amazon Inc.
 * Author: Chih Chieh Chou <chihcho@amazon.com>
 */
//#define DEBUG
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <asm/gpio.h>
#include <power/max20342.h>


#define MAX20342_NAME		"max20342"
#define MAX20342_OF_NODE_NAME	"maxim,max20342"

#define MAX20342_SLV_ADDR 0x35

/*
 * USER INTERRUPTS
 */
#define MAX20342_REVISION_ID		0x00
#define MAX20342_COMMON_INT		0x01
#define MAX20342_CC_INT			0x02
#define MAX20342_BC_INT			0x03
#define MAX20342_OVP_INT		0x04
#define MAX20342_RES_INT1		0x05
#define MAX20342_RES_INT2		0x06
#define MAX20342_COMMON_STATUS		0x07
#define MAX20342_CC_STATUS1		0x08
#define MAX20342_CC_STATUS2		0x09
#define MAX20342_BC_STATUS		0x0A
#define MAX20342_OVP_STATUS		0x0B
#define MAX20342_COMMON_MASK		0x0C
#define MAX20342_CC_MASK		0x0D
#define MAX20342_BC_MASK		0x0E
#define MAX20342_OVP_MASK		0x0F
#define MAX20342_RES_MASK1		0x10
#define MAX20342_RES_MASK2		0x11

/*
 * USER COMMON
 */
#define MAX20342_COMM_CTRL1		0x15
#define MAX20342_COMM_CTRL2		0x16
#define MAX20342_COMM_CTRL3		0x19

/*
 * USER OVP
 */
#define MAX20342_OVP_CTRL		0x1A

/*
 * USER USBC
 */
#define MAX20342_CC_CTRL0		0x20
#define MAX20342_CC_CTRL1		0x21
#define MAX20342_CC_CTRL2		0x22
#define MAX20342_CC_CTRL3		0x23
#define MAX20342_CC_CTRL4		0x24
#define MAX20342_CC_CTRL5		0x25
#define MAX20342_CC_CTRL6		0x26
#define MAX20342_VCONN_ILIM		0x28

/*
 * USER BC12
 */
#define MAX20342_BC_CTRL0		0x2A
#define MAX20342_BC_CTRL1		0x2B


#define BC_MASK_DEFAULT			0x12
#define INTR_MASK_ALL			0x00

#define COMM_CTRL1_DEFAULT		0xe2
#define COMM_CTRL2_DEFAULT		0xc1 // Should be the same as setting of kernel driver
#define COMM_CTRL3_DEFAULT		0x01
#define OVP_CTRL_DEFAULT		0x02

#define COMMON_STATUS_FAULT_MASK	BIT(7)
#define COMMON_STATUS_FAULT_STATE_MASKS	0x50

#define COMM_CTRL3_FAULT_UNLOCK_MASK	BIT(0)

#define BC_STATUS_CHG_DET_RUN_MASK	BIT(1)


#define GPIO_INTER_NUM			29


struct udevice *usb_i2c_dev __attribute__((section(".data"))) = NULL;


static int max20342_init_i2c(void)
{
	struct udevice *bus;
	int ret;
	int i;


	// Loop for all i2c buses
	for (i = 0; ; i++) {
		ret = uclass_get_device_by_seq(UCLASS_I2C, i, &bus);
		if (ret) {
			printf("%s: No bus\n", __func__);
			return 0;
		}

		ret = dm_i2c_probe(bus, MAX20342_SLV_ADDR, 0, &usb_i2c_dev);
		if (ret) {
			pr_notice("%s: i2c probe bus %d fail\n",  __func__, i);
		}
		else {
			pr_notice("%s: i2c probe bus %d ok\n",  __func__, i);
			return 1;
		}
	}

	return 0;
}

static int max20342_reg_read(uint dest_reg, uint mask, uint shift)
{
	u_char read_val;
	int ret;
	static int dev_init = -1;

	if (!usb_i2c_dev && (dev_init < 0))
		dev_init = max20342_init_i2c();

	if (!dev_init)
		return -1;

	ret = dm_i2c_read(usb_i2c_dev, dest_reg, &read_val, 1);
	if (ret)
		return -1;

	debug("read dest_reg from 0x%02x: 0x%02x\n", dest_reg, read_val);
	read_val &= mask;
	read_val >>= shift;

	return read_val;
}

static int max20342_reg_write(uint dest_reg, uint dest_val, uint mask, uint shift)
{
	u_char read_val;
	int ret;
	static int dev_init = -1;

	if (!usb_i2c_dev && (dev_init < 0))
		dev_init = max20342_init_i2c();

	if (!dev_init)
		return -1;

	ret = dm_i2c_read(usb_i2c_dev, dest_reg, &read_val, 1);
	if (ret) {
		// MAX20342 might be in shutdown mode, re-read again
		// wait 1s for MAX20342 to exit from shutdown mode
		udelay(1000000);
		ret = dm_i2c_read(usb_i2c_dev, dest_reg, &read_val, 1);
		if (ret) {
			printf("max20342 write error\n");
			return ret;
		}
	}

	debug("read dest_reg from 0x%02x: 0x%02x\n", dest_reg, read_val);

	read_val &= (~mask);
	read_val |= ((dest_val<<shift) & mask);

	debug("write dest_reg to 0x%02x: 0x%02x\n", dest_reg, read_val);

	ret = dm_i2c_write(usb_i2c_dev, dest_reg, &read_val, 1);
	if (ret)
		return -1;

	return 0;
}

static u8 max20342_read_bc_int(void)
{
	u8 value;

	value = max20342_reg_read(MAX20342_BC_INT, 0xff, 0);
	debug("MAX20342_BC_INT :0x%02x\n", value);
	return value;
}

static u8 max20342_read_bc_status(void)
{
	u8 value;

	value = max20342_reg_read(MAX20342_BC_STATUS, 0xff, 0);
	debug("MAX20342_BC_STATUS :0x%02x\n", value);
	return value;
}

/*
 * For debug only
void max20342_read_regs(void){
	printf("COMMON_MASK :0x%02x\n", max20342_reg_read(MAX20342_COMMON_MASK, 0xff, 0));
	printf("MAX20342_CC_MASK :0x%02x\n", max20342_reg_read(MAX20342_CC_MASK, 0xff, 0));
	printf("MAX20342_BC_MASK :0x%02x\n", max20342_reg_read(MAX20342_BC_MASK, 0xff, 0));
	printf("MAX20342_OVP_MASK :0x%02x\n", max20342_reg_read(MAX20342_OVP_MASK, 0xff, 0));
	printf("MAX20342_RES_MASK1 :0x%02x\n", max20342_reg_read(MAX20342_RES_MASK1, 0xff, 0));
	printf("MAX20342_RES_MASK2 :0x%02x\n", max20342_reg_read(MAX20342_RES_MASK2, 0xff, 0));
	printf("MAX20342_COMM_CTRL1 :0x%02x\n", max20342_reg_read(MAX20342_COMM_CTRL1, 0xff, 0));
	printf("MAX20342_COMM_CTRL2 :0x%02x\n", max20342_reg_read(MAX20342_COMM_CTRL2, 0xff, 0));
}
*/

static void max20342_clear_intrs(void)
{
	// ReadClear all interrupts
	max20342_reg_read(MAX20342_COMMON_INT, 0xff, 0);
	max20342_reg_read(MAX20342_CC_INT, 0xff, 0);
	max20342_reg_read(MAX20342_BC_INT, 0xff, 0);
	max20342_reg_read(MAX20342_OVP_INT, 0xff, 0);
	max20342_reg_read(MAX20342_RES_INT1, 0xff, 0);
	max20342_reg_read(MAX20342_RES_INT2, 0xff, 0);
}

static u8 max20342_inquiry_charger_type(void)
{
	return max20342_reg_read(MAX20342_BC_STATUS, 0x60, 5);
}

void max20342_fastboot_initialize(void)
{
	static bool init = false;
	int ret;
	u8 chg_typ;

	debug("max20342_fastboot_initialize\n");
	// Clear all pending interrupts
	max20342_clear_intrs();

	// Set some default values
	max20342_reg_write(MAX20342_COMMON_MASK, INTR_MASK_ALL, 0xff, 0);
	max20342_reg_write(MAX20342_CC_MASK, INTR_MASK_ALL, 0xff, 0);
	max20342_reg_write(MAX20342_BC_MASK, BC_MASK_DEFAULT, 0xff, 0);
	max20342_reg_write(MAX20342_OVP_MASK, INTR_MASK_ALL, 0xff, 0);
	max20342_reg_write(MAX20342_RES_MASK1, INTR_MASK_ALL, 0xff, 0);
	max20342_reg_write(MAX20342_RES_MASK2, INTR_MASK_ALL, 0xff, 0);

	if (init == false) {
		ret = gpio_request(GPIO_INTER_NUM, "chg_det_intr");
		if (ret < 0) {
			printf("gpio_request error\n");
			return;
		}

		ret = gpio_direction_input(GPIO_INTER_NUM);
		if (ret < 0) {
			printf("gpio_direction_input error\n");
		}
		init = true;
	}

	chg_typ = max20342_inquiry_charger_type();
	debug("chg_typ: %d\n", chg_typ);

	if (chg_typ == 2) {
		// CDP is detected
		printf("USBSWC to USB\n");
		max20342_swc_usb();
	}
}

void max20342_initialize(void)
{
	u8 common_status;

	debug("max20342_initialize\n");
	max20342_reg_write(MAX20342_COMM_CTRL1, COMM_CTRL1_DEFAULT, 0xff, 0);
	max20342_reg_write(MAX20342_COMM_CTRL2, COMM_CTRL2_DEFAULT, 0xff, 0);
	/* Set OVP switch to auto mode */
	max20342_reg_write(MAX20342_OVP_CTRL, OVP_CTRL_DEFAULT, 0xff, 0);

	/* Clear fault state if needed */
	common_status = max20342_reg_read(MAX20342_COMMON_STATUS, 0xff, 0);

	if (common_status & COMMON_STATUS_FAULT_MASK) {
		printf("max20342 in fault state\n");

		if (!(common_status & COMMON_STATUS_FAULT_STATE_MASKS)) {
			max20342_reg_write(MAX20342_COMM_CTRL3, COMM_CTRL3_FAULT_UNLOCK_MASK, 0x01, 0);
			printf("max20342 self faultunlocks\n");
		}
	}
	/* Set FaultUnlock bit */
	max20342_reg_write(MAX20342_COMM_CTRL3, COMM_CTRL3_DEFAULT, 0xff, 0);
}

void max20342_enable_shutdown_mode(void)
{
	max20342_reg_write(MAX20342_COMM_CTRL1, 1, 0x01, 0);
}

void max20342_swc_auto(void)
{
	max20342_reg_write(MAX20342_COMM_CTRL2, 3, 0xc0, 6);
}

void max20342_swc_usb(void)
{
	max20342_reg_write(MAX20342_COMM_CTRL2, 2, 0xc0, 6);
}

static bool max20342_is_int_pending(void)
{
	int ret;

	ret = gpio_get_value(GPIO_INTER_NUM);
	if (ret < 0) {
		printf("gpio_get_value error\n");
		return false;
	}

	// low active
	if (ret == 0)
		debug("gpio:%d\n", ret);
	return ret ? false : true;
}

void max20342_handle_interrupts(void)
{
	u8 bc_status;
	u8 chg_typ;

	/*
	 * Check USB type and set USB/UART switch when CDP.
	 * We must not block the code here because we need to consider the Bellatrix,
	 * which may not be reworked and bypasses charger detection IC.
	 */
	if (max20342_is_int_pending()) {
		// We must clear before check chg_type!
		max20342_read_bc_int();
		bc_status = max20342_read_bc_status();

		if (!(bc_status & BC_STATUS_CHG_DET_RUN_MASK)) {
			chg_typ = max20342_inquiry_charger_type();
			debug("chg_typ: %d\n", chg_typ);

			if (chg_typ == 2) {
				// CDP is detected
				printf("USBSWC to USB\n");
				max20342_swc_usb();
			}
			else {
				printf("USBSWC to auto\n");
				max20342_swc_auto();
			}
		}
	}
}


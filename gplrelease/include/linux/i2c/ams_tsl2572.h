/*
 *****************************************************************************
 * Copyright by ams AG                                                       *
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
 *  within the AMS-TAOS TSL2572 family of devices.
 */

#ifndef __TSL2572_H
#define __TSL2572_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#ifdef CONFIG_QUALCOMM_AP
#include <linux/sensors.h>
#endif
#ifdef AMS_MUTEX_DEBUG
#define AMS_MUTEX_LOCK(m) { \
    pr_info("%s: Mutex Lock\n", __func__); \
    mutex_lock(m); \
}
#define AMS_MUTEX_UNLOCK(m) { \
    pr_info("%s: Mutex Unlock\n", __func__); \
    mutex_unlock(m); \
}
#else
#define AMS_MUTEX_LOCK(m) { \
    mutex_lock(m); \
}
#define AMS_MUTEX_UNLOCK(m) { \
    mutex_unlock(m); \
}
#endif

// fraction out of 10
#define TENTH_FRACTION_OF_VAL(v, x) ({ \
        int __frac = v; \
        if (((x) > 0) && ((x) < 10)) __frac = (__frac*(x)) / 10 ; \
        __frac; \
        })


enum tsl2572_regs {
    TSL2572_CMD_OFFSET  = 0xA0,
    TSL2572_REG_ENABLE  = 0x00 + TSL2572_CMD_OFFSET,
    TSL2572_REG_ATIME   = 0x01 + TSL2572_CMD_OFFSET,
    TSL2572_REG_WTIME   = 0x03 + TSL2572_CMD_OFFSET,
    TSL2572_REG_AILTL   = 0x04 + TSL2572_CMD_OFFSET,
    TSL2572_REG_AILTH   = 0x05 + TSL2572_CMD_OFFSET,
    TSL2572_REG_AIHTL   = 0x06 + TSL2572_CMD_OFFSET,
    TSL2572_REG_AIHTH   = 0x07 + TSL2572_CMD_OFFSET,
    TSL2572_REG_PERS    = 0x0C + TSL2572_CMD_OFFSET,
    TSL2572_REG_CFG     = 0x0D + TSL2572_CMD_OFFSET,
    TSL2572_REG_CTRL    = 0x0F + TSL2572_CMD_OFFSET,
    TSL2572_REG_ID      = 0x12 + TSL2572_CMD_OFFSET,
    TSL2572_REG_STATUS  = 0x13 + TSL2572_CMD_OFFSET,
    TSL2572_REG_C0DATAL = 0x14 + TSL2572_CMD_OFFSET,
    TSL2572_REG_C0DATAH = 0x15 + TSL2572_CMD_OFFSET,
    TSL2572_REG_C1DATAL = 0x16 + TSL2572_CMD_OFFSET,
    TSL2572_REG_C1DATAH = 0x17 + TSL2572_CMD_OFFSET,

    TSL2572_SPCCMD_OFFSET = 0xE0,
    TSL2572_REG_AINTCLR   = 0x06 + TSL2572_SPCCMD_OFFSET,
};

enum tsl2572__reg {
    TSL2572_MASK_APERS = 0x0f,
    TSL2572_SHIFT_APERS = 0,

    TSL2572_MASK_WLONG = 0x02,
    TSL2572_SHIFT_WLONG = 1,

    TSL2572_MASK_AGAIN = 0x03,
    TSL2572_SHIFT_AGAIN = 0,

    TSL2572_MASK_AGL = 0x04,
    TSL2572_SHIFT_AGL = 2,
};

enum tsl2572_en_reg {
    TSL2572_PON = (1 << 0),
    TSL2572_AEN = (1 << 1),
    TSL2572_WEN = (1 << 3),
    TSL2572_AIEN = (1 << 4),
    TSL2572_SAI = (1 << 6),
    TSL2572_EN_ALL = (TSL2572_AEN | TSL2572_WEN),
};

enum tsl2572_status {
    TSL2572_ST_ALS_VALID = (1 << 0),
    TSL2572_ST_ALS_IRQ = (1 << 4),
};

#define MAX_REGS 256
struct device;

enum tsl2572_pwr_state {
    POWER_ON, POWER_OFF, POWER_STANDBY,
};

enum tsl2572_ctrl_reg {
    AGAIN_1   = (0 << TSL2572_SHIFT_AGAIN),
    AGAIN_8   = (1 << TSL2572_SHIFT_AGAIN),
    AGAIN_16  = (2 << TSL2572_SHIFT_AGAIN),
    AGAIN_120 = (3 << TSL2572_SHIFT_AGAIN),
};

#define SCALE 1000
#define INTEGRATION_CYCLE 2730
#define AW_TIME_MS(p)  (256 - ((((p) * SCALE) + (INTEGRATION_CYCLE - 1)) / INTEGRATION_CYCLE))
#define ATIME_TO_MS(p)  (((256 - (p)) * INTEGRATION_CYCLE) / SCALE)

/* lux */
#define TSL2572_MAX_LUX		0xffff
#define TSL2572_MAX_ALS_VALUE	0xffff
#define TSL2572_MIN_ALS_VALUE	1

struct tsl2572_lux_segment {
    u32 ch0_coef;
    u32 ch1_coef;
};

struct tsl2572_parameters {
    u8 persist;
    u8 als_time;
    u16 als_deltap;
    u8 als_gain;
    u32 als_gain_auto;
    u32 d_factor;
    struct tsl2572_lux_segment lux_segment[2];
};

struct tsl2572_als_info {
    u16 als_ch0; /* photopic channel */
    u16 als_ch1; /* ir channel */
    u32 cpl;
    u32 lux1_ch0_coef;
    u32 lux1_ch1_coef;
    u32 lux2_ch0_coef;
    u32 lux2_ch1_coef;
    u32 saturation;
    u16 lux;
};

struct tsl2572_chip {
    struct mutex lock;
    struct i2c_client *client;
    struct tsl2572_als_info als_inf;
    struct tsl2572_parameters params;
    struct tsl2572_i2c_platform_data *pdata;
    u8 shadow[MAX_REGS];

    struct input_dev *a_idev;
#ifdef CONFIG_QUALCOMM_AP
    struct sensors_classdev als_cdev;
#endif

    bool unpowered;
    bool als_enabled;
    bool is_als_valid;
    bool in_asat;
    bool in_suspend;
    bool wake_irq;

    u8 device_index;
};

/* Must match definition in ../arch file */
struct tsl2572_i2c_platform_data {
    /* The following callback for power events received and handled by
       the driver.  Currently only for SUSPEND and RESUME */
    int (*platform_power)(struct device *dev, enum tsl2572_pwr_state state);
    int (*platform_init)(void);
    void (*platform_teardown)(struct device *dev);

    char const *als_name;
    struct tsl2572_parameters parameters;
    bool als_can_wake;
    u32 ams_irq_gpio; /* as per DTS */

#ifdef CONFIG_OF
    struct device_node *of_node;
#endif
};

#ifdef CONFIG_LAB126
extern void tsl2572_xthresh_uev(int lux);
#endif

#endif /* __TSL2572_H */

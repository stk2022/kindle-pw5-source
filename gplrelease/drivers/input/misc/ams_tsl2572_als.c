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
 * proximity detection (prox) functionality within the
 * AMS-TAOS TSL2572 family of devices.
 */

#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include <linux/i2c/ams_tsl2572.h>
#include "ams_i2c.h"

#define Q_SCALE (8)

/* gain code is a bitmask:
 * bits-  7:3 |  2:1  |   0
 * value-  0  | AGAIN |  AGL
 */
enum tsl2572_als_gains {
    GAIN1_6 = 0x1,
    GAIN8_6 = 0x3,
    GAIN1   = 0x0,
    GAIN8   = 0x2,
    GAIN16  = 0x4,
    GAIN120 = 0x6,
};

typedef struct als_gain_t {
    u32  q_factor;
    u8   code;
    char *str;
} als_gain_t;
static als_gain_t als_gains[] = {
    { 41,    GAIN1_6, "0.16" },
    { 256,   GAIN1,   "1"   },
    { 328,   GAIN8_6, "1.28" },
    { 2048,  GAIN8,   "8"   },
    { 4096,  GAIN16,  "16"  },
    { 30720, GAIN120, "120"  },
};

static u8 const restorable_als_regs[] = {
    TSL2572_REG_ATIME,
    TSL2572_REG_WTIME,
    TSL2572_REG_PERS,
    TSL2572_REG_CFG,
    TSL2572_REG_CTRL,
};

static u8 tsl2572_current_gain_code(struct tsl2572_chip *chip)
{
    u8 gain_code = 0;
    gain_code  =
        (((chip->shadow[TSL2572_REG_CTRL] & TSL2572_MASK_AGAIN)
          >> TSL2572_SHIFT_AGAIN) << 1);
    gain_code |=
        ((chip->shadow[TSL2572_REG_CFG] & TSL2572_MASK_AGL)
         >> TSL2572_SHIFT_AGL);
    return gain_code;
}

static als_gain_t tsl2572_gain_code_to_gain_t(struct tsl2572_chip *chip, u8 gain_code)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(als_gains); i++) {
        if (als_gains[i].code == gain_code)
            return als_gains[i];
    }
    dev_warn(&chip->client->dev, "Cannot find gain_code: %x", gain_code);
    return als_gains[1]; //invalid code, default return Gain = 1
}

static als_gain_t *tsl2572_gain_str_to_gain_t(struct tsl2572_chip *chip, const char *gain_str, size_t size)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(als_gains); i++) {
        if ( strncmp(als_gains[i].str,
                    gain_str,
                    max(size, strlen(als_gains[i].str))) == 0)
            return &als_gains[i];
    }
    dev_warn(&chip->client->dev, "Cannot find gain_str: %s", gain_str);
    return NULL; //invalid str, default return Gain = 1
}

static int tsl2572_flush_als_regs(struct tsl2572_chip *chip)
{
    int i;
    int rc;
    u8 reg;

    for (i = 0; i < ARRAY_SIZE(restorable_als_regs); i++) {
        reg = restorable_als_regs[i];
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

int tsl2572_read_als(struct tsl2572_chip *chip)
{
    int ret;

    ret = ams_i2c_blk_read(chip->client, TSL2572_REG_C0DATAL,
            &chip->shadow[TSL2572_REG_C0DATAL], 4 * sizeof(u8));

    if (ret >= 0) {
        chip->als_inf.als_ch0 = le16_to_cpu(
                *((const __le16 *) &chip->shadow[TSL2572_REG_C0DATAL]));
        chip->als_inf.als_ch1 = le16_to_cpu(
                *((const __le16 *) &chip->shadow[TSL2572_REG_C1DATAL]));

        ret = 0;
    }

    return ret;
}

static int tsl2572_max_als_value(struct tsl2572_chip *chip)
{
    u32 val;

    val = 256 - chip->shadow[TSL2572_REG_ATIME];
    if (val > 63)
        val = 0xffff;
    else
        val = ((val * 1024) - 1);
    return val;
}

static void tsl2572_calc_cpl(struct tsl2572_chip *chip)
{
    u32 cpl;
    u32 sat;
    u8 atime;
    u8 gain_code = tsl2572_current_gain_code(chip);

    atime = 256 - chip->shadow[TSL2572_REG_ATIME];

    cpl = atime;
    cpl *= INTEGRATION_CYCLE;
    cpl *= tsl2572_gain_code_to_gain_t(chip, gain_code).q_factor;
    cpl >>= Q_SCALE;

    if (chip->params.d_factor > 0)
        //do_div(cpl, chip->params.d_factor);
        cpl /= chip->params.d_factor;

    sat = tsl2572_max_als_value(chip);
    sat = TENTH_FRACTION_OF_VAL(sat, 9);

    chip->als_inf.cpl = cpl;
    chip->als_inf.saturation = sat;
}

int tsl2572_configure_als_mode(struct tsl2572_chip *chip, u8 state)
{
    struct i2c_client *client = chip->client;
    u8 *sh = chip->shadow;

    /* Turning on ALS */
    if (state) {
        chip->shadow[TSL2572_REG_ATIME] = chip->params.als_time;
        tsl2572_calc_cpl(chip);

        /* set PERS.apers to 2 consecutive ALS values out of range */
        chip->shadow[TSL2572_REG_PERS] &= (~TSL2572_MASK_APERS);
        chip->shadow[TSL2572_REG_PERS] |= ((chip->params.persist << TSL2572_SHIFT_APERS) & TSL2572_MASK_APERS);

        tsl2572_flush_als_regs(chip);

        ams_i2c_modify(client, sh, TSL2572_REG_ENABLE,
	#ifndef CONFIG_LAB126
                TSL2572_AIEN | TSL2572_WEN | TSL2572_AEN | TSL2572_PON,
                TSL2572_AIEN | TSL2572_WEN | TSL2572_AEN | TSL2572_PON);
	#else
                /* Disable the wait time */
                TSL2572_AIEN | TSL2572_AEN | TSL2572_PON,
                TSL2572_AIEN | TSL2572_AEN | TSL2572_PON);
	#endif
        chip->als_enabled = true;
    } else {
        /* Disable ALS, Wait and ALS Interrupt */
        ams_i2c_modify(client, sh, TSL2572_REG_ENABLE,
                TSL2572_AIEN | TSL2572_WEN | TSL2572_AEN, 0);
        chip->als_enabled = false;

        /* If nothing else is enabled set PON = 0; */
        if (!(sh[TSL2572_REG_ENABLE] & TSL2572_EN_ALL))
            ams_i2c_modify(client, sh, TSL2572_REG_ENABLE,
                    TSL2572_PON, 0);
    }

    return 0;
}

static int tsl2572_set_als_gain(struct tsl2572_chip *chip, struct als_gain_t *gain)
{
    int rc;
    u8 ctrl_reg;
    u8 saved_enable;
    u8 cfg_reg;

    if (!gain) {
        dev_err(&chip->client->dev, "%s: bad als gain\n", __func__);
        return -EINVAL;
    }

    cfg_reg = ((gain->code & 0x1) << TSL2572_SHIFT_AGL); //retrieve AGL
    ctrl_reg = ((gain->code >> 1) & 0x3) << TSL2572_SHIFT_AGAIN; // retrieve AGAIN

    /*
     * Turn off ALS, so that new ALS gain value will take
     * effect at start of new integration cycle.
     * New ALS gain value will then be used in next lux calculation.
     */
    ams_i2c_read(chip->client, TSL2572_REG_ENABLE, &saved_enable);
    ams_i2c_write(chip->client, chip->shadow, TSL2572_REG_ENABLE, 0);
    rc = ams_i2c_modify(chip->client, chip->shadow, TSL2572_REG_CFG,
            TSL2572_MASK_AGL, cfg_reg);
    rc = ams_i2c_modify(chip->client, chip->shadow, TSL2572_REG_CTRL,
            TSL2572_MASK_AGAIN, ctrl_reg);
    ams_i2c_write(chip->client, chip->shadow, TSL2572_REG_ENABLE,
            saved_enable);

    if (rc >= 0)
        chip->params.als_gain = gain->q_factor;

    return rc;
}

static int tsl2572_at_min_gain(struct tsl2572_chip *chip)
{
    return chip->params.als_gain == als_gains[0].q_factor;
}

static void tsl2572_inc_gain(struct tsl2572_chip *chip)
{
    int rc;
    u8 gain_code = tsl2572_current_gain_code(chip);
    struct als_gain_t gain_t;
    int i;

    gain_t = tsl2572_gain_code_to_gain_t(chip, gain_code);

    /*Check if already at max gain*/
    if (gain_t.code == als_gains[ARRAY_SIZE(als_gains) - 1].code)
        return;

    tsl2572_configure_als_mode(chip, 0);

    for ( i = 0; i < ARRAY_SIZE(als_gains); i++ ) {
        if (als_gains[i].code == gain_code)
            break;
    }

    //increment gain
    i += 1;
    dev_info(&chip->client->dev, "%s: AUTOGAIN INC to %s\n", __func__,
            als_gains[i].str);

    rc = tsl2572_set_als_gain(chip, &als_gains[i]);
    if (rc == 0)
        tsl2572_calc_cpl(chip);

    tsl2572_configure_als_mode(chip, 1);
}

static void tsl2572_dec_gain(struct tsl2572_chip *chip)
{
    int rc;
    u8 gain_code = tsl2572_current_gain_code(chip);
    struct als_gain_t gain_t;
    int i;

    gain_t = tsl2572_gain_code_to_gain_t(chip, gain_code);

    /*Check if already at min gain*/
    if (tsl2572_at_min_gain(chip))
        return;

    tsl2572_configure_als_mode(chip, 0);

    for ( i = ARRAY_SIZE(als_gains) - 1; i >= 0; i-- ) {
        if (als_gains[i].code == gain_code)
            break;
    }

    //decrement gain
    i -= 1;
    dev_info(&chip->client->dev, "%s: AUTOGAIN DEC to %s\n", __func__, 
            als_gains[i].str);

    rc = tsl2572_set_als_gain(chip, &als_gains[i]);
    if (rc == 0)
        tsl2572_calc_cpl(chip);

    tsl2572_configure_als_mode(chip, 1);
}

int tsl2572_get_lux(struct tsl2572_chip *chip)
{
    u16 ch0;
    u16 ch1;
    s32 lux1;
    s32 lux2;
    s32 lux;
    struct tsl2572_lux_segment *pls = &chip->params.lux_segment[0];
    s32 coefa = pls[0].ch0_coef;
    s32 coefb = pls[0].ch1_coef;
    s32 coefc = pls[1].ch0_coef;
    s32 coefd = pls[1].ch1_coef;
    u32 cpl;
    s32 _lux1, _lux2;
    u32 low_thrs = tsl2572_max_als_value(chip) / 200;

    ch0 = chip->als_inf.als_ch0;
    ch1 = chip->als_inf.als_ch1;
    tsl2572_calc_cpl(chip);
    cpl = chip->als_inf.cpl;

    /*
     * Lux1 = 1000 * ((coefa * ch0) - (coefb * ch1)) / cpl;
     * Lux2 = 1000 * ((coefc * ch0) - (coefd * ch1)) / cpl;
     * Lux = Max(Lux1,Lux2,0)
     */

    _lux1 = ((coefa * (s32)ch0) - (coefb * (s32)ch1));
    _lux2 = ((coefc * (s32)ch0) - (coefd * (s32)ch1));

    //lux1 = (s32)div64_s64(_lux1, (s64)cpl);
    //lux2 = (s32)div64_s64(_lux2, (s64)cpl);
    lux1 = _lux1 /= cpl;
    lux2 = _lux2 /= cpl;
    lux = max(lux1, lux2);
    lux = min(TSL2572_MAX_LUX, max(0, lux));
    chip->is_als_valid = 1;

    if (lux <= 0) {
        chip->als_inf.lux = 0;
        chip->is_als_valid = 0;
        return chip->als_inf.lux;
    }

    /* ..... Not in Autogain ........... */
    if (!chip->params.als_gain_auto) {
        if ((ch0 <= TSL2572_MIN_ALS_VALUE) ||
            (ch1 <= TSL2572_MIN_ALS_VALUE)) {
            chip->als_inf.lux = 0;
            chip->is_als_valid = 0;
            return 1;
        }
        if ((ch0 >= chip->als_inf.saturation) ||
            (ch1 >= chip->als_inf.saturation) ||
            (chip->in_asat == 1)) {
            chip->als_inf.lux = 65535;
            chip->is_als_valid = 0;
            return 1;
        }
    }
    /* This can run multiple times on the same data if get_lux is called from separate points
     * (i.e. from interrupt and from sysfs file)
     * This can cause a loop on saturation where it continuously
     * switches between min gain and saturation point. */
    /* ..... In Autogain ..........*/
    if (chip->params.als_gain_auto) {
        if ((ch0 < low_thrs && (ch1 < chip->als_inf.saturation/4)) ||
                (ch1 < low_thrs && (ch0 < chip->als_inf.saturation/4))) {
            if (chip->shadow[TSL2572_REG_STATUS]) {
                tsl2572_inc_gain(chip);
                tsl2572_flush_als_regs(chip);
            }
            chip->is_als_valid = 0;
        }else if ((ch0 > chip->als_inf.saturation) ||
                (ch1 > chip->als_inf.saturation) ||
                (chip->in_asat == 1)) {

            /* Gain at lowest and still sat? */
            chip->is_als_valid = 0;
            if (tsl2572_at_min_gain(chip)) {
                chip->als_inf.lux = 65535;
                return 1;
            } else if (chip->shadow[TSL2572_REG_STATUS])
                tsl2572_dec_gain(chip);
            /* not yet lowest gain */
            tsl2572_flush_als_regs(chip);
        }
    }

    /* always capture the computed lux
     * unless saturated.
     * Channel data always available.
     */
    chip->als_inf.lux = (u16)lux;
    return 0;
}

int tsl2572_update_als_thres(struct tsl2572_chip *chip, bool on_enable)
{
    s32 ret;
    u16 deltap = chip->params.als_deltap;
    u16 from, to, cur;
    u16 saturation = chip->als_inf.saturation;

    cur = chip->als_inf.als_ch0;

    if (on_enable) {
        /* move deltap far away from current position to force an irq */
        from = to = cur > (saturation / 2) ? 0 : saturation;
    } else {
        deltap = cur * deltap / 100;
        if (!deltap)
            deltap = 1;

        if (cur > deltap)
            from = cur - deltap;
        else
            from = 0;

        if (cur < (saturation - deltap))
            to = cur + deltap;
        else
            to = saturation;
    }

    *((__le16 *) &chip->shadow[TSL2572_REG_AILTL]) = cpu_to_le16(from);
    *((__le16 *) &chip->shadow[TSL2572_REG_AIHTL]) = cpu_to_le16(to);

    dev_info(&chip->client->dev,
            "%s: low:%d  hi:%d, oe:%d cur:%d deltap:%d (%d) sat:%d\n",
            __func__, from, to, on_enable, cur, deltap,
            chip->params.als_deltap, saturation);

    ret = ams_i2c_reg_blk_write(chip->client, TSL2572_REG_AILTL,
            &chip->shadow[TSL2572_REG_AILTL],
            (TSL2572_REG_AIHTH - TSL2572_REG_AILTL) + 1);

    return (ret < 0) ? ret : 0;
}

void tsl2572_report_als(struct tsl2572_chip *chip)
{
    int lux;
    int rc;

    if (chip->a_idev) {
        rc = tsl2572_get_lux(chip);
        if (!rc) {
            lux = chip->als_inf.lux;
#ifdef CONFIG_LAB126
            tsl2572_xthresh_uev(lux);
#endif
            input_report_abs(chip->a_idev, ABS_MISC, lux);
            input_sync(chip->a_idev);
            tsl2572_update_als_thres(chip, 0);
        } else {
            tsl2572_update_als_thres(chip, 1);
        }
    }
}

/*
 * ABI Functions
 */

static ssize_t tsl2572_device_als_lux(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    u32 lux;

    AMS_MUTEX_LOCK(&chip->lock);

    tsl2572_read_als(chip);
    tsl2572_get_lux(chip);
    lux = chip->als_inf.lux;

    AMS_MUTEX_UNLOCK(&chip->lock);

    return snprintf(buf, PAGE_SIZE, "%d\n", lux);
}

static ssize_t tsl2572_als_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_enabled);
}

static ssize_t tsl2572_als_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t size)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    bool value;

    if (strtobool(buf, &value))
        return -EINVAL;

    if (value)
        tsl2572_configure_als_mode(chip, 1);
    else
        tsl2572_configure_als_mode(chip, 0);

    return size;
}

static ssize_t tsl2572_auto_gain_enable_show(
        struct device *dev, struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    return snprintf(buf, PAGE_SIZE, "%s\n",
            chip->params.als_gain_auto ? "auto" : "manual");
}

static ssize_t tsl2572_auto_gain_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t size)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    bool value;

    if (strtobool(buf, &value))
        return -EINVAL;

    if (value)
        chip->params.als_gain_auto = 1;
    else
        chip->params.als_gain_auto = 0;

    return size;
}

static ssize_t tsl2572_als_gain_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    u8 gain_code = tsl2572_current_gain_code(chip);;

    return snprintf(buf, PAGE_SIZE, "%s (%s)\n",
            tsl2572_gain_code_to_gain_t(chip, gain_code).str,
            chip->params.als_gain_auto ? "auto" : "manual");
}

static ssize_t tsl2572_als_gain_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t size)
{
    int rc = 0;
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    struct als_gain_t *gain_t = NULL;
    size_t count = strcspn(buf, "\n");

    gain_t = tsl2572_gain_str_to_gain_t(chip, buf, count);
    if( !gain_t ) {
        if ( strncmp(buf, "0", count) ) {
            dev_err(&chip->client->dev, "%s: gain not found %s\n",
                    __func__, buf);
            return -EINVAL;
        }
    }

    if ( gain_t ) {
        chip->params.als_gain_auto = 0;
        rc = tsl2572_set_als_gain(chip, gain_t);
        if (!rc)
            tsl2572_calc_cpl(chip);
    } else {
        chip->params.als_gain_auto = 1;
    }
    tsl2572_flush_als_regs(chip);

    return rc ? -EIO : size;

}

static ssize_t tsl2572_als_cpl_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    return snprintf(buf, PAGE_SIZE, "%u\n", chip->als_inf.cpl);
}

static ssize_t tsl2572_als_persist_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    u8 pers = 0;
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    ams_i2c_read(chip->client, TSL2572_REG_PERS, &pers);
    return snprintf(buf, PAGE_SIZE, "%d\n", (pers & TSL2572_MASK_APERS));
}

static ssize_t tsl2572_als_persist_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t size)
{
    long persist;
    int rc;
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    rc = kstrtoul(buf, 10, &persist);
    if (rc)
        return -EINVAL;

    AMS_MUTEX_LOCK(&chip->lock);
    chip->shadow[TSL2572_REG_PERS] &= ~TSL2572_MASK_APERS;
    chip->shadow[TSL2572_REG_PERS] |=
        (((u8)persist << TSL2572_SHIFT_APERS) & TSL2572_MASK_APERS);

    tsl2572_flush_als_regs(chip);

    AMS_MUTEX_UNLOCK(&chip->lock);
    return size;
}

static ssize_t tsl2572_als_atime_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    int t;

    t = 256 - chip->shadow[TSL2572_REG_ATIME];
    t *= INTEGRATION_CYCLE;
    return snprintf(buf, PAGE_SIZE, "%dms (%dus)\n", t / 1000, t);
}

static ssize_t tsl2572_als_atime_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t size)
{
    long itime;
    int rc;
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    rc = kstrtoul(buf, 10, &itime);
    if (rc)
        return -EINVAL;
    if (itime < 3)
        itime = 3; /*since actual is 2.x */

    itime = AW_TIME_MS(itime);

    AMS_MUTEX_LOCK(&chip->lock);
    chip->shadow[TSL2572_REG_ATIME] = (u8) itime;
    chip->params.als_time = chip->shadow[TSL2572_REG_ATIME];
    tsl2572_calc_cpl(chip);
    tsl2572_flush_als_regs(chip);

    AMS_MUTEX_UNLOCK(&chip->lock);

    return size;
}

static ssize_t tsl2572_als_wtime_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int t;
    u8 wlongcurr;
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    AMS_MUTEX_LOCK(&chip->lock);

    t = 256 - chip->shadow[TSL2572_REG_WTIME];
    wlongcurr = chip->shadow[TSL2572_REG_CFG] & TSL2572_MASK_WLONG;
    if (wlongcurr)
        t *= 12;

    t *= INTEGRATION_CYCLE;
    t /= 1000;

    AMS_MUTEX_UNLOCK(&chip->lock);

    return snprintf(buf, PAGE_SIZE, "%d (in ms)\n", t);
}

static ssize_t tsl2572_als_wtime_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    unsigned long wtime;
    int wlong;
    int rc;

    rc = kstrtoul(buf, 10, &wtime);
    if (rc)
        return -EINVAL;

    if (wtime > (256 * INTEGRATION_CYCLE)) {
        wlong = 1;
        wtime -= (256 * INTEGRATION_CYCLE);
    } else {
        wlong = 0;
    }

    AMS_MUTEX_LOCK(&chip->lock);

    chip->shadow[TSL2572_REG_WTIME] = (u8) AW_TIME_MS(wtime);
    if (wlong)
        chip->shadow[TSL2572_REG_CFG] |= TSL2572_MASK_WLONG;
    else
        chip->shadow[TSL2572_REG_CFG] &= ~TSL2572_MASK_WLONG;

    tsl2572_flush_als_regs(chip);

    AMS_MUTEX_UNLOCK(&chip->lock);
    return size;
}

static ssize_t tsl2572_als_deltap_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    return snprintf(buf, PAGE_SIZE,
            "%d (in %%)\n", chip->params.als_deltap);
}

static ssize_t tsl2572_als_deltap_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t size)
{
    unsigned long deltap;
    int rc;
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    rc = kstrtoul(buf, 10, &deltap);
    if (rc || deltap > 100)
        return -EINVAL;
    AMS_MUTEX_LOCK(&chip->lock);
    chip->params.als_deltap = deltap;
    AMS_MUTEX_UNLOCK(&chip->lock);
    return size;
}

static ssize_t tsl2572_als_ch0_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.als_ch0);
}

static ssize_t tsl2572_als_ch1_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.als_ch1);
}

static ssize_t tsl2572_als_asat_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    return snprintf(buf, PAGE_SIZE, "%d\n", chip->in_asat);
}

static ssize_t tsl2572_als_adc_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);

    tsl2572_get_lux(chip);

    return snprintf(buf, PAGE_SIZE,
            "LUX: %d, CH0: %d, CH1:%d\n", chip->als_inf.lux,
            chip->als_inf.als_ch0, chip->als_inf.als_ch1);
}

static ssize_t tsl2572_als_adc_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t size)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    u32 ch0, ch1;

    if (2 != sscanf(buf, "%10d,%10d", &ch0, &ch1))
        return -EINVAL;

    AMS_MUTEX_LOCK(&chip->lock);

    chip->als_inf.als_ch0 = ch0;
    chip->als_inf.als_ch1 = ch1;

    AMS_MUTEX_UNLOCK(&chip->lock);
    return size;
}

static ssize_t tsl2572_als_valid_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct tsl2572_chip *chip = dev_get_drvdata(dev);
    int count;

    AMS_MUTEX_LOCK(&chip->lock);
    count =  snprintf(buf, PAGE_SIZE, "%d\n", chip->is_als_valid);
    AMS_MUTEX_UNLOCK(&chip->lock);
    return count;
}

struct device_attribute tsl2572_als_attrs[] = {
    __ATTR(als_atime, 0660, tsl2572_als_atime_show,
            tsl2572_als_atime_store),
    __ATTR(als_wtime, 0660, tsl2572_als_wtime_show,
            tsl2572_als_wtime_store),
    __ATTR(als_lux, 0440, tsl2572_device_als_lux, NULL),
    __ATTR(als_gain, 0660, tsl2572_als_gain_show, tsl2572_als_gain_store),
    __ATTR(als_cpl, 0440, tsl2572_als_cpl_show, NULL),
    __ATTR(als_thresh_deltap, 0660, tsl2572_als_deltap_show,
            tsl2572_als_deltap_store),
    __ATTR(als_auto_gain, 0660, tsl2572_auto_gain_enable_show,
            tsl2572_auto_gain_enable_store),
    __ATTR(als_enable, 0660, tsl2572_als_enable_show,
            tsl2572_als_enable_store),
    __ATTR(als_persist, 0660, tsl2572_als_persist_show,
            tsl2572_als_persist_store),
    __ATTR(als_ch0, 0440, tsl2572_als_ch0_show, NULL),
    __ATTR(als_ch1, 0440, tsl2572_als_ch1_show, NULL),
    __ATTR(als_asat, 0440, tsl2572_als_asat_show, NULL),
    __ATTR(als_adc, 0660, tsl2572_als_adc_show, tsl2572_als_adc_store),
    __ATTR(als_valid, 0440, tsl2572_als_valid_show, NULL),
};

int tsl2572_als_attrs_size = ARRAY_SIZE(tsl2572_als_attrs);

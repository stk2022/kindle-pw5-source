/*
 * linux/sound/soc/codecs/tlv320wn.h
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * v1.2: add adc-pos & ref-ch fields in dts.
 * v1.3: for gain dynamically config, register read/write
 * v1.4: remove define CONFIG_SND_SOC_4_ADCS in mt8167 Kconfig.
 *          add dmic-en field in dts for dmic_cfg. dmic_cfg:
 *          0 for Amic; 1 for Dmic.
 */

#ifndef _Tlv320_H
#define _Tlv320_H

#define AUDIO_NAME "tlv320aic3101"
#define TLV320_VERSION "1.6"
/*
 *    1.5 for Speaker audio data loopback by differential signal
 *         and Line-out audio data loopback by single-ended singnal
 *
 *    1.6 1:add enable adc pll function for tlv320 clock source
 *          2:add high pass fiter for dc offset @16k
 */
#define CONFIG_SND_SOC_4_ADCS

/* Microphone analog gain in half dB */
#define MIC_PGA_GAIN_L      0	/* 0dB Gain */
#define MIC_PGA_GAIN_R      0	/* 0dB Gain */

/* Differential input with 0db gain */
#define DIFF_MIC_INPUT_0db  0x3F

/* Default POV for ADC Input select register */
#define DEFAULT_PGA_INPUT_SEL 0x3F


enum {
	TLV320_PLL_ADC_FS_CLKIN_MCLK = 1,
	TLV320_PLL_ADC_FS_CLKIN_BCLK,
	TLV320_PLL_ADC_FS_CLKIN_PLL_MCLK,
	TLV320_PLL_ADC_FS_CLKIN_PLL_BCLK,
	TLV320_ADC_FS_CLKIN_BCLK,
};

enum single_ended_differential {
	SINGLE_ENDED_SIGNAL = 0,
	DIFFERENTIAL_SIGNAL,
};

/* AIC31xx supported sample rate are 8k to 48k */
#define TLV320_RATES   SNDRV_PCM_RATE_8000_48000

/* AIC31xx supports the word formats 16bits,24bits and 32 bits */
#define TLV320_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)



#define TLV320_WORD_LEN_16BITS		0x00
#define TLV320_WORD_LEN_24BITS		0x02
#define TLV320_WORD_LEN_32BITS		0x03



#ifdef CONFIG_SND_SOC_4_ADCS
#define NUM_ADC3101             4
#else
#define NUM_ADC3101             1
#endif

#define ADC3101_CACHEREGNUM     (128 + 128)
#define TLV320_CACHEREGNUM     (ADC3101_CACHEREGNUM * NUM_ADC3101)

/*
 * Creates the register using 8 bits for reg, 3 bits for device
 * and 7 bits for page. The extra 2 high order bits for page are
 * necessary for large pages, which may be sent in with the 32
 * bit reg value to the following functions where dev, page, and
 * reg no are properly masked out:
 * - aic31xx_write
 * - aic31xx_read_reg_cache
 * For ALSA calls (where the register is limited to 16bits), the
 * 5 bits for page is sufficient, and no high order bits will be
 * truncated.
 */
#define MAKE_REG(device, page, reg) \
	((u32) (((page) & 0x7f) << 11 | ((device) & 0x7) << 8 | ((reg) & 0x7f)))

/****************************/
/*          Page 0 Registers            */
/****************************/

/* Page select register */
#define ADC_PAGE_SELECT(cnum)         MAKE_REG((cnum), 0, 0)
/* Software reset register */
#define ADC_RESET(cnum)               MAKE_REG((cnum), 0, 1)

/* 2-3 Reserved */

/* PLL programming register B */
#define ADC_CLKGEN_MUX(cnum)          MAKE_REG((cnum), 0, 4)
#define TLV320_PLL_CLKIN_MASK				0xC  /* (0b00001100) */
#define TLV320_PLL_CLKIN_SHIFT				2
#define TLV320_PLL_CLKIN_MCLK				0x0
#define TLV320_PLL_CLKIN_BCLK				0x1
#define TLV320_CODEC_CLKIN_SHIFT			0
#define TLV320_CODEC_CLKIN_MCLK			0x0
#define TLV320_CODEC_CLKIN_BCLK			0x1
#define TLV320_CODEC_CLKIN_PLL				0x3

/* PLL P and R-Val */
#define ADC_PLL_PROG_PR(cnum)         MAKE_REG((cnum), 0, 5)
/* PLL J-Val */
#define ADC_PLL_PROG_J(cnum)          MAKE_REG((cnum), 0, 6)
/* PLL D-Val MSB */
#define ADC_PLL_PROG_D_MSB(cnum)      MAKE_REG((cnum), 0, 7)
/* PLL D-Val LSB */
#define ADC_PLL_PROG_D_LSB(cnum)      MAKE_REG((cnum), 0, 8)

/* 9-17 Reserved */

/* ADC NADC */
#define ADC_ADC_NADC(cnum)            MAKE_REG((cnum), 0, 18)
/* ADC MADC */
#define ADC_ADC_MADC(cnum)            MAKE_REG((cnum), 0, 19)
/* ADC AOSR */
#define ADC_ADC_AOSR(cnum)            MAKE_REG((cnum), 0, 20)
/* ADC IADC */
#define ADC_ADC_IADC(cnum)            MAKE_REG((cnum), 0, 21)
/* ADC miniDSP engine decimation */
#define ADC_MINIDSP_DECIMATION(cnum)   MAKE_REG((cnum), 0, 22)

/* 23-24 Reserved */

/* CLKOUT MUX */
#define ADC_CLKOUT_MUX(cnum)      MAKE_REG((cnum), 0, 25)
/* CLOCKOUT M divider value */
#define ADC_CLKOUT_M_DIV(cnum)      MAKE_REG((cnum), 0, 26)
/*Audio Interface Setting Register 1*/
#define ADC_INTERFACE_CTRL_1(cnum)     MAKE_REG((cnum), 0, 27)
/* Data Slot Offset (Ch_Offset_1) */
#define ADC_CH_OFFSET_1(cnum)      MAKE_REG((cnum), 0, 28)
/* ADC interface control 2 */
#define ADC_INTERFACE_CTRL_2(cnum)     MAKE_REG((cnum), 0, 29)
/* BCLK N Divider */
#define ADC_BCLK_N_DIV(cnum)      MAKE_REG((cnum), 0, 30)
/* Secondary audio interface control 1 */
#define ADC_INTERFACE_CTRL_3(cnum)     MAKE_REG((cnum), 0, 31)
/* Secondary audio interface control 2 */
#define ADC_INTERFACE_CTRL_4(cnum)     MAKE_REG((cnum), 0, 32)
/* Secondary audio interface control 3 */
#define ADC_INTERFACE_CTRL_5(cnum)     MAKE_REG((cnum), 0, 33)
/* I2S sync */
#define ADC_I2S_SYNC(cnum)      MAKE_REG((cnum), 0, 34)

/* 35 Reserved */

/* ADC flag register */
#define ADC_ADC_FLAG(cnum)      MAKE_REG((cnum), 0, 36)
/* Data slot offset 2 (Ch_Offset_2) */
#define ADC_CH_OFFSET_2(cnum)      MAKE_REG((cnum), 0, 37)
/* I2S TDM control register */
#define ADC_I2S_TDM_CTRL(cnum)      MAKE_REG((cnum), 0, 38)

/* 39-41 Reserved */

/* Interrupt flags (overflow) */
#define ADC_INTR_FLAG_1(cnum)      MAKE_REG((cnum), 0, 42)
/* Interrupt flags (overflow) */
#define ADC_INTR_FLAG_2(cnum)      MAKE_REG((cnum), 0, 43)

/* 44 Reserved */

/* Interrupt flags ADC */
#define ADC_INTR_FLAG_ADC1(cnum)      MAKE_REG((cnum), 0, 45)

/* 46 Reserved */

/* Interrupt flags ADC */
#define ADC_INTR_FLAG_ADC2(cnum)      MAKE_REG((cnum), 0, 47)
/* INT1 interrupt control */
#define ADC_INT1_CTRL(cnum)      MAKE_REG((cnum), 0, 48)
/* INT2 interrupt control */
#define ADC_INT2_CTRL(cnum)      MAKE_REG((cnum), 0, 49)

/* 50 Reserved */

/* DMCLK/GPIO2 control */
#define ADC_GPIO2_CTRL(cnum)      MAKE_REG((cnum), 0, 51)
/* DMDIN/GPIO1 control */
#define ADC_GPIO1_CTRL(cnum)      MAKE_REG((cnum), 0, 52)
/* DOUT Control */
#define ADC_DOUT_CTRL(cnum)      MAKE_REG((cnum), 0, 53)

/* 54-56 Reserved */

/* ADC sync control 1 */
#define ADC_SYNC_CTRL_1(cnum)      MAKE_REG((cnum), 0, 57)
/* ADC sync control 2 */
#define ADC_SYNC_CTRL_2(cnum)      MAKE_REG((cnum), 0, 58)
/* ADC CIC filter gain control */
#define ADC_CIC_GAIN_CTRL(cnum)      MAKE_REG((cnum), 0, 59)

/* 60 Reserved */

/* ADC processing block selection  */
#define ADC_PRB_SELECT(cnum)      MAKE_REG((cnum), 0, 61)
/* Programmable instruction mode control bits */
#define ADC_INST_MODE_CTRL(cnum)      MAKE_REG((cnum), 0, 62)

/* 63-79 Reserved */

/* Digital microphone polarity control */
#define ADC_MIC_POLARITY_CTRL(cnum)    MAKE_REG((cnum), 0, 80)
/* ADC Digital */
#define ADC_ADC_DIGITAL(cnum)          MAKE_REG((cnum), 0, 81)
/* ADC Fine Gain Adjust */
#define ADC_ADC_FGA(cnum)              MAKE_REG((cnum), 0, 82)
/* Left ADC Channel Volume Control */
#define ADC_LADC_VOL(cnum)             MAKE_REG((cnum), 0, 83)
/* Right ADC Channel Volume Control */
#define ADC_RADC_VOL(cnum)             MAKE_REG((cnum), 0, 84)
/* ADC phase compensation */
#define ADC_ADC_PHASE_COMP(cnum)       MAKE_REG((cnum), 0, 85)
/* Left Channel AGC Control Register 1 */
#define ADC_LEFT_CHN_AGC_1(cnum)       MAKE_REG((cnum), 0, 86)
/* Left Channel AGC Control Register 2 */
#define ADC_LEFT_CHN_AGC_2(cnum)       MAKE_REG((cnum), 0, 87)
/* Left Channel AGC Control Register 3 */
#define ADC_LEFT_CHN_AGC_3(cnum)       MAKE_REG((cnum), 0, 88)
/* Left Channel AGC Control Register 4 */
#define ADC_LEFT_CHN_AGC_4(cnum)       MAKE_REG((cnum), 0, 89)
/* Left Channel AGC Control Register 5 */
#define ADC_LEFT_CHN_AGC_5(cnum)       MAKE_REG((cnum), 0, 90)
/* Left Channel AGC Control Register 6 */
#define ADC_LEFT_CHN_AGC_6(cnum)       MAKE_REG((cnum), 0, 91)
/* Left Channel AGC Control Register 7 */
#define ADC_LEFT_CHN_AGC_7(cnum)       MAKE_REG((cnum), 0, 92)
/* Left AGC gain */
#define ADC_LEFT_AGC_GAIN(cnum)        MAKE_REG((cnum), 0, 93)
/* Right Channel AGC Control Register 1 */
#define ADC_RIGHT_CHN_AGC_1(cnum)      MAKE_REG((cnum), 0, 94)
/* Right Channel AGC Control Register 2 */
#define ADC_RIGHT_CHN_AGC_2(cnum)      MAKE_REG((cnum), 0, 95)
/* Right Channel AGC Control Register 3 */
#define ADC_RIGHT_CHN_AGC_3(cnum)      MAKE_REG((cnum), 0, 96)
/* Right Channel AGC Control Register 4 */
#define ADC_RIGHT_CHN_AGC_4(cnum)      MAKE_REG((cnum), 0, 97)
/* Right Channel AGC Control Register 5 */
#define ADC_RIGHT_CHN_AGC_5(cnum)      MAKE_REG((cnum), 0, 98)
/* Right Channel AGC Control Register 6 */
#define ADC_RIGHT_CHN_AGC_6(cnum)      MAKE_REG((cnum), 0, 99)
/* Right Channel AGC Control Register 7 */
#define ADC_RIGHT_CHN_AGC_7(cnum)      MAKE_REG((cnum), 0, 100)
/* Right AGC gain */
#define ADC_RIGHT_AGC_GAIN(cnum)       MAKE_REG((cnum), 0, 101)

/* 102-127 Reserved */

/****************************************************************************/
/*                           Page 1 Registers                               */
/****************************************************************************/
#define ADC_PAGE_1                    128

/* 1-25 Reserved */

/* Dither control */
#define ADC_DITHER_CTRL(cnum)          MAKE_REG((cnum), 1, 26)

/* 27-50 Reserved */

/* MICBIAS Configuration Register */
#define ADC_MICBIAS_CTRL(cnum)         MAKE_REG((cnum), 1, 51)
/* Left ADC input selection for Left PGA */
#define ADC_LEFT_PGA_SEL_1(cnum)       MAKE_REG((cnum), 1, 52)

/* 53 Reserved */

/* Right ADC input selection for Left PGA */
#define ADC_LEFT_PGA_SEL_2(cnum)       MAKE_REG((cnum), 1, 54)
/* Right ADC input selection for right PGA */
#define ADC_RIGHT_PGA_SEL_1(cnum)      MAKE_REG((cnum), 1, 55)

/* 56 Reserved */

/* Right ADC input selection for right PGA */
#define ADC_RIGHT_PGA_SEL_2(cnum)      MAKE_REG((cnum), 1, 57)

/* 58 Reserved */

/* Left analog PGA settings */
#define ADC_LEFT_APGA_CTRL(cnum)       MAKE_REG((cnum), 1, 59)
/* Right analog PGA settings */
#define ADC_RIGHT_APGA_CTRL(cnum)      MAKE_REG((cnum), 1, 60)
/* ADC Low current Modes */
#define ADC_LOW_CURRENT_MODES(cnum)    MAKE_REG((cnum), 1, 61)
/* ADC analog PGA flags */
#define ADC_ANALOG_PGA_FLAGS(cnum)     MAKE_REG((cnum), 1, 62)

/* 63-127 Reserved */

/* PLL Enable bits */
#define ADC_ENABLE_NADC             0x80
#define ADC_ENABLE_MADC             0x80
#define ADC_ENABLE_BCLK             0x80

/* Power bits */
#define ADC_LADC_PWR_ON             D7
#define ADC_RADC_PWR_ON             D6

#define ADC_SOFT_RESET              D0
#define ADC_BCLK_MASTER             D3
#define ADC_WCLK_MASTER             D2



#define EARLY_3STATE_ENABLED        0x02
#define TIME_SOLT_MODE              0x01

/* AIC31XX_IFACE2 */
#define TLV320_BCLKINV_MASK		0x08
#define TLV320_BDIVCLK_MASK		0x03
#define TLV320_DAC2BCLK		0x00
#define TLV320_DACMOD2BCLK		0x01
#define TLV320_ADC2BCLK		0x02
#define TLV320_ADCMOD2BCLK		0x03

/* PLL P/R bit offsets */
#define ADC_PLLP_SHIFT          4
#define ADC_PLLR_SHIFT          0
#define ADC_PLL_PR_MASK         0x7F
#define ADC_PLLJ_MASK           0x3F
#define ADC_PLLD_MSB_MASK       0x3F
#define ADC_PLLD_LSB_MASK       0xFF
#define ADC_NADC_MASK           0x7F
#define ADC_MADC_MASK           0x7F
#define ADC_AOSR_MASK           0xFF
#define ADC_IADC_MASK           0xFF
#define ADC_BDIV_MASK           0x7F

/* PLL_CLKIN bits */
#define ADC_PLL_CLKIN_SHIFT         2
#define ADC_PLL_CLKIN_MCLK          0x0
#define ADC_PLL_CLKIN_BCLK          0x1
#define ADC_PLL_CLKIN_ZERO          0x3

/* CODEC_CLKIN bits */
#define ADC_CODEC_CLKIN_SHIFT       0
#define ADC_CODEC_CLKIN_MCLK        0x0
#define ADC_CODEC_CLKIN_BCLK        0x1
#define ADC_CODEC_CLKIN_PLL_CLK     0x3
#define ADC_PLL_POWER_UP            0x80

#define ADC_USE_PLL     ((ADC_PLL_CLKIN_MCLK << ADC_PLL_CLKIN_SHIFT) | \
	(ADC_CODEC_CLKIN_PLL_CLK << ADC_CODEC_CLKIN_SHIFT))

#define TLV320_IFACE1_DATALEN_MASK	0x30
#define TLV320_IFACE1_DATALEN_SHIFT	(4)
#define TLV320_IFACE1_DATATYPE_MASK	0xC0
#define TLV320_IFACE1_DATATYPE_SHIFT	(6)
/* Serial data bus uses I2S mode (Default mode) */
#define TLV320_I2S_MODE		0x00
#define TLV320_DSP_MODE		0x01
#define TLV320_RIGHT_JUSTIFIED_MODE	0x02
#define TLV320_LEFT_JUSTIFIED_MODE	0x03

#define TLV320_IFACE1_MASTER_MASK	0x0C
#define TLV320_IFACE1_MASTER_SHIFT	(2)
#define TLV320_BCLK_MASTER	0x08
#define TLV320_WCLK_MASTER	0x04

#define TLV320_IFACE1_3STATES_MASK      0x01
#define TLV320_IFACE1_3STATES_SHIFT     (0)
#define TLV320_3STATES		0x01

#define TLV320_DATA_OFFSET_MASK	0xFE
#define TLV320_BCLKINV_MASK		0x08
#define TLV320_BCLKEN_MASK		0x04
#define TLV320_BDIVCLK_MASK		0x03
#define TLV320_BCLK_WCLK_ACTIVE	0x04

#define TLV320_DAC2BCLK		0x00
#define TLV320_DACMOD2BCLK		0x01
#define TLV320_ADC2BCLK		0x02
#define TLV320_ADCMOD2BCLK		0x03

#define TLV320_BCLK_N_POWER_MASK       0x80
#define TLV320_CLKOUT_N_MASK		0x7f
#define TLV320_ADC_PRB_MASK		0x7f


/*
 *****************************************************************************
 * Structures Definitions
 *****************************************************************************
 */
struct tlv320_priv {
	u8 adc_page_no[4];
	struct i2c_client *adc_control_data[5];
	struct mutex codecMutex;
	struct gpio_desc *reset_gpiod;
	int adc_pos;
	int ref_ch;
	int adc_num;
	int dmic_cfg;/*0:for Amic, 1:for Dmic*/
	int single_difference; /*0:for single-ended, 1:for differential*/
	u32 freq_in;
	u32 freq_out;
	u8 dsp_a_val;
	u32 fmt;
	u32  dither;
	u32  dither_adcno;
};

struct tlv320_HP_configs {
	u32 reg_offset;
	u8 reg_val;
};

/****************** RATES TABLE FOR ADC31xx ************************/
struct tlv320_rate_divs {
	u32 mclk;
	u32 rate;
	u8 pll_p;
	u8 pll_r;
	u8 pll_j;
	u16 pll_d;
	u8 nadc;
	u8 madc;
	u8 aosr;
	u8 bdiv_n;
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *          It is SoC Codec DAI structure which has DAI capabilities viz.,
 *          playback and capture, DAI runtime information viz. state of DAI
 *          and pop wait state, and DAI private data.
 *----------------------------------------------------------------------------
 */
extern struct snd_soc_dai tlv320aic3101_dai;

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic31xx_probe(), aic31xx_remove(),
 *          aic31xx_suspend() and aic31xx_resume()
 *
 */
extern struct snd_soc_codec_device soc_codec_dev_aic31xx;

#endif              /* _Tlv320aic3101_H */

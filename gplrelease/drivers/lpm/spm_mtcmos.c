#include <common.h>
#include "pwr_ctrl.h"
#include "spm_mtcmos.h"

#define SPM_BASE		(0x10006000)

#define INFRA_TOPAXI_PROTECTEN              (INFRACFG_AO_BASE + 0x220)
#define INFRA_TOPAXI_PROTECTEN_SET          (INFRACFG_AO_BASE + 0x2A0)
#define INFRA_TOPAXI_PROTECTEN_CLR          (INFRACFG_AO_BASE + 0x2A4)
#define INFRA_TOPAXI_PROTECTEN_STA0         (INFRACFG_AO_BASE + 0x224)
#define INFRA_TOPAXI_PROTECTEN_STA1         (INFRACFG_AO_BASE + 0x228)
#define INFRA_TOPAXI_PROTECTEN_1            (INFRACFG_AO_BASE + 0x250)
#define INFRA_TOPAXI_PROTECTEN_1_SET        (INFRACFG_AO_BASE + 0x2A8)
#define INFRA_TOPAXI_PROTECTEN_1_CLR        (INFRACFG_AO_BASE + 0x2AC)
#define INFRA_TOPAXI_PROTECTEN_STA0_1       (INFRACFG_AO_BASE + 0x254)
#define INFRA_TOPAXI_PROTECTEN_STA1_1       (INFRACFG_AO_BASE + 0x258)
#define INFRA_TOPAXI_PROTECTEN_2            (INFRACFG_AO_BASE + 0x420)
#define INFRA_TOPAXI_PROTECTEN_2_SET        (INFRACFG_AO_BASE + 0x42C)
#define INFRA_TOPAXI_PROTECTEN_2_CLR        (INFRACFG_AO_BASE + 0x430)
#define INFRA_TOPAXI_PROTECTEN_STA0_2       (INFRACFG_AO_BASE + 0x424)
#define INFRA_TOPAXI_PROTECTEN_STA1_2       (INFRACFG_AO_BASE + 0x428)
#define PERI_BUS_DCM_CTRL		    (INFRACFG_AO_BASE + 0x74)
#define AUDIO_BUS_AUD_SI0                   (INFRACFG_AO_BASE + 0x800)
#define AUDIO_BUS_INFRA_SI0                 (INFRACFG_AO_BASE + 0x808)

#define POWERON_CONFIG_EN	   (SPM_BASE + 0x000)
#define CONN_PWR_CON           (SPM_BASE + 0x32C)
#define MM_PWR_CON             (SPM_BASE + 0x374)
#define IMG_PWR_CON            (SPM_BASE + 0x38C)
#define IP0_PWR_CON            (SPM_BASE + 0x39C)
#define IP1_PWR_CON            (SPM_BASE + 0x384)
#define IP2_PWR_CON            (SPM_BASE + 0x388)
#define USB_MAC_P1_PWR_CON     (SPM_BASE + 0x3A4)
#define AUDIO_PWR_CON          (SPM_BASE + 0x314)
#define ASRC_PWR_CON           (SPM_BASE + 0x328)
#define DSP_PWR_CON            (SPM_BASE + 0x37C)
#define PWR_STATUS             (SPM_BASE + 0x180)
#define PWR_STATUS_2ND         (SPM_BASE + 0x184)

#define SPM_PROJECT_CODE    0xb16

/* Define MTCMOS power control */
#define PWR_RST_B                        (0x1 << 0)
#define PWR_ISO                          (0x1 << 1)
#define PWR_ON                           (0x1 << 2)
#define PWR_ON_2ND                       (0x1 << 3)
#define PWR_CLK_DIS                      (0x1 << 4)

/* Define MTCMOS Bus Protect Mask */
#define DIS_PROT_STEP1_0_MASK            ((0x1 << 16) |(0x1 << 17))
#define DIS_PROT_STEP1_0_ACK_MASK        ((0x1 << 16) |(0x1 << 17))
#define DIS_PROT_STEP2_0_MASK            ((0x1 << 1) |(0x1 << 2) |(0x1 << 10) |(0x1 << 11))
#define DIS_PROT_STEP2_0_ACK_MASK        ((0x1 << 1) |(0x1 << 2) |(0x1 << 10) |(0x1 << 11))

//STEP0 -> SLV port, STEP1 -> MST port
//AFE
#define AFE_PROT_STEP0_0_MASK            ((0x1 << 28))
#define AFE_PROT_STEP0_0_ACK_MASK        ((0x1 << 28))

#define AFE_PROT_STEP1_0_MASK            ((0x1 << 22))
#define AFE_PROT_STEP1_0_ACK_MASK        ((0x1 << 22))

#define AFE_CLK_DCM_EN                   ((0x1 << 29))

//SRC
#define SRC_PROT_STEP1_0_MASK            ((0x1 << 21))
#define SRC_PROT_STEP1_0_ACK_MASK        ((0x1 << 21))

//MM
#define MM_PROT_STEP0_0_MASK            ((0x1 << 15))
#define MM_PROT_STEP0_0_ACK_MASK        ((0x1 << 15))

#define MM_PROT_STEP1_00_MASK            ((0x1 << 16) |(0x1 << 17))
#define MM_PROT_STEP1_00_ACK_MASK        ((0x1 << 16) |(0x1 << 17))

#define MM_PROT_STEP1_10_MASK            ((0x1 <<  8) |(0x1 <<  9) |(0x1 << 10) |(0x1 << 11))

#define MM_PROT_STEP1_10_ACK_MASK        ((0x1 <<  8) |(0x1 <<  9) |(0x1 << 10) |(0x1 << 11))
//IP0(NNA1)
#define NNA1_PROT_STEP0_0_MASK            ((0x1 << 4))
#define NNA1_PROT_STEP0_0_ACK_MASK        ((0x1 << 4))

#define NNA1_PROT_STEP1_0_MASK            ((0x1 << 2))
#define NNA1_PROT_STEP1_0_ACK_MASK        ((0x1 << 2))

//IP1(WFST)
#define WFST_PROT_STEP0_0_MASK            ((0x1 << 5))
#define WFST_PROT_STEP0_0_ACK_MASK        ((0x1 << 5))

#define WFST_PROT_STEP1_0_MASK            ((0x1 << 3))
#define WFST_PROT_STEP1_0_ACK_MASK        ((0x1 << 3))

//IP2(NNA0)
#define NNA0_PROT_STEP0_0_MASK            ((0x1 << 12))
#define NNA0_PROT_STEP0_0_ACK_MASK        ((0x1 << 12))

#define NNA0_PROT_STEP1_00_MASK            ((0x1 << 5) |(0x1 << 6) |(0x1 << 7))
#define NNA0_PROT_STEP1_00_ACK_MASK        ((0x1 << 5) |(0x1 << 6) |(0x1 << 7))
#define NNA0_PROT_STEP1_10_MASK            ((0x1 << 14)  |(0x1 << 18) |(0x1 << 16))
#define NNA0_PROT_STEP1_10_ACK_MASK        ((0x1 << 14) |(0x1 << 18) |(0x1 << 16))

//DSP
#define DSP_PROT_STEP1_00_MASK            ((0x1 << 1) |(0x1 << 24))
#define DSP_PROT_STEP1_00_ACK_MASK        ((0x1 << 1) |(0x1 << 24))

#define DSP_PROT_STEP1_10_MASK            ((0x1 << 7) |(0x1 << 10) |(0x1 << 11))
#define DSP_PROT_STEP1_10_ACK_MASK        ((0x1 << 7) |(0x1 << 10) |(0x1 << 11))

//CONN
#define CONN_PROT_STEP0_0_MASK           ((0x1 << 13))
#define CONN_PROT_STEP0_0_ACK_MASK       ((0x1 << 13))

#define CONN_PROT_STEP1_00_MASK           ((0x1 << 18))
#define CONN_PROT_STEP1_00_ACK_MASK       ((0x1 << 18))
#define CONN_PROT_STEP1_10_MASK           ((0x1 << 14))
#define CONN_PROT_STEP1_10_ACK_MASK       ((0x1 << 14))

//USB
#define USB_MAC_PROT_STEP0_0_MASK            ((0x1 << 0) |(0x1 << 1))
#define USB_MAC_PROT_STEP0_0_ACK_MASK        ((0x1 << 0) |(0x1 << 1))

/* Define MTCMOS Power Status Mask */

#define CONN_PWR_STA_MASK                (0x1 << 1)
#define MM_PWR_STA_MASK                  (0x1 << 15)
#define IMG_PWR_STA_MASK                 (0x1 << 16)
#define DSP_PWR_STA_MASK                 (0x1 << 17)
#define USB_MAC_P1_PWR_STA_MASK          (0x1 << 20)
#define ASRC_PWR_STA_MASK                (0x1 << 23)
#define AUDIO_PWR_STA_MASK               (0x1 << 24)
#define IP0_PWR_STA_MASK                 (0x1 << 25)
#define IP1_PWR_STA_MASK                 (0x1 << 26)
#define IP2_PWR_STA_MASK                 (0x1 << 27)

/* Define Non-CPU SRAM Mask */
#define MM_SRAM_PDN                      (0x1 << 8)
#define MM_SRAM_PDN_ACK                  (0x1 << 12)
#define MM_SRAM_PDN_ACK_BIT0             (0x1 << 12)
#define IMG_SRAM_PDN                     (0x1 << 8)
#define IMG_SRAM_PDN_ACK                 (0x1 << 12)
#define IMG_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define DSP_SRAM_PDN                     (0xF << 8)
#define DSP_SRAM_PDN_ACK                 (0xF << 12)
#define DSP_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define DSP_SRAM_PDN_ACK_BIT1            (0x1 << 13)
#define DSP_SRAM_PDN_ACK_BIT2            (0x1 << 14)
#define DSP_SRAM_PDN_ACK_BIT3            (0x1 << 15)
#define IP0_SRAM_PDN                     (0x1 << 8)
#define IP0_SRAM_PDN_ACK                 (0x1 << 12)
#define IP0_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define IP1_SRAM_PDN                     (0x1 << 8)
#define IP1_SRAM_PDN_ACK                 (0x1 << 12)
#define IP1_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define IP2_SRAM_PDN                     (0x1 << 8)
#define IP2_SRAM_PDN_ACK                 (0x1 << 12)
#define IP2_SRAM_PDN_ACK_BIT0            (0x1 << 12)
#define USB_SRAM_PDN                     (0x1 << 8)
#define USB_SRAM_PDN_ACK                 (0x1 << 12)
#define AUDIO_SRAM_PDN                   (0xF << 8)
#define AUDIO_SRAM_PDN_ACK               (0xF << 13)
#define AUDIO_SRAM_PDN_ACK_BIT0          (0x1 << 13)
#define AUDIO_SRAM_PDN_ACK_BIT1          (0x1 << 14)
#define AUDIO_SRAM_PDN_ACK_BIT2          (0x1 << 15)
#define AUDIO_SRAM_PDN_ACK_BIT3          (0x1 << 16)
#define ASRC_SRAM_PDN                    (0x1 << 8)
#define ASRC_SRAM_PDN_ACK                (0x1 << 12)
#define ASRC_SRAM_PDN_ACK_BIT0           (0x1 << 12)

#define spm_write(addr, value) writel(value,addr)
#define spm_read(addr) readl(addr)

#define IGNORE_MTCMOS_CHECK

int spm_mtcmos_ctrl_conn(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off CONN" */
		/* TINFO="Set bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & CONN_PROT_STEP0_0_ACK_MASK) != CONN_PROT_STEP0_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step1 : 00" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, CONN_PROT_STEP1_00_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) & CONN_PROT_STEP1_00_ACK_MASK) != CONN_PROT_STEP1_00_ACK_MASK) {
		}
#endif

		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, ((0x1 << 21)));
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) & (0x1 << 21)) != (0x1 << 21)) {
		}
#endif

		/* TINFO="Set bus protect - step1 : 10" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, CONN_PROT_STEP1_10_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & CONN_PROT_STEP1_10_ACK_MASK) != CONN_PROT_STEP1_10_ACK_MASK) {
		}
#endif
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off CONN" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on CONN" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & CONN_PWR_STA_MASK) != CONN_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & CONN_PWR_STA_MASK) != CONN_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(CONN_PWR_CON, spm_read(CONN_PWR_CON) | PWR_RST_B);
#ifndef IGNORE_MTCMOS_CHECK
#endif
		/* TINFO="Release bus protect - step1 : 10" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CONN_PROT_STEP1_10_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & CONN_PROT_STEP1_10_MASK)) {
		}
#endif

		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, ((0x1 << 21)));
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) & (0x1 << 21))) {
		}
#endif

		/* TINFO="Release bus protect - step1 : 00" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, CONN_PROT_STEP1_00_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) & CONN_PROT_STEP1_00_MASK)) {
		}
#endif
		/* TINFO="Release bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, CONN_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & CONN_PROT_STEP0_0_MASK)) {
		}
#endif
		/* TINFO="Finish to turn on CONN" */
	}
	return err;
}

int spm_mtcmos_ctrl_mm(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off MM" */
		/* TINFO="enable mm gals in_ck    bit25 & bit24" */
	    spm_write(MMSYS_CG_CLR0, 0x03000000);	// & 0xFCFFFFFF;
		/* TINFO="Set bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, MM_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & MM_PROT_STEP0_0_ACK_MASK) != MM_PROT_STEP0_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step1 : 00" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_SET, MM_PROT_STEP1_00_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) & MM_PROT_STEP1_00_ACK_MASK) != MM_PROT_STEP1_00_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step1 : 10" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, MM_PROT_STEP1_10_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & MM_PROT_STEP1_10_ACK_MASK) != MM_PROT_STEP1_10_ACK_MASK) {
		}
#endif

		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) | MM_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MM_SRAM_PDN_ACK = 1" */
		while ((spm_read(MM_PWR_CON) & MM_SRAM_PDN_ACK) != MM_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & MM_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & MM_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off MM" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on MM" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & MM_PWR_STA_MASK) != MM_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & MM_PWR_STA_MASK) != MM_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(MM_PWR_CON, spm_read(MM_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until MM_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(MM_PWR_CON) & MM_SRAM_PDN_ACK_BIT0) {
				/*  */
		}
#endif
		/* TINFO="Release bus protect - step1 : 10" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, MM_PROT_STEP1_10_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & MM_PROT_STEP1_10_ACK_MASK)) {
		}
#endif
		/* TINFO="Release bus protect - step1 : 00" */
		spm_write(INFRA_TOPAXI_PROTECTEN_1_CLR, MM_PROT_STEP1_00_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_1) & MM_PROT_STEP1_00_ACK_MASK)) {
		}
#endif
		/* TINFO="Release bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, MM_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & MM_PROT_STEP0_0_ACK_MASK)) {
		}
#endif


		/* TINFO="Finish to turn on MM" */
	}
	return err;
}

int spm_mtcmos_ctrl_img(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off IMG" */
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) | IMG_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IMG_SRAM_PDN_ACK = 1" */
		while ((spm_read(IMG_PWR_CON) & IMG_SRAM_PDN_ACK) != IMG_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & IMG_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & IMG_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off IMG" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on IMG" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & IMG_PWR_STA_MASK) != IMG_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & IMG_PWR_STA_MASK) != IMG_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(IMG_PWR_CON, spm_read(IMG_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IMG_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(IMG_PWR_CON) & IMG_SRAM_PDN_ACK_BIT0) {
				/*  */
		}
#endif
		/* TINFO="Finish to turn on IMG" */
	}
	return err;
}

int spm_mtcmos_ctrl_ip0(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off IP0" */

		        /* TINFO="Set bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, NNA0_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & NNA0_PROT_STEP0_0_ACK_MASK) != NNA0_PROT_STEP0_0_ACK_MASK) {
		}
#endif
	        /* TINFO="Set bus protect - step1 : 00" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, NNA0_PROT_STEP1_00_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & NNA0_PROT_STEP1_00_ACK_MASK) != NNA0_PROT_STEP1_00_ACK_MASK) {
		}
#endif
	        /* TINFO="Set bus protect - step1 : 10" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, NNA0_PROT_STEP1_10_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & NNA0_PROT_STEP1_10_ACK_MASK) != NNA0_PROT_STEP1_10_ACK_MASK) {
		}
#endif

		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) | IP0_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IP0_SRAM_PDN_ACK = 1" */
		while ((spm_read(IP0_PWR_CON) & IP0_SRAM_PDN_ACK) != IP0_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & IP0_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & IP0_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off IP0" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on IP0" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & IP0_PWR_STA_MASK) != IP0_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & IP0_PWR_STA_MASK) != IP0_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(IP0_PWR_CON, spm_read(IP0_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IP0_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(IP0_PWR_CON) & IP0_SRAM_PDN_ACK_BIT0) {
				/*  */
		}
#endif
		/* TINFO="Release bus protect - step1 : 10" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, NNA0_PROT_STEP1_10_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & NNA0_PROT_STEP1_10_ACK_MASK)) {
		}
#endif
		/* TINFO="Release bus protect - step1 : 00" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, NNA0_PROT_STEP1_00_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & NNA0_PROT_STEP1_00_ACK_MASK)) {
		}

#endif
		/* TINFO="Release bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, NNA0_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & NNA0_PROT_STEP0_0_ACK_MASK)) {
		}
#endif


		/* TINFO="Finish to turn on IP0" */
	}
	return err;
}

int spm_mtcmos_ctrl_ip1(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off IP1" */
	        /* TINFO="Set bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, NNA1_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & NNA1_PROT_STEP0_0_ACK_MASK) != NNA1_PROT_STEP0_0_ACK_MASK) {
		}
#endif
 	        /* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, NNA1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & NNA1_PROT_STEP1_0_ACK_MASK) != NNA1_PROT_STEP1_0_ACK_MASK) {
		}
#endif

		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) | IP1_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IP1_SRAM_PDN_ACK = 1" */
		while ((spm_read(IP1_PWR_CON) & IP1_SRAM_PDN_ACK) != IP1_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & IP1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & IP1_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off IP1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on IP1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & IP1_PWR_STA_MASK) != IP1_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & IP1_PWR_STA_MASK) != IP1_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(IP1_PWR_CON, spm_read(IP1_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IP1_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(IP1_PWR_CON) & IP1_SRAM_PDN_ACK_BIT0) {
				/*  */
		}
#endif

		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, NNA1_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & NNA1_PROT_STEP1_0_ACK_MASK)) {
		}
#endif
		/* TINFO="Release bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, NNA1_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & NNA1_PROT_STEP0_0_ACK_MASK)) {
		}
#endif

		/* TINFO="Finish to turn on IP1" */
	}
	return err;
}

int spm_mtcmos_ctrl_ip2(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off IP2" */
	        /* TINFO="Set bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, WFST_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & WFST_PROT_STEP0_0_ACK_MASK) != WFST_PROT_STEP0_0_ACK_MASK) {
		}
#endif
 	        /* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, WFST_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & WFST_PROT_STEP1_0_ACK_MASK) != WFST_PROT_STEP1_0_ACK_MASK) {
		}
#endif

		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) | IP2_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IP2_SRAM_PDN_ACK = 1" */
		while ((spm_read(IP2_PWR_CON) & IP2_SRAM_PDN_ACK) != IP2_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & IP2_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & IP2_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off IP2" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on IP2" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & IP2_PWR_STA_MASK) != IP2_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & IP2_PWR_STA_MASK) != IP2_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(IP2_PWR_CON, spm_read(IP2_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until IP2_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(IP2_PWR_CON) & IP2_SRAM_PDN_ACK_BIT0) {
				/*  */
		}
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, WFST_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & WFST_PROT_STEP1_0_ACK_MASK)) {
		}
#endif
		/* TINFO="Release bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, WFST_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & WFST_PROT_STEP0_0_ACK_MASK)) {
		}
#endif


		/* TINFO="Finish to turn on IP2" */
	}
	return err;
}

int spm_mtcmos_ctrl_usb_mac_p1(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		spm_write(INFRA_TOPAXI_PROTECTEN_2_SET, USB_MAC_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & USB_MAC_PROT_STEP0_0_ACK_MASK) != USB_MAC_PROT_STEP0_0_ACK_MASK) {
		}
#endif

		/* TINFO="Set USB_SRAM_PDN = 1" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) | USB_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until USB_SRAM_PDN_ACK = 1" */
		while ((spm_read(USB_MAC_P1_PWR_CON) & USB_SRAM_PDN_ACK) != USB_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Start to turn off USB_MAC_P1" */
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & USB_MAC_P1_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & USB_MAC_P1_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off USB_MAC_P1" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on USB_MAC_P1" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & USB_MAC_P1_PWR_STA_MASK) != USB_MAC_P1_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & USB_MAC_P1_PWR_STA_MASK) != USB_MAC_P1_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) | PWR_RST_B);

		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(USB_MAC_P1_PWR_CON, spm_read(USB_MAC_P1_PWR_CON) & ~USB_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until USB_SRAM_PDN_ACK = 0" */
		while (spm_read(USB_MAC_P1_PWR_CON) & USB_SRAM_PDN_ACK) {
				/*  */
		}
#endif

		/* TINFO="Release bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_2_CLR, USB_MAC_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1_2) & USB_MAC_PROT_STEP0_0_ACK_MASK)) {
		}
#endif
		/* TINFO="Finish to turn on USB_MAC_P1" */
	}
	return err;
}

int spm_mtcmos_ctrl_dsp(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off DSP" */
		/* TINFO="Set bus protect - step1 : 10" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DSP_PROT_STEP1_10_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & DSP_PROT_STEP1_10_ACK_MASK) != DSP_PROT_STEP1_10_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step1 : 01" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, DSP_PROT_STEP1_00_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & DSP_PROT_STEP1_00_ACK_MASK) != DSP_PROT_STEP1_00_ACK_MASK) {
		}
#endif

		/* TINFO="AUDIO_BUS_AUD_SI0[0]=0"*/
		spm_write(AUDIO_BUS_AUD_SI0, spm_read(AUDIO_BUS_AUD_SI0) & ~(0x1 << 0));
		/* TINFO="AUDIO_BUS_INFRA_SI0[0]=0"*/
		spm_write(AUDIO_BUS_INFRA_SI0, spm_read(AUDIO_BUS_INFRA_SI0) & ~(0x1 << 0));
		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) | DSP_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DSP_SRAM_PDN_ACK = 1" */
		while ((spm_read(DSP_PWR_CON) & DSP_SRAM_PDN_ACK) != DSP_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & DSP_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & DSP_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off DSP" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on DSP" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & DSP_PWR_STA_MASK) != DSP_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & DSP_PWR_STA_MASK) != DSP_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DSP_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(DSP_PWR_CON) & DSP_SRAM_PDN_ACK_BIT0) {
				/*  */
		}
#endif
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DSP_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(DSP_PWR_CON) & DSP_SRAM_PDN_ACK_BIT1) {
				/*  */
		}
#endif
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~(0x1 << 10));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DSP_SRAM_PDN_ACK_BIT2 = 0" */
		while (spm_read(DSP_PWR_CON) & DSP_SRAM_PDN_ACK_BIT2) {
				/*  */
		}
#endif
		spm_write(DSP_PWR_CON, spm_read(DSP_PWR_CON) & ~(0x1 << 11));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until DSP_SRAM_PDN_ACK_BIT3 = 0" */
		while (spm_read(DSP_PWR_CON) & DSP_SRAM_PDN_ACK_BIT3) {
				/*  */
		}
#endif
		/* TINFO="AUDIO_BUS_AUD_SI0[0]=1"*/
		spm_write(AUDIO_BUS_AUD_SI0, spm_read(AUDIO_BUS_AUD_SI0) | (0x1 << 0));
		/* TINFO="AUDIO_BUS_INFRA_SI0[0]=1"*/
		spm_write(AUDIO_BUS_INFRA_SI0, spm_read(AUDIO_BUS_INFRA_SI0) | (0x1 << 0));

		/* TINFO="Release bus protect - step1 : 00" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DSP_PROT_STEP1_00_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & DSP_PROT_STEP1_00_ACK_MASK)) {
		}
#endif
		/* TINFO="Release bus protect - step1 : 10" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, DSP_PROT_STEP1_10_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & DSP_PROT_STEP1_10_ACK_MASK)) {
		}
#endif



		/* TINFO="Finish to turn on DSP" */
	}
	return err;
}

int spm_mtcmos_ctrl_audio(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off AUDIO AFE" */
		/* TINFO="disable audio dcm en bit29" */
		spm_write(PERI_BUS_DCM_CTRL, spm_read(PERI_BUS_DCM_CTRL) & 0xDFFFFFFF);
		/* TINFO="Set bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, AFE_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & AFE_PROT_STEP0_0_ACK_MASK) != AFE_PROT_STEP0_0_ACK_MASK) {
		}
#endif
		/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, AFE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & AFE_PROT_STEP1_0_ACK_MASK) != AFE_PROT_STEP1_0_ACK_MASK) {
		}
#endif


		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | AUDIO_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK = 1" */
		while ((spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK) != AUDIO_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off AUDIO AFE" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on AUDIO AFE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & AUDIO_PWR_STA_MASK) != AUDIO_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & AUDIO_PWR_STA_MASK) != AUDIO_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT0) {
				/*  */
		}
#endif
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 9));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT1 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT1) {
				/*  */
		}
#endif
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 10));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT2 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT2) {
				/*  */
		}
#endif
		spm_write(AUDIO_PWR_CON, spm_read(AUDIO_PWR_CON) & ~(0x1 << 11));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until AUDIO_SRAM_PDN_ACK_BIT3 = 0" */
		while (spm_read(AUDIO_PWR_CON) & AUDIO_SRAM_PDN_ACK_BIT3) {
				/*  */
		}
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, AFE_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & AFE_PROT_STEP1_0_ACK_MASK)) {
		}
#endif
		/* TINFO="Release bus protect - step0 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, AFE_PROT_STEP0_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & AFE_PROT_STEP0_0_ACK_MASK)) {
		}
#endif

		/* TINFO="Finish to turn on AUDIO AFE" */
	}
	return err;
}

int spm_mtcmos_ctrl_asrc(int state)
{
	int err = 0;

	/* TINFO="enable SPM register control" */
	spm_write(POWERON_CONFIG_EN, (SPM_PROJECT_CODE << 16) | (0x1 << 0));

	if (state == STA_POWER_DOWN) {
		/* TINFO="Start to turn off AUDIO ASRC" */

			/* TINFO="Set bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_SET, SRC_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & SRC_PROT_STEP1_0_ACK_MASK) != SRC_PROT_STEP1_0_ACK_MASK) {
		}
#endif

		/* TINFO="Set SRAM_PDN = 1" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) | ASRC_SRAM_PDN);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ASRC_SRAM_PDN_ACK = 1" */
		while ((spm_read(ASRC_PWR_CON) & ASRC_SRAM_PDN_ACK) != ASRC_SRAM_PDN_ACK) {
				/*  */
		}
#endif
		/* TINFO="Set PWR_ISO = 1" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) | PWR_ISO);
		/* TINFO="Set PWR_CLK_DIS = 1" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) | PWR_CLK_DIS);
		/* TINFO="Set PWR_RST_B = 0" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) & ~PWR_RST_B);
		/* TINFO="Set PWR_ON = 0" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) & ~PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 0" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) & ~PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
		while ((spm_read(PWR_STATUS) & ASRC_PWR_STA_MASK)
		       || (spm_read(PWR_STATUS_2ND) & ASRC_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Finish to turn off AUDIO ASRC" */
	} else {    /* STA_POWER_ON */
		/* TINFO="Start to turn on AUDIO AFE" */
		/* TINFO="Set PWR_ON = 1" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) | PWR_ON);
		/* TINFO="Set PWR_ON_2ND = 1" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) | PWR_ON_2ND);
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
		while (((spm_read(PWR_STATUS) & ASRC_PWR_STA_MASK) != ASRC_PWR_STA_MASK)
		       || ((spm_read(PWR_STATUS_2ND) & ASRC_PWR_STA_MASK) != ASRC_PWR_STA_MASK)) {
				/* No logic between pwr_on and pwr_ack. Print SRAM / MTCMOS control and PWR_ACK for debug. */
		}
#endif
		/* TINFO="Set PWR_CLK_DIS = 0" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) & ~PWR_CLK_DIS);
		/* TINFO="Set PWR_ISO = 0" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) & ~PWR_ISO);
		/* TINFO="Set PWR_RST_B = 1" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) | PWR_RST_B);
		/* TINFO="Set SRAM_PDN = 0" */
		spm_write(ASRC_PWR_CON, spm_read(ASRC_PWR_CON) & ~(0x1 << 8));
#ifndef IGNORE_MTCMOS_CHECK
		/* TINFO="Wait until ASRC_SRAM_PDN_ACK_BIT0 = 0" */
		while (spm_read(ASRC_PWR_CON) & ASRC_SRAM_PDN_ACK_BIT0) {
				/*  */
		}
#endif
		/* TINFO="Release bus protect - step1 : 0" */
		spm_write(INFRA_TOPAXI_PROTECTEN_CLR, SRC_PROT_STEP1_0_MASK);
#ifndef IGNORE_MTCMOS_CHECK
		/* Note that this protect ack check after releasing protect has been ignored */
		while ((spm_read(INFRA_TOPAXI_PROTECTEN_STA1) & SRC_PROT_STEP1_0_ACK_MASK)) {
		}
#endif

		
		/* TINFO="Finish to turn on AUDIO ASRC" */
	}
	return err;
}

int dump_pwr_status(void)
{
	int i;
	static const char * const pwr_names[] = {
		[0]  = "",
		[1]  = "CONN",
		[2]  = "DDRPHY",
		[3]  = "",
		[4]  = "",
		[5]  = "",
		[6]  = "INFRA",
		[7]  = "",
		[8]  = "MP0_CPUTOP",
		[9]  = "MP0_CPU0",
		[10] = "MP0_CPU1",
		[11] = "",
		[12] = "",
		[13] = "",
		[14] = "",
		[15] = "MM",
		[16] = "IMG",
		[17] = "DSP",
		[18] = "",
		[19] = "",
		[20] = "USB",
		[21] = "",
		[22] = "",
		[23] = "AUDSRC",
		[24] = "AUDAFE",
		[25] = "IP0_NNA",
		[26] = "IP1_NNA",
		[27] = "IP2_WFST",
		[28] = "",
		[29] = "",
		[30] = "",
		[31] = "",
	};

	u32 sta = readl(PWR_STATUS);
	u32 sta2 = readl(PWR_STATUS_2ND);

	u32 spm_pwr_status = sta & sta2;

	for (i = 0; i < 32; i++) {
		const char *st = (spm_pwr_status & BIT(i)) ? "ON" : "off";

		pr_err("[%2d]: %3s: %s\n", i, st, pwr_names[i]);
	}

	return 0;
}

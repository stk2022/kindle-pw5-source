#include "hwtcon_def.h"
#include "hwtcon_reg.h"
#include "cmdq.h"
#include "tcon_config.h"
#include "panel_setting.h"


void tcon_config_global_register(struct cmdq_pkt *pkt)
{
	pp_write(pkt, TCON_GR0, 0x8000007F);
	pp_write(pkt, TCON_GR1, 0x0C000000);
}

void tcon_config_timing0(struct cmdq_pkt *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM0R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM0R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM0R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing1(struct cmdq_pkt *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM1R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM1R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM1R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing2(struct cmdq_pkt *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM2R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM2R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM2R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing3(struct cmdq_pkt *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM3R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM3R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM3R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing4(struct cmdq_pkt *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM4R0,
		(timing_para->HE<<16) | timing_para->HS);
	pp_write(pkt, TCON_TIM4R3,
		(timing_para->VE<<16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM4R1,
		((timing_para->HSPLCNT >> 8) & 0x00FF) |
		((timing_para->HSPLCNT & 0x00FF) << 8),
		BIT_MASKS(15, 0));
	pp_write_mask(pkt, TCON_TIM4R4,
		timing_para->VACTSEL << 31,
		BIT_MASK(31));
	pp_write_mask(pkt, TCON_TIM4R6,
		timing_para->TCOPR,
		BIT_MASKS(2, 0));
	pp_write_mask(pkt, TCON_TIM4R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing5(struct cmdq_pkt *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM5R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM5R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM5R1,
		((timing_para->HSPLCNT >> 8) & 0x00FF) |
		((timing_para->HSPLCNT & 0x00FF) << 8),
		BIT_MASKS(15, 0));
	pp_write_mask(pkt, TCON_TIM5R4,
		timing_para->VACTSEL << 31,
		BIT_MASK(31));
	pp_write_mask(pkt, TCON_TIM5R6,
		timing_para->TCOPR,
		BIT_MASKS(2, 0));
	pp_write_mask(pkt, TCON_TIM5R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_config_timing6(struct cmdq_pkt *pkt,
	struct tcon_timing_para *timing_para)
{
	pp_write(pkt, TCON_TIM6R0,
		(timing_para->HE << 16) | timing_para->HS);
	pp_write(pkt, TCON_TIM6R3,
		(timing_para->VE << 16) | timing_para->VS);
	pp_write_mask(pkt, TCON_TIM6R1,
		((timing_para->HSPLCNT >> 8) & 0x00FF) |
		((timing_para->HSPLCNT & 0x00FF) << 8),
		BIT_MASKS(15, 0));
	pp_write_mask(pkt, TCON_TIM6R4,
		timing_para->VACTSEL << 31,
		BIT_MASK(31));
	pp_write_mask(pkt, TCON_TIM6R6,
		timing_para->TCOPR,
		BIT_MASKS(2, 0));
	pp_write_mask(pkt, TCON_TIM6R6,
		timing_para->INV << 7,
		BIT_MASK(7));
}

void tcon_setting(struct cmdq_pkt *pkt)
{
	struct tcon_timing_para time0_para = {0};
	struct tcon_timing_para time1_para = {0};
	struct tcon_timing_para time2_para = {0};
	struct tcon_timing_para time3_para = {0};
	struct tcon_timing_para time4_para = {0};
	struct tcon_timing_para time5_para = {0};
	struct tcon_timing_para time6_para = {0};

	/* sdce */
	time0_para.HS = platform->TIME0_HS;
	time0_para.HE = platform->TIME0_HE;
	time0_para.VS = platform->TIME0_VS;
	time0_para.VE = platform->TIME0_VE;
	time0_para.INV = platform->TIME0_INV;

	/* sdle */
	time1_para.HS = platform->TIME1_HS;
	time1_para.HE = platform->TIME1_HE;
	time1_para.VS = platform->TIME1_VS;
	time1_para.VE = platform->TIME1_VE;
	time1_para.INV = platform->TIME1_INV;

	/* time 2 sdoe all high */
	time2_para.HS = platform->TIME2_HS;
	time2_para.HE = platform->TIME2_HE;
	time2_para.VS = platform->TIME2_VS;
	time2_para.VE = platform->TIME2_VE;
	time2_para.INV = platform->TIME2_INV;

	/* gdck */
	time3_para.HS = platform->TIME3_HS;
	time3_para.HE = platform->TIME3_HE;
	time3_para.VS = platform->TIME3_VS;
	time3_para.VE = platform->TIME3_VE;
	time3_para.INV = platform->TIME3_INV;

	/* time 4 gdoe all high */


	/* gdsp */
	time5_para.HS = platform->TIME5_HS;
	time5_para.HE = platform->TIME5_HE;
	time5_para.VS = platform->TIME5_VS;
	time5_para.VE = platform->TIME5_VE;
	time5_para.HSPLCNT = platform->TIME5_HSPLCNT;
	time5_para.VACTSEL = platform->TIME5_VACTSEL;
	time5_para.INV = platform->TIME5_INV;
	time5_para.TCOPR = platform->TIME5_TCOPR;


	tcon_config_global_register(pkt);
	tcon_config_timing0(pkt, &time0_para);
	tcon_config_timing1(pkt, &time1_para);
	tcon_config_timing2(pkt, &time2_para);
	tcon_config_timing3(pkt, &time3_para);
	tcon_config_timing5(pkt, &time5_para);

}

void tcon_disable(struct cmdq_pkt *pkt)
{
	pp_write(pkt, TCON_GR1, 0x00000000);
	pp_write(pkt, TCON_GR0, 0x00000000);
}


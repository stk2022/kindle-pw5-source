/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include "cmdq_helper_ext.h"
#include "cmdq_reg.h"
#include "cmdq_device.h"
#include "cmdq_virtual.h"
#include <linux/seq_file.h>
#ifdef CMDQ_CG_M4U_LARB0
#include "m4u.h"
#endif
#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#include "smi_debug.h"
#endif

static struct cmdqCoreFuncStruct gFunctionPointer;

/*
 * GCE capability
 */
u32 cmdq_virtual_get_subsys_LSB_in_arg_a(void)
{
	return 16;
}

/* HW thread related */
bool cmdq_virtual_is_a_secure_thread(const s32 thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if ((thread >= CMDQ_MIN_SECURE_THREAD_ID) &&
		(thread < CMDQ_MIN_SECURE_THREAD_ID +
		CMDQ_MAX_SECURE_THREAD_COUNT)) {
		return true;
	}
#endif
	return false;
}

/**
 * Scenario related
 *
 */
bool cmdq_virtual_is_disp_scenario(const enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_DISPLAY:
		return true;
	default:
		return false;
	}
#if 0
	bool dispScenario = false;

	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA1_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_RDMA0_COLOR0_DISP:
	case CMDQ_SCENARIO_TRIGGER_LOOP:
	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
	case CMDQ_SCENARIO_LOWP_TRIGGER_LOOP:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
	case CMDQ_SCENARIO_DISP_VFP_CHANGE:
		/* color path */
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
#ifdef CMDQ_SECURE_PATH_SUPPORT
		/* secure path */
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
#endif
		dispScenario = true;
		break;
	default:
		break;
	}
	/* freely dispatch */
	return dispScenario;
#endif
}

bool cmdq_virtual_is_dynamic_scenario(
	const enum CMDQ_SCENARIO_ENUM scenario)
{
#if 0
	bool dynamic_thread;

	switch (scenario) {
	case CMDQ_SCENARIO_USER_SPACE:
	case CMDQ_SCENARIO_USER_MDP:
	case CMDQ_SCENARIO_DEBUG_MDP:
		dynamic_thread = true;
		break;
	default:
		dynamic_thread = false;
		break;
	}

	return dynamic_thread;
#endif

	return false;
}

bool cmdq_virtual_should_enable_prefetch(enum CMDQ_SCENARIO_ENUM scenario)
{
	bool shouldPrefetch = false;

	#if 0
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
	/* HACK: force debug into 0/1 thread */
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		/* any path that connects to Primary DISP HW
		 * should enable prefetch.
		 * MEMOUT scenarios does not.
		 * Also, since thread 0/1 shares one prefetch buffer,
		 * we allow only PRIMARY path to use prefetch.
		 */
		shouldPrefetch = true;
		break;
	default:
		break;
	}
	#endif

	return shouldPrefetch;
}

int cmdq_virtual_disp_thread(enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_HWTCON:
		return 1;
	case CMDQ_SCENARIO_HWTCON_AUTO_COLLISION_LOOP:
		return 0;
	case CMDQ_SCENARIO_MDP:
		return 2;
	case CMDQ_SCENARIO_IMGRESZ:
		return 3;
	case CMDQ_SCENARIO_PIPELINE:
		return 4;
	case CMDQ_SCENARIO_WF_LUT:
		return 5;
	case CMDQ_SCENARIO_HWTCON_LUT_END_LOOP:
		return 6;
	default:
		CMDQ_ERR("invalid scenraio:%d for dispatch thread\n", scenario);
		return 15;
	}
#if 0
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA0_COLOR0_DISP:
	/* HACK: force debug into 0/1 thread */
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		/* primary config: thread 0 */
		return 0;

	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_RDMA1_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_SUB_MEMOUT:
#ifdef CMDQ_DISP_LEGACY_SUB_SCENARIO
		/* when HW thread 0 enables pre-fetch,
		 * any thread 1 operation will let HW thread 0's behavior
		 * abnormally forbid thread 1
		 */
		return 5;
#else
		return 1;
#endif

	case CMDQ_SCENARIO_MHL_DISP:
		return 5;

	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
	case CMDQ_SCENARIO_DISP_VFP_CHANGE:
		return 2;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
		return 6;

	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
		return 3;

	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
		return 4;
	default:
		/* freely dispatch */
		return CMDQ_INVALID_THREAD;
	}
	/* freely dispatch */
	return CMDQ_INVALID_THREAD;
#endif
}

int cmdq_virtual_get_thread_index(enum CMDQ_SCENARIO_ENUM scenario,
	const bool secure)
{
	return cmdq_get_func()->dispThread(scenario);

#if 0
	if (scenario == CMDQ_SCENARIO_TIMER_LOOP)
		return CMDQ_DELAY_THREAD_ID;

	if (!secure)
		return cmdq_get_func()->dispThread(scenario);

	/* dispatch secure thread according to scenario */
	switch (scenario) {
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		/* CMDQ_MIN_SECURE_THREAD_ID */
		return CMDQ_THREAD_SEC_PRIMARY_DISP;
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
		/* because mirror mode and sub disp never use at the same time
		 * in secure path, dispatch to same HW thread
		 */
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
		return CMDQ_THREAD_SEC_SUB_DISP;
	case CMDQ_SCENARIO_USER_MDP:
	case CMDQ_SCENARIO_USER_SPACE:
	case CMDQ_SCENARIO_DEBUG:
		/* because there is one input engine for MDP, reserve one
		 * secure thread is enough
		 */
		return CMDQ_THREAD_SEC_MDP;
	default:
		CMDQ_ERR("no dedicated secure thread for senario:%d\n",
			scenario);
		return CMDQ_INVALID_THREAD;
	}
#endif
}

enum CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_virtual_priority_from_scenario(
	enum CMDQ_SCENARIO_ENUM scenario)
{
	/* harry todo: */
	switch (scenario) {
	case CMDQ_SCENARIO_HWTCON:
		return 3;
	case CMDQ_SCENARIO_HWTCON_LUT_END_LOOP:
		return 4;
	case CMDQ_SCENARIO_HWTCON_AUTO_COLLISION_LOOP:
		return 4;
	case CMDQ_SCENARIO_PIPELINE:
	case CMDQ_SCENARIO_WF_LUT:
		return 4;
	default:
		return 2;
	}
#if 0
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_RDMA1_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA0_COLOR0_DISP:
	case CMDQ_SCENARIO_DISP_MIRROR_MODE:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
	case CMDQ_SCENARIO_DISP_VFP_CHANGE:
		/* color path */
	case CMDQ_SCENARIO_DISP_COLOR:
	case CMDQ_SCENARIO_USER_DISP_COLOR:
		/* secure path */
	case CMDQ_SCENARIO_DISP_PRIMARY_DISABLE_SECURE_PATH:
	case CMDQ_SCENARIO_DISP_SUB_DISABLE_SECURE_PATH:
		/* currently, a prefetch thread is always in high priority. */
		return CMDQ_THR_PRIO_DISPLAY_CONFIG;

		/* HACK: force debug into 0/1 thread */
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		return CMDQ_THR_PRIO_DISPLAY_CONFIG;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
		return CMDQ_THR_PRIO_DISPLAY_ESD;

	case CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP:
		return CMDQ_THR_PRIO_SUPERHIGH;

	case CMDQ_SCENARIO_LOWP_TRIGGER_LOOP:
		return CMDQ_THR_PRIO_SUPERLOW;

	default:
		/* other cases need exta logic, see below. */
		break;
	}
#if 0
	if (cmdq_get_func()->is_disp_loop(scenario))
		return CMDQ_THR_PRIO_DISPLAY_TRIGGER;
	else
		return CMDQ_THR_PRIO_NORMAL;
#endif
#endif
}

bool cmdq_virtual_force_loop_irq(enum CMDQ_SCENARIO_ENUM scenario)
{
	return false;
#if 0
	bool force_loop = false;

	if (scenario == CMDQ_SCENARIO_HIGHP_TRIGGER_LOOP ||
		scenario == CMDQ_SCENARIO_LOWP_TRIGGER_LOOP) {
		/* For monitor thread loop, we need IRQ to set callback
		 * function
		 */
		force_loop = true;
	}

	return force_loop;
#endif
}

bool cmdq_virtual_is_disp_loop(enum CMDQ_SCENARIO_ENUM scenario)
{
	bool is_disp_loop = false;
#if 0
	if (scenario == CMDQ_SCENARIO_TRIGGER_LOOP)
		is_disp_loop = true;
#endif

	return is_disp_loop;
}

/**
 * Module dependent
 *
 */
#if 0
void cmdq_virtual_get_reg_id_from_hwflag(u64 hwflag,
	enum cmdq_gpr_reg *valueRegId,
	enum cmdq_gpr_reg *destRegId,
	enum cmdq_event *regAccessToken)
{
	*regAccessToken = CMDQ_SYNC_TOKEN_INVALID;

	if (hwflag & (1LL << CMDQ_ENG_JPEG_ENC)) {
		*valueRegId = CMDQ_DATA_REG_JPEG;
		*destRegId = CMDQ_DATA_REG_JPEG_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_0;
	} else if (hwflag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		*valueRegId = CMDQ_DATA_REG_2D_SHARPNESS_0;
		*destRegId = CMDQ_DATA_REG_2D_SHARPNESS_0_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_1;
	} else if (hwflag & (1LL << CMDQ_ENG_MDP_TDSHP1)) {
		*valueRegId = CMDQ_DATA_REG_2D_SHARPNESS_1;
		*destRegId = CMDQ_DATA_REG_2D_SHARPNESS_1_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_2;
	} else if (hwflag & ((1LL << CMDQ_ENG_DISP_COLOR0 |
		(1LL << CMDQ_ENG_DISP_COLOR1)))) {
		*valueRegId = CMDQ_DATA_REG_PQ_COLOR;
		*destRegId = CMDQ_DATA_REG_PQ_COLOR_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_3;
	} else {
		/* assume others are debug cases */
		*valueRegId = CMDQ_DATA_REG_DEBUG;
		*destRegId = CMDQ_DATA_REG_DEBUG_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	}
}
#endif

const char *cmdq_virtual_module_from_event_id(const s32 event,
	struct CmdqCBkStruct *groupCallback, u64 engineFlag)
{
	const char *module = "CMDQ";
	enum CMDQ_GROUP_ENUM group = CMDQ_MAX_GROUP_COUNT;

	switch (event) {
	case CMDQ_EVENT_DPI0_SOF ... CMDQ_EVENT_TCON_END:
		module = "hwtcon";
		group = CMDQ_GROUP_HWTCON;
		break;
	case CMDQ_EVENT_MDP_RDMA0_SOF ... CMDQ_EVENT_MDP_DITHER0_SOF:
	case CMDQ_EVENT_MDP_RDMA0_FRAME_DONE:
	case CMDQ_EVENT_MDP_RSZ0_FRAME_DONE:
	case CMDQ_EVENT_MDP_TDSHP0_FRAME_DONE:
	case CMDQ_EVENT_MDP_WROT0_WRITE_DONE:
	case CMDQ_EVENT_MDP_GAMMA0_FRAME_DONE:
	case CMDQ_EVENT_MDP_DITHER0_FRAME_DONE:
	case CMDQ_EVENT_MDP_OVL0_2L_FRAME_DONE:
	case CMDQ_EVENT_MDP_WDMA0_FRAME_DONE:
	case CMDQ_EVENT_MDP_RDMA0_SW_RST_DONE:
	case CMDQ_EVENT_MDP_WROT0_SW_RST_DONE:
		module = "mdp";
		group = CMDQ_GROUP_MDP;
		break;
	case CMDQ_EVENT_DISP_OVL0_2L_SOF:
	case CMDQ_EVENT_DISP_WDMA0_SOF:
	case CMDQ_EVENT_DISP_OVL0_SW_RST_DONE:
	case CMDQ_EVENT_DISP_WDMA0_SW_RST_DONE:
		module = "disp";
		group = CMDQ_GROUP_DISP;
		break;
	case CMDQ_EVENT_JPGDEC_FRAME_DONE:
	case CMDQ_EVENT_JPGDEC_BITS_FRAME_DONE:
		module = "JPEG";
		group = CMDQ_GROUP_JPEG;
		break;
	case CMDQ_EVENT_PNG_FRAME_DONE ... CMDQ_EVENT_PNG_FRAME_DONE2:
		module = "PNG";
		group = CMDQ_GROUP_PNG;
		break;
	case CMDQ_EVENT_IMGRSZ_FRAME_DONE:
	case CMDQ_EVENT_IMG_RESERVE_FRAME_DONE_0:
	case CMDQ_EVENT_IMG_RESERVE_FRAME_DONE_1:
	case CMDQ_EVENT_IMG_RESERVE_FRAME_DONE_2:
	case CMDQ_EVENT_IMG_RESERVE_FRAME_DONE_3:
		module = "IMGRESZ";
		group = CMDQ_GROUP_IMGRESZ;
		break;
	default:
		module = "CMDQ";
		group = CMDQ_MAX_GROUP_COUNT;
		break;
	}

	#if 0
	if (group < CMDQ_MAX_GROUP_COUNT && groupCallback[group].dispatchMod)
		module = groupCallback[group].dispatchMod(engineFlag);
	#endif

	return module;
}

const char *cmdq_virtual_module_from_hw_engine(u64 engineFlag)
{
	if (CMDQ_ENG_HWTCON_GROUP_FLAG(engineFlag))
		return "hwtcon";

	if (CMDQ_ENG_MDP_GROUP_FLAG(engineFlag))
		return "mdp";

	if (CMDQ_ENG_IMGRESZ_GROUP_FLAG(engineFlag))
		return "imgresz";

	if (CMDQ_ENG_JPEGDEC_GROUP_FlAG(engineFlag))
		return "jpeg_dec";

	if (CMDQ_ENG_PNGDEC_GROUP_FLAG(engineFlag))
		return "png_dec";

	return "cmdq";
}

const char *cmdq_virtual_parse_module_from_reg_addr(u32 reg_addr)
{
	const u32 addr_base_and_page = (reg_addr & 0xFFFFF000);

	/* for well-known base, we check them with 12-bit mask
	 * defined in mt_reg_base.h
	 * TODO: comfirm with SS if IO_VIRT_TO_PHYS workable when enable
	 * device tree?
	 */
	switch (addr_base_and_page) {
	/* imgssys */
	case 0x15000000:
		return "imgsys_config";
	case 0x15001000:
		return "disp_imgsyss_mutex";
	case 0x15002000:
		return "smi_larb1";
	case 0x15003000:
		return "jpeg_dec";
	case 0x15004000:
		return "imgresz";
	case 0x15005000:
		return "disp_ovl0_2l";
	case 0x15006000:
		return "disp_wdma0";
	case 0x15007000:
		return "mdp_rdma0";
	case 0x15008000:
		return "mdp_rsz0";
	case 0x15009000:
		return "mdp_tdshp0";
	case 0x1500a000:
		return "mdp_wrot0";
	case 0x1500b000:
		return "mdp_gamma0";
	case 0x1500c000:
		return "mdp_dither0";
	case 0x1500d000:
		return "mdp_aal0";
	case 0x1500e000:
		return "png_dec";
	/* mmsys */
	case 0x14000000:
		return "mmsys_config";
	case 0x14001000:
		return "disp_mmsys_mutex";
	case 0x14002000:
		return "smi_common";
	case 0x14003000:
		return "smi_larb0";
	case 0x14004000:
		return "wf_lut";
	case 0x14005000:
		return "wf_lut_disp_rdma";
	case 0x14006000:
		return "dpi0";
	case 0x14007000:
		return "tcon";
	case 0x14008000:
		return "img_rdma";
	case 0x14009000:
		return "wb_rdma";
	case 0x1400a000:
		return "wb_wdma";
	case 0x1400b000:
		return "pipeline";
	case 0x1400c000:
		return "regal";
	case 0x1400d000:
		return "paper_top";
	default:
		/* for other register address we rely
		 * on GCE subsys to group them
		 * with 16-bit mask.
		 */
		return cmdq_core_parse_subsys_from_reg_addr(reg_addr);
	}
}

s32 cmdq_virtual_can_module_entry_suspend(struct EngineStruct *engineList)
{
	s32 status = 0;
	int i;
#if 0
	enum CMDQ_ENG_ENUM mdpEngines[] = {
		CMDQ_ENG_ISP_IMGI,
		CMDQ_ENG_MDP_RDMA0,
		CMDQ_ENG_MDP_RDMA1,
		CMDQ_ENG_MDP_RSZ0,
		CMDQ_ENG_MDP_RSZ1,
		CMDQ_ENG_MDP_RSZ2,
		CMDQ_ENG_MDP_TDSHP0,
		CMDQ_ENG_MDP_TDSHP1,
		CMDQ_ENG_MDP_COLOR0,
		CMDQ_ENG_MDP_WROT0,
		CMDQ_ENG_MDP_WROT1,
		CMDQ_ENG_MDP_WDMA
	};

	for (i = 0; i < ARRAY_SIZE(mdpEngines); i++) {
		e = mdpEngines[i];
		if (engineList[e].userCount != 0) {
			CMDQ_ERR(
				"suspend but engine %d has userCount %d, owner=%d\n",
				e, engineList[e].userCount,
				engineList[e].currOwner);
			status = -EBUSY;
		}
	}
#else
	for (i = 0; i < CMDQ_MAX_ENGINE_COUNT; i++) {
		if (engineList[i].userCount != 0) {
			CMDQ_ERR(
				"suspend but engine %d has userCount %d, owner=%d\n",
				i, engineList[i].userCount,
				engineList[i].currOwner);
			status = -EBUSY;
		}
	}
#endif
	return status;
}

ssize_t cmdq_virtual_print_status_clock(char *buf)
{
	s32 length = 0;
	char *pBuffer = buf;

#ifdef CMDQ_PWR_AWARE
	/* MT_CG_DISP0_MUTEX_32K is removed in this platform */
	pBuffer += sprintf(pBuffer, "MT_CG_INFRA_GCE: %d\n",
		cmdq_dev_gce_clock_is_enable());

	pBuffer += sprintf(pBuffer, "\n");
#endif

	length = pBuffer - buf;
	return length;
}

void cmdq_virtual_print_status_seq_clock(struct seq_file *m)
{
#ifdef CMDQ_PWR_AWARE
	/* MT_CG_DISP0_MUTEX_32K is removed in this platform */
	seq_printf(m, "MT_CG_INFRA_GCE: %d", cmdq_dev_gce_clock_is_enable());

	seq_puts(m, "\n");
#endif
}

void cmdq_virtual_enable_common_clock_locked(bool enable)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable SMI & LARB0 Clock\n");
		/* Use SMI clock API */
#ifdef CONFIG_MTK_SMI_EXT
		smi_bus_prepare_enable(SMI_LARB0_REG_INDX, "CMDQ", true);
#endif
	} else {
		CMDQ_VERBOSE("[CLOCK] Disable SMI & LARB0 Clock\n");
		/* disable, reverse the sequence */
#ifdef CONFIG_MTK_SMI_EXT
		smi_bus_disable_unprepare(SMI_LARB0_REG_INDX, "CMDQ", true);
#endif
	}
#endif				/* CMDQ_PWR_AWARE */
}

void cmdq_virtual_enable_gce_clock_locked(bool enable)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable CMDQ(GCE) Clock\n");
		cmdq_dev_enable_gce_clock(enable);
	} else {
		CMDQ_VERBOSE("[CLOCK] Disable CMDQ(GCE) Clock\n");
		cmdq_dev_enable_gce_clock(enable);
	}
#endif
}


const char *cmdq_virtual_parse_handle_error_module_by_hwflag_impl(
	const struct cmdqRecStruct *pHandle)
{
	return cmdq_virtual_module_from_hw_engine(pHandle->engineFlag);
}

const char *cmdq_virtual_parse_error_module_by_hwflag_impl(
	const struct cmdqRecStruct *task)
{
	return cmdq_virtual_module_from_hw_engine(task->engineFlag);
}

/**
 * Debug
 *
 */
int cmdq_virtual_dump_smi(const int showSmiDump)
{
	int isSMIHang = 0;

#if defined(CONFIG_MTK_SMI_EXT) && !defined(CONFIG_MTK_FPGA) && \
	!defined(CONFIG_MTK_SMI_VARIANT)
	isSMIHang = smi_debug_bus_hang_detect(SMI_PARAM_BUS_OPTIMIZATION,
		showSmiDump, showSmiDump, showSmiDump);
	CMDQ_ERR("SMI Hang? = %d\n", isSMIHang);
#else
	CMDQ_LOG("[WARNING]not enable SMI dump now\n");
#endif

	return isSMIHang;
}

void cmdq_virtual_dump_gpr(void)
{
	int i = 0;
	long offset = 0;
	u32 value = 0;

	CMDQ_LOG("========= GPR dump =========\n");
	for (i = 0; i < 16; i++) {
		offset = CMDQ_GPR_R32(i);
		value = CMDQ_REG_GET32(offset);
		CMDQ_LOG("[GPR %2d]+0x%lx = 0x%08x\n", i, offset, value);
	}
	CMDQ_LOG("========= GPR dump =========\n");
}


/**
 * Record usage
 *
 */
/* harry TODO: need to implement */
u64 cmdq_virtual_flag_from_scenario(enum CMDQ_SCENARIO_ENUM scn)
{
	u64 flag = 0L;
	switch (scn) {
	case CMDQ_SCENARIO_HWTCON:
	case CMDQ_SCENARIO_HWTCON_LUT_END_LOOP:
	case CMDQ_SCENARIO_HWTCON_AUTO_COLLISION_LOOP:
	case CMDQ_SCENARIO_PIPELINE:
	case CMDQ_SCENARIO_WF_LUT:
		flag = BIT_MASK(CMDQ_ENG_IMG_RDMA) |
			BIT_MASK(CMDQ_ENG_WB_RDMA) |
			BIT_MASK(CMDQ_ENG_WB_WDMA) |
			BIT_MASK(CMDQ_ENG_PIPELINE) |
			BIT_MASK(CMDQ_ENG_REGAL) |
			BIT_MASK(CMDQ_ENG_PAPER_TOP) |
			BIT_MASK(CMDQ_ENG_WF_LUT) |
			BIT_MASK(CMDQ_ENG_DISP_RDMA) |
			BIT_MASK(CMDQ_ENG_TCON) |
			BIT_MASK(CMDQ_ENG_DISP_DPI);
		break;
	case CMDQ_SCENARIO_MDP:
		flag = BIT_MASK(CMDQ_ENG_MDP_RDMA0) |
			BIT_MASK(CMDQ_ENG_MDP_GAMMA) |
			BIT_MASK(CMDQ_ENG_MDP_RSZ0) |
			BIT_MASK(CMDQ_ENG_MDP_DTH) |
			BIT_MASK(CMDQ_ENG_MDP_TDSHP0) |
			BIT_MASK(CMDQ_ENG_MDP_WROT0);
		break;
	case CMDQ_SCENARIO_DISPLAY:
		flag = BIT_MASK(CMDQ_ENG_DISP_OVL_2L) |
			BIT_MASK(CMDQ_ENG_DISP_WDMA);
		break;

	case CMDQ_SCENARIO_IMGRESZ:
		flag = BIT_MASK(CMDQ_ENG_IMGRESZ);
		break;
	case CMDQ_SCENARIO_PNG_DEC:
		flag = BIT_MASK(CMDQ_ENG_PNG_DEC);
		break;
	case CMDQ_SCENARIO_JPG_DEC:
		flag = BIT_MASK(CMDQ_ENG_JPEG_DEC);
		break;
	default:
		flag = 0L;
	}

	return flag;
}

/**
 * Event backup
 *
 */
struct cmdq_backup_event_struct {
	enum cmdq_event EventID;
	u32 BackupValue;
};

static struct cmdq_backup_event_struct g_cmdq_backup_event[] = {
#ifdef CMDQ_EVENT_NEED_BACKUP
	{CMDQ_SYNC_TOKEN_VENC_EOF, 0,},
	{CMDQ_SYNC_TOKEN_VENC_INPUT_READY, 0,},
#endif				/* CMDQ_EVENT_NEED_BACKUP */
#ifdef CMDQ_EVENT_SVP_BACKUP
	{CMDQ_SYNC_DISP_OVL0_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_OVL1_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_2LOVL0_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_2LOVL1_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_RDMA0_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_RDMA1_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_WDMA0_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_WDMA1_2NONSEC_END, 0,},
	{CMDQ_SYNC_DISP_EXT_STREAM_EOF, 0,},
#endif				/* CMDQ_EVENT_SVP_BACKUP */
};


void cmdq_virtual_event_backup(void)
{
	int i;
	int array_size = (sizeof(g_cmdq_backup_event) /
		sizeof(struct cmdq_backup_event_struct));

	for (i = 0; i < array_size; i++) {
		if (g_cmdq_backup_event[i].EventID < 0 ||
			g_cmdq_backup_event[i].EventID >= CMDQ_SYNC_TOKEN_MAX)
			continue;

		g_cmdq_backup_event[i].BackupValue = cmdqCoreGetEvent(
			g_cmdq_backup_event[i].EventID);
		CMDQ_MSG("[backup event] event: %s, value: %d\n",
			cmdq_core_get_event_name_enum(
			g_cmdq_backup_event[i].EventID),
			g_cmdq_backup_event[i].BackupValue);
	}
}

void cmdq_virtual_event_restore(void)
{
	int i;
	int array_size = (sizeof(g_cmdq_backup_event) /
		sizeof(struct cmdq_backup_event_struct));

	for (i = 0; i < array_size; i++) {
		if (g_cmdq_backup_event[i].EventID < 0 ||
			g_cmdq_backup_event[i].EventID >= CMDQ_SYNC_TOKEN_MAX)
			continue;

		CMDQ_MSG("[restore event] event: %s, value: %d\n",
			cmdq_core_get_event_name_enum(
			g_cmdq_backup_event[i].EventID),
			g_cmdq_backup_event[i].BackupValue);

		if (g_cmdq_backup_event[i].BackupValue == 1)
			cmdqCoreSetEvent(g_cmdq_backup_event[i].EventID);
		else if (g_cmdq_backup_event[i].BackupValue == 0)
			cmdqCoreClearEvent(g_cmdq_backup_event[i].EventID);
	}
}

/**
 * Test
 *
 */
void cmdq_virtual_test_setup(void)
{
#if 0
	/* unconditionally set CMDQ_SYNC_TOKEN_CONFIG_ALLOW and mutex
	 * STREAM_DONE so that DISPSYS scenarios may pass check.
	 */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX1_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX2_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX3_STREAM_EOF);
#endif
}


void cmdq_virtual_test_cleanup(void)
{
	/* do nothing */
}

void cmdq_virtual_init_module_PA_stat(void)
{
}

void cmdq_virtual_function_setting(void)
{
	struct cmdqCoreFuncStruct *pFunc;

	pFunc = &(gFunctionPointer);

	/*
	 * GCE capability
	 */
	pFunc->getSubsysLSBArgA = cmdq_virtual_get_subsys_LSB_in_arg_a;

	/* HW thread related */
	pFunc->isSecureThread = cmdq_virtual_is_a_secure_thread;

	/**
	 * Scenario related
	 *
	 */
	pFunc->isDispScenario = cmdq_virtual_is_disp_scenario;
	pFunc->isDynamic = cmdq_virtual_is_dynamic_scenario;
	pFunc->shouldEnablePrefetch = cmdq_virtual_should_enable_prefetch;
	pFunc->dispThread = cmdq_virtual_disp_thread;
	pFunc->getThreadID = cmdq_virtual_get_thread_index;
	pFunc->priority = cmdq_virtual_priority_from_scenario;
	pFunc->force_loop_irq = cmdq_virtual_force_loop_irq;
	pFunc->is_disp_loop = cmdq_virtual_is_disp_loop;

	/**
	 * Module dependent
	 *
	 */
	#if 0
	pFunc->getRegID = cmdq_virtual_get_reg_id_from_hwflag;
	#else
	pFunc->getRegID = NULL;
	#endif
	pFunc->moduleFromEvent = cmdq_virtual_module_from_event_id;
	pFunc->parseModule = cmdq_virtual_parse_module_from_reg_addr;
	pFunc->moduleEntrySuspend = cmdq_virtual_can_module_entry_suspend;
	pFunc->printStatusClock = cmdq_virtual_print_status_clock;
	pFunc->printStatusSeqClock = cmdq_virtual_print_status_seq_clock;
	pFunc->enableGCEClockLocked = cmdq_virtual_enable_gce_clock_locked;
	pFunc->parseErrorModule =
		cmdq_virtual_parse_error_module_by_hwflag_impl;
	pFunc->parseHandleErrorModule =
		cmdq_virtual_parse_handle_error_module_by_hwflag_impl;

	/**
	 * Debug
	 *
	 */
	pFunc->dumpSMI = cmdq_virtual_dump_smi;
	pFunc->dumpGPR = cmdq_virtual_dump_gpr;

	/**
	 * Record usage
	 *
	 */
	pFunc->flagFromScenario = cmdq_virtual_flag_from_scenario;

	/**
	 * Event backup
	 *
	 */
	pFunc->eventBackup = cmdq_virtual_event_backup;
	pFunc->eventRestore = cmdq_virtual_event_restore;

	/**
	 * Test
	 *
	 */
	pFunc->testSetup = cmdq_virtual_test_setup;
	pFunc->testCleanup = cmdq_virtual_test_cleanup;
	pFunc->initModulePAStat = cmdq_virtual_init_module_PA_stat;
}

struct cmdqCoreFuncStruct *cmdq_get_func(void)
{
	return &gFunctionPointer;
}

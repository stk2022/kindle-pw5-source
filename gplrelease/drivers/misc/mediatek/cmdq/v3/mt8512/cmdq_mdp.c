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
#include "cmdq_reg.h"
#include "cmdq_mdp_common.h"
#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif
#include <linux/slab.h>
#ifdef COFNIG_MTK_IOMMU
#include "mtk_iommu.h"
#elif defined(CONFIG_MTK_M4U)
#include "m4u.h"
#endif
#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif
#include "cmdq_virtual.h"
#include "cmdq_device.h"
struct CmdqMdpModuleBaseVA {
	long MDP_RDMA;
	long MDP_GAMMA;
	long MDP_RSZ;
	long MDP_DITHER;
	long MDP_SHARP;
	long MDP_WROT;
};
static struct CmdqMdpModuleBaseVA gCmdqMdpModuleBaseVA;

struct CmdqMdpModuleClock {
	struct clk *clk_MDP_RDMA;
	struct clk *clk_MDP_GAMMA;
	struct clk *clk_MDP_RSZ;
	struct clk *clk_MDP_DITHER;
	struct clk *clk_MDP_SHARP;
	struct clk *clk_MDP_WROT;
};
static struct CmdqMdpModuleClock gCmdqMdpModuleClock;

#define IMP_ENABLE_MDP_HW_CLOCK(FN_NAME, HW_NAME)	\
uint32_t cmdq_mdp_enable_clock_##FN_NAME(bool enable)	\
{		\
	return cmdq_dev_enable_device_clock(enable,	\
		gCmdqMdpModuleClock.clk_##HW_NAME, #HW_NAME "-clk");	\
}
#define IMP_MDP_HW_CLOCK_IS_ENABLE(FN_NAME, HW_NAME)	\
bool cmdq_mdp_clock_is_enable_##FN_NAME(void)	\
{		\
	return cmdq_dev_device_clock_is_enable(		\
		gCmdqMdpModuleClock.clk_##HW_NAME);	\
}
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA, MDP_RDMA);
IMP_ENABLE_MDP_HW_CLOCK(MDP_GAMMA, MDP_GAMMA);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ, MDP_RSZ);
IMP_ENABLE_MDP_HW_CLOCK(MDP_DITHER, MDP_DITHER);
IMP_ENABLE_MDP_HW_CLOCK(MDP_SHARP, MDP_SHARP);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT, MDP_WROT);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA, MDP_RDMA);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_GAMMA, MDP_GAMMA);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ, MDP_RSZ);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_DITHER, MDP_DITHER);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_SHARP, MDP_SHARP);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT, MDP_WROT);
#undef IMP_ENABLE_MDP_HW_CLOCK
#undef IMP_MDP_HW_CLOCK_IS_ENABLE

static const uint64_t gCmdqEngineGroupBits[CMDQ_MAX_GROUP_COUNT] = {
	CMDQ_ENG_HWTCON_GROUP_BITS,
	CMDQ_ENG_MDP_GROUP_BITS,
	CMDQ_ENG_IMGRESZ_GROUP_BITS,
	CMDQ_ENG_JPEGDEC_GROUP_BITS,
	CMDQ_ENG_PNGDEC_GROPU_BITS
};


long cmdq_mdp_get_module_base_VA_MDP_RDMA(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA;
}

long cmdq_mdp_get_module_base_VA_MDP_GAMMA(void)
{
	return gCmdqMdpModuleBaseVA.MDP_GAMMA;
}

long cmdq_mdp_get_module_base_VA_MDP_RSZ(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ;
}

long cmdq_mdp_get_module_base_VA_MDP_DITHER(void)
{
	return gCmdqMdpModuleBaseVA.MDP_DITHER;
}

long cmdq_mdp_get_module_base_VA_MDP_SHARP(void)
{
	return gCmdqMdpModuleBaseVA.MDP_SHARP;
}

long cmdq_mdp_get_module_base_VA_MDP_WROT(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT;
}

#define MMSYS_CONFIG_BASE	cmdq_mdp_get_module_base_VA_MMSYS_CONFIG()
#define MDP_RDMA_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA()
#define MDP_GAMMA_BASE		cmdq_mdp_get_module_base_VA_MDP_GAMMA()
#define MDP_RSZ_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ()
#define MDP_DITHER_BASE		cmdq_mdp_get_module_base_VA_MDP_DITHER()
#define MDP_SHARP_BASE		cmdq_mdp_get_module_base_VA_MDP_SHARP()
#define MDP_WROT_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT()

struct RegDef {
	int offset;
	const char *name;
};

void cmdq_mdp_dump_mmsys_config(void)
{
#if 0
	int i = 0;
	uint32_t value = 0;

	static const struct RegDef configRegisters[] = {
		{0x100, "MMSYS Clock Gating Config_0"},
		{0x110, "MMSYS Clock Gating Config_1"},
		{0xF04, "ISP_MOUT_EN"},
		{0xF08, "MDP_RDMA0_MOUT_EN"},
		{0xF0C, "MDP_CCORR_MOUT_EN"},
		{0xF10, "MDP_PRZ0_MOUT_EN"},
		{0xF14, "MDP_PRZ1_MOUT_EN"},
		{0xF18, "MDP_TDSHP_SOUT_SEL"},
		{0xF1C, "MDP_COLOR_MOUT_EN"},
		{0xF20, "MDP_CCORR_SEL_IN"},
		{0xF24, "MDP_PRZ0_SEL_IN"},
		{0xF28, "MDP_PRZ1_SEL_IN"},
		{0xF2C, "MDP_TDSHP_SEL_IN"},
		{0xF30, "MDP_COLOR_OUT_SEL_IN"},
		{0xF34, "MDP_WDMA_SEL_IN"},
		{0xF38, "MDP_WROT0_SEL_IN"},
		{0xFA0, "MMSYS_MOUT_MASK0"},
		{0xFA4, "MMSYS_MOUT_MASK1"},
		{0xFB0, "MDP_DL_VALID_0"},
		{0xFB4, "MDP_DL_VALID_1"},
		{0xFB8, "MDP_DL_VALID_2"},
		{0xFC0, "MDP_DL_READY_0"},
		{0xFC4, "MDP_DL_READY_1"},
		{0xFC8, "MDP_DL_READY_2"},
		{0x938, "MDP_DL_CFG_RD"},
		{0x940, "MDP_DL_ASYNC_CFG_RD0"},
		{0x94C, "MDP_DL_ASYNC_CFG_RD1"},
		/*{0xF24, "COLOR0_SEL_IN"},*/
		/*{0xF18, "MDP_TDSHP_MOUT_EN"},*/
		/*{0xF0C, "DISP_OVL0_MOUT_EN"},*/
		/*{0xF04, "DISP_OVL0_2L_MOUT_EN"},*/
		/*{0xF08, "DISP_OVL1_2L_MOUT_EN"},*/
		/*{0xF4C, "DISP_DITHER0_MOUT_EN"},*/
		/*{0xF10, "DISP_RSZ_MOUT_EN"},*/
		/* {0x040, "DISP_UFOE_MOUT_EN"}, */
		/* {0x040, "MMSYS_MOUT_RST"}, */
		/*{0xFA0, "DISP_TO_WROT_SOUT_SEL"},*/
		/*{0xFA4, "MDP_COLOR_IN_SOUT_SEL"},*/
		/*{0xFB0, "MDP_TDSHP_SOUT_SEL"},*/
		{0xF6C, "DISP_WDMA0_SEL_IN"},
		/*{0xFDC, "MDP_COLOR_SEL_IN"},*/
		/*{0xF20, "DISP_COLOR_OUT_SEL_IN"},*/
		/*{0xFD8, "MDP_COLOR_OUT_SEL_IN"},*/
		/*{0xFDC, "MDP_COLOR_SEL_IN "},*/
		/* {0xFDC, "DISP_COLOR_SEL_IN"},*/
		/*{0xFE0, "MDP_PATH0_SEL_IN"},*/
		/*{0xFE4, "MDP_PATH1_SEL_IN"},*/
		/*{0x070, "DISP_WDMA1_SEL_IN"},*/
		/*{0x074, "DISP_UFOE_SEL_IN"},*/
		{0xF68, "DSI0_SEL_IN"},
		/*{0xF30, "DPI1_SEL_IN"},*/
		/*{0xF50, "DISP_RDMA0_SOUT_SEL_IN"},*/
		/*{0xF54, "DISP_RDMA1_SOUT_SEL_IN"},*/
		{0x0F0, "MMSYS_MISC"}
		/* ACK and REQ related */
		/*{0xF58, "DISP_DL_VALID_0"},*/
		/*{0xF5C, "DISP_DL_VALID_1"},*/
		/*{0xF60, "DISP_DL_READY_0"},*/
		/*{0xF64, "DISP_DL_READY_1"},*/
	};

	if (!MMSYS_CONFIG_BASE) {
		CMDQ_ERR("mmsys not porting\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(configRegisters); i++) {
		value = CMDQ_REG_GET32(MMSYS_CONFIG_BASE +
			configRegisters[i].offset);
		CMDQ_ERR("%s: 0x%08x\n", configRegisters[i].name, value);
	}
#endif
}

#if 0
int32_t cmdq_mdp_reset_with_mmsys(const uint64_t engineToResetAgain)
{
	long MMSYS_SW0_RST_B_REG = MMSYS_CONFIG_BASE + (0x140);
#ifdef CMDQ_MDP_COLOR
	int mdpColorResetBit = CMDQ_ENG_MDP_COLOR0;
#else
	int mdpColorResetBit = -1;
#endif
	int i = 0;
	uint32_t reset_bits = 0L;
	int engineResetBit[32] = {
		CMDQ_ENG_MDP_RDMA0,	/* bit  0 : MDP_RDMA0 */
		CMDQ_ENG_MDP_CCORR0,	/* bit  1 : MDP_CCORR0 */
		CMDQ_ENG_MDP_RSZ0,	/* bit  2 : MDP_RSZ0  */
		CMDQ_ENG_MDP_RSZ1,	/* bit  3 : MDP_RSZ1 */
		CMDQ_ENG_MDP_TDSHP0, /* bit  4 : MDP_TDSHP0 */
		CMDQ_ENG_MDP_WROT0,	 /* bit  5 : MDP_WROT0 */
		CMDQ_ENG_MDP_WDMA,	/* bit  6 : MDP_WDMA */
		-1,
		-1,
		-1,
		-1,
		-1,
		mdpColorResetBit,	/* bit  12 : COLOR0 */
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		-1,
		CMDQ_ENG_MDP_CAMIN,	/* bit  23 : CAM_MDP */
		[24 ... 31] = -1
	};

	for (i = 0; i < 32; ++i) {
		if (engineResetBit[i] < 0)
			continue;

		if (engineToResetAgain & (1LL << engineResetBit[i]))
			reset_bits |= (1 << i);
	}

	if (reset_bits != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits = ~reset_bits;

		CMDQ_REG_SET32(MMSYS_SW0_RST_B_REG, reset_bits);
		CMDQ_REG_SET32(MMSYS_SW0_RST_B_REG, ~0);
		/* This takes effect immediately, no need to poll state */
	}

	return 0;
}
#endif

#ifdef COFNIG_MTK_IOMMU
mtk_iommu_callback_ret_t cmdq_TranslationFault_callback(
	int port, unsigned int mva, void *data)
{
	char dispatchModel[MDP_DISPATCH_KEY_STR_LEN] = "MDP";

	CMDQ_ERR("================= [MDP M4U] Dump Begin ================\n");
	CMDQ_ERR("[MDP M4U]fault call port=%d, mva=0x%x", port, mva);

	cmdq_core_dump_tasks_info();

	switch (port) {
	case M4U_PORT_MDP_RDMA0:
		cmdq_mdp_dump_rdma(MDP_RDMA0_BASE, "RDMA0");
		break;
	case M4U_PORT_MDP_WDMA0:
		cmdq_mdp_dump_wdma(MDP_WDMA_BASE, "WDMA");
		break;
	case M4U_PORT_MDP_WROT0:
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");
		break;
	default:
		CMDQ_ERR("[MDP M4U]fault callback function");
		break;
	}

	CMDQ_ERR(
		"=============== [MDP] Frame Information Begin ====================================\n");
	/* find dispatch module and assign dispatch key */
	cmdq_mdp_check_TF_address(mva, dispatchModel);
	memcpy(data, dispatchModel, sizeof(dispatchModel));
	CMDQ_ERR(
		"=============== [MDP] Frame Information End ====================================\n");
	CMDQ_ERR("================= [MDP M4U] Dump End ================\n");

	return MTK_IOMMU_CALLBACK_HANDLED;
}
#elif defined(CONFIG_MTK_M4U)
m4u_callback_ret_t cmdq_TranslationFault_callback(
	int port, unsigned int mva, void *data)
{
	char dispatchModel[MDP_DISPATCH_KEY_STR_LEN] = "MDP";

	CMDQ_ERR("================= [MDP M4U] Dump Begin ================\n");
	CMDQ_ERR("[MDP M4U]fault call port=%d, mva=0x%x", port, mva);

	cmdq_core_dump_tasks_info();

	switch (port) {
	case M4U_PORT_MDP_RDMA0:
		cmdq_mdp_dump_rdma(MDP_RDMA0_BASE, "RDMA0");
		break;
	case M4U_PORT_MDP_WDMA0:
		cmdq_mdp_dump_wdma(MDP_WDMA_BASE, "WDMA");
		break;
	case M4U_PORT_MDP_WROT0:
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");
		break;
	default:
		CMDQ_ERR("[MDP M4U]fault callback function");
		break;
	}

	CMDQ_ERR(
		"=============== [MDP] Frame Information Begin ====================================\n");
	/* find dispatch module and assign dispatch key */
	cmdq_mdp_check_TF_address(mva, dispatchModel);
	memcpy(data, dispatchModel, sizeof(dispatchModel));
	CMDQ_ERR(
		"=============== [MDP] Frame Information End ====================================\n");
	CMDQ_ERR(
		"================= [MDP M4U] Dump End ================\n");

	return M4U_CALLBACK_HANDLED;
}
#endif

int32_t cmdqVEncDumpInfo(uint64_t engineFlag, int logLevel)
{
	return 0;
#if 0
	if (engineFlag & (1LL << CMDQ_ENG_VIDEO_ENC))
		cmdq_mdp_dump_venc(VENC_BASE, "VENC");

	return 0;
#endif
}

void cmdq_mdp_init_module_base_VA(void)
{
	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));

	gCmdqMdpModuleBaseVA.MDP_RDMA =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma");
	gCmdqMdpModuleBaseVA.MDP_GAMMA =
		cmdq_dev_alloc_reference_VA_by_name("mdp_gamma");
	gCmdqMdpModuleBaseVA.MDP_RSZ =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz");
	gCmdqMdpModuleBaseVA.MDP_DITHER =
		cmdq_dev_alloc_reference_VA_by_name("mdp_dither");
	gCmdqMdpModuleBaseVA.MDP_SHARP =
		cmdq_dev_alloc_reference_VA_by_name("mdp_sharp");
	gCmdqMdpModuleBaseVA.MDP_WROT =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot");
}

void cmdq_mdp_deinit_module_base_VA(void)
{
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_GAMMA());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_DITHER());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_SHARP());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT());

	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));
}

bool cmdq_mdp_clock_is_on(enum CMDQ_ENG_ENUM engine)
{
	switch (engine) {
	case CMDQ_ENG_MDP_RDMA0:
		return cmdq_mdp_clock_is_enable_MDP_RDMA();
	case CMDQ_ENG_MDP_GAMMA:
		return cmdq_mdp_clock_is_enable_MDP_GAMMA();
	case CMDQ_ENG_MDP_RSZ0:
		return cmdq_mdp_clock_is_enable_MDP_RSZ();
	case CMDQ_ENG_MDP_DTH:
		return cmdq_mdp_clock_is_enable_MDP_DITHER();
	case CMDQ_ENG_MDP_TDSHP0:
		return cmdq_mdp_clock_is_enable_MDP_SHARP();
	case CMDQ_ENG_MDP_WROT0:
		return cmdq_mdp_clock_is_enable_MDP_WROT();
	default:
		CMDQ_ERR("try to check unknown mdp clock:%d\n", engine);
		return false;
	}
}

void cmdq_mdp_enable_clock(bool enable, enum CMDQ_ENG_ENUM engine)
{
#if 0
	unsigned long register_address;
	uint32_t register_value;
#endif

	switch (engine) {
	case CMDQ_ENG_MDP_RDMA0:
		cmdq_mdp_enable_clock_MDP_RDMA(enable);
		break;
	case CMDQ_ENG_MDP_GAMMA:
		cmdq_mdp_enable_clock_MDP_GAMMA(enable);
		break;
	case CMDQ_ENG_MDP_RSZ0:
		cmdq_mdp_enable_clock_MDP_RSZ(enable);
		break;
	case CMDQ_ENG_MDP_DTH:
		cmdq_mdp_enable_clock_MDP_DITHER(enable);
		break;
	case CMDQ_ENG_MDP_TDSHP0:
		cmdq_mdp_enable_clock_MDP_SHARP(enable);
		break;
	case CMDQ_ENG_MDP_WROT0:
		cmdq_mdp_enable_clock_MDP_WROT(enable);
		break;
	default:
		CMDQ_ERR("try to enable unknown mdp clock:%d\n", engine);
		break;
	}
#if 0
	switch (engine) {
	case CMDQ_ENG_MDP_CAMIN:
		cmdq_mdp_enable_clock_CAM_MDP(enable);
		cmdq_mdp_enable_clock_IMG_DL_RELAY(enable);
		cmdq_mdp_enable_clock_IMG_DL_ASYNC_TOP(enable);
		break;
	case CMDQ_ENG_MDP_RDMA0:
		cmdq_mdp_enable_clock_MDP_RDMA0(enable);
		if (enable) {
			/* Set MDP_RDMA0 DCM enable */
			register_address = MDP_RDMA0_BASE + 0x0;
			register_value = CMDQ_REG_GET32(register_address);
			/* DCM_EN is bit 4 */
			register_value |= (0x1 << 4);
			CMDQ_REG_SET32(register_address, register_value);
		}
		break;
	case CMDQ_ENG_MDP_RSZ0:
		cmdq_mdp_enable_clock_MDP_RSZ0(enable);
		break;
	case CMDQ_ENG_MDP_RSZ1:
		cmdq_mdp_enable_clock_MDP_RSZ1(enable);
		break;
	case CMDQ_ENG_MDP_WDMA:
		cmdq_mdp_enable_clock_MDP_WDMA(enable);
		if (enable) {
			/* Set MDP_WDMA DCM enable */
			register_address = MDP_WDMA_BASE + 0x8;
			register_value = CMDQ_REG_GET32(register_address);
			/* DCM_EN is bit 31 */
			register_value |= (0x1 << 31);
			CMDQ_REG_SET32(register_address, register_value);
		}
		break;
	case CMDQ_ENG_MDP_WROT0:
#ifdef CONFIG_MTK_SMI_EXT
		if (enable)
			smi_bus_prepare_enable(SMI_LARB0_REG_INDX, "MDPSRAM",
				true);
#endif
		cmdq_mdp_enable_clock_MDP_WROT0(enable);
		if (enable) {
			/* Set MDP_WROT0 DCM enable */
			register_address = MDP_WROT0_BASE + 0x7C;
			register_value = CMDQ_REG_GET32(register_address);
			/* DCM_EN is bit 16 */
			register_value |= (0x1 << 16);
			CMDQ_REG_SET32(register_address, register_value);
		}
#ifdef CONFIG_MTK_SMI_EXT
		if (!enable)
			smi_bus_disable_unprepare(SMI_LARB0_REG_INDX, "MDPSRAM",
				true);
#endif
		break;
	case CMDQ_ENG_MDP_TDSHP0:
		cmdq_mdp_enable_clock_MDP_TDSHP0(enable);
		break;
	case CMDQ_ENG_MDP_COLOR0:
#ifdef CMDQ_MDP_COLOR
		cmdq_mdp_enable_clock_MDP_COLOR0(enable);
#endif
		break;
	case CMDQ_ENG_MDP_CCORR0:
		cmdq_mdp_enable_clock_MDP_CCORR(enable);
		break;
	default:
		CMDQ_ERR("try to enable unknown mdp clock");
		break;
	}
	#endif
}

/* Common Clock Framework */
void cmdq_mdp_init_module_clk(void)
{
	cmdq_dev_get_module_clock_by_name("mdp_rdma", "MDP_RDMA",
		&gCmdqMdpModuleClock.clk_MDP_RDMA);
	cmdq_dev_get_module_clock_by_name("mdp_gamma", "MDP_GAMMA",
		&gCmdqMdpModuleClock.clk_MDP_GAMMA);
	cmdq_dev_get_module_clock_by_name("mdp_rsz", "MDP_RSZ",
		&gCmdqMdpModuleClock.clk_MDP_RSZ);
	cmdq_dev_get_module_clock_by_name("mdp_dither", "MDP_DITHER",
		&gCmdqMdpModuleClock.clk_MDP_DITHER);
	cmdq_dev_get_module_clock_by_name("mdp_sharp", "MDP_SHARP",
		&gCmdqMdpModuleClock.clk_MDP_SHARP);
	cmdq_dev_get_module_clock_by_name("mdp_wrot", "MDP_WROT",
		&gCmdqMdpModuleClock.clk_MDP_WROT);
}
/* MDP engine dump */
void cmdq_mdp_dump_rsz(const unsigned long base, const char *label)
{
	uint32_t value[11] = { 0 };
	uint32_t request[4] = { 0 };
	uint32_t state = 0;

	value[0] = CMDQ_REG_GET32(base + 0x004);
	value[1] = CMDQ_REG_GET32(base + 0x008);
	value[2] = CMDQ_REG_GET32(base + 0x010);
	value[3] = CMDQ_REG_GET32(base + 0x014);
	value[4] = CMDQ_REG_GET32(base + 0x018);
	value[5] = CMDQ_REG_GET32(base + 0x01C);
	CMDQ_REG_SET32(base + 0x044, 0x00000001);
	value[6] = CMDQ_REG_GET32(base + 0x048);
	CMDQ_REG_SET32(base + 0x044, 0x00000002);
	value[7] = CMDQ_REG_GET32(base + 0x048);
	CMDQ_REG_SET32(base + 0x044, 0x00000003);
	value[8] = CMDQ_REG_GET32(base + 0x048);
	value[9] = CMDQ_REG_GET32(base + 0x100);
	value[10] = CMDQ_REG_GET32(base + 0x200);
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"RSZ_CONTROL_1: 0x%08x, RSZ_CONTROL_2: 0x%08x, RSZ_INPUT_IMAGE: 0x%08x, RSZ_OUTPUT_IMAGE: 0x%08x\n",
		value[0], value[1], value[2], value[3]);
	CMDQ_ERR(
		"RSZ_HORIZONTAL_COEFF_STEP: 0x%08x, RSZ_VERTICAL_COEFF_STEP: 0x%08x\n",
		value[4], value[5]);
	CMDQ_ERR(
		"RSZ_DEBUG_1: 0x%08x, RSZ_DEBUG_2: 0x%08x, RSZ_DEBUG_3: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR("PAT1_GEN_SET: 0x%08x, PAT2_GEN_SET: 0x%08x\n",
		value[9], value[10]);
	/* parse state */
	/* .valid=1/request=1: upstream module sends data */
	/* .ready=1: downstream module receives data */
	state = value[7] & 0xF;
	request[0] = state & (0x1);	/* out valid */
	request[1] = (state & (0x1 << 1)) >> 1;	/* out ready */
	request[2] = (state & (0x1 << 2)) >> 2;	/* in valid */
	request[3] = (state & (0x1 << 3)) >> 3;	/* in ready */
	CMDQ_ERR("RSZ inRdy,inRsq,outRdy,outRsq: %d,%d,%d,%d (%s)\n",
		request[3], request[2], request[1], request[0],
		cmdq_mdp_get_rsz_state(state));
}
void cmdq_mdp_dump_tdshp(const unsigned long base, const char *label)
{
	uint32_t value[10] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x114);
	value[1] = CMDQ_REG_GET32(base + 0x11C);
	value[2] = CMDQ_REG_GET32(base + 0x104);
	value[3] = CMDQ_REG_GET32(base + 0x108);
	value[4] = CMDQ_REG_GET32(base + 0x10C);
	value[5] = CMDQ_REG_GET32(base + 0x110);
	value[6] = CMDQ_REG_GET32(base + 0x120);
	value[7] = CMDQ_REG_GET32(base + 0x124);
	value[8] = CMDQ_REG_GET32(base + 0x128);
	value[9] = CMDQ_REG_GET32(base + 0x12C);
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("TDSHP INPUT_CNT: 0x%08x, OUTPUT_CNT: 0x%08x\n",
		value[0], value[1]);
	CMDQ_ERR("TDSHP INTEN: 0x%08x, INTSTA: 0x%08x, STATUS: 0x%08x\n",
		value[2], value[3], value[4]);
	CMDQ_ERR("TDSHP CFG: 0x%08x, IN_SIZE: 0x%08x, OUT_SIZE: 0x%08x\n",
		value[5], value[6], value[8]);
	CMDQ_ERR("TDSHP OUTPUT_OFFSET: 0x%08x, BLANK_WIDTH: 0x%08x\n",
		value[7], value[9]);
}
int32_t cmdqMdpClockOn(uint64_t engineFlag)
{
	CMDQ_MSG("Enable MDP(0x%llx) clock begin\n", engineFlag);
#ifdef CMDQ_PWR_AWARE
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_GAMMA);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_DTH);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TDSHP0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT0);
#else
	CMDQ_MSG("Force MDP clock all on\n");

	/* enable all bits in MMSYS_CG_CLR0 and MMSYS_CG_CLR1 */
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x108, 0xFFFFFFFF);
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x118, 0xFFFFFFFF);

#endif				/* #ifdef CMDQ_PWR_AWARE */

	CMDQ_MSG("Enable MDP(0x%llx) clock end\n", engineFlag);

	return 0;
}

struct MODULE_BASE {
	uint64_t engine;
	/* considering 64 bit kernel, use long type to store base addr */
	long base;
	const char *name;
};

#define DEFINE_MODULE(eng, base) {eng, base, #eng}

int32_t cmdqMdpDumpInfo(uint64_t engineFlag, int logLevel)
{
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0))
		cmdq_mdp_dump_rdma(MDP_RDMA_BASE, "RDMA");
#if 0
	if (engineFlag & (1LL << CMDQ_ENG_MDP_GAMMA))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_GAMMA_BASE, "GAMMA");
#endif
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ_BASE, "RSZ");
#if 0
	if (engineFlag & (1LL << CMDQ_ENG_MDP_DTH))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_DITHER_BASE, "DITHER");
#endif
	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_SHARP_BASE, "SHARP");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0))
		cmdq_mdp_dump_rot(MDP_WROT_BASE, "WROT");

	#if 0
	/* verbose case, dump entire 1KB HW register region */
	/* for each enabled HW module. */
	if (logLevel >= 1) {
		int inner = 0;

		const struct MODULE_BASE bases[] = {
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA0, MDP_RDMA0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ0, MDP_RSZ0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ1, MDP_RSZ1_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_COLOR0, MDP_COLOR_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT0, MDP_WROT0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WDMA, MDP_WDMA_BASE),
		};

		for (inner = 0; inner < ARRAY_SIZE(bases); ++inner) {
			if (engineFlag & (1LL << bases[inner].engine)) {
				CMDQ_ERR(
					"========= [CMDQ] %s dump base 0x%lx ========\n",
					bases[inner].name, bases[inner].base);
				print_hex_dump(KERN_ERR, "",
					DUMP_PREFIX_ADDRESS, 32, 4,
					(void *)bases[inner].base, 1024,
					false);
			}
		}
	}
	#endif

	return 0;
}

enum MOUT_BITS {
	MOUT_BITS_ISP_MDP = 4,	/* bit  4: ISP_MDP multiple outupt reset */
	MOUT_BITS_MDP_CCORR = 5,/* bit  5: MDP_CCORR multiple outupt reset */
	MOUT_BITS_MDP_COLOR = 6, /* bit  6: MDP_COLOR multiple outupt reset */
	MOUT_BITS_MDP_RDMA0 = 7, /* bit  7: MDP_RDMA0 multiple outupt reset */
	MOUT_BITS_MDP_PRZ0 = 8,	/* bit  8: MDP_PRZ0 multiple outupt reset */
	MOUT_BITS_MDP_PRZ1 = 9,	/* bit  9: MDP_PRZ1 multiple outupt reset */
};

int32_t cmdqMdpResetEng(uint64_t engineFlag)
{
/* #ifndef CMDQ_PWR_AWARE */
#if 1
	return 0;
#else
	int status = 0;
	int64_t engineToResetAgain = 0LL;
	uint32_t mout_bits_old = 0L;
	uint32_t mout_bits = 0L;

	long MMSYS_MOUT_RST_REG = MMSYS_CONFIG_BASE + (0x048);

	CMDQ_PROF_START(0, "MDP_Rst");
	CMDQ_VERBOSE("Reset MDP(0x%llx) begin\n", engineFlag);

	/* After resetting each component, */
	/* we need also reset corresponding MOUT config. */
	mout_bits_old = CMDQ_REG_GET32(MMSYS_MOUT_RST_REG);
	mout_bits = 0;

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		mout_bits |= (1 << MOUT_BITS_MDP_RDMA0);

		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_RDMA0,
			MDP_RDMA0_BASE + 0x8, MDP_RDMA0_BASE + 0x408,
			0x7FF00, 0x100, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0)) {
		mout_bits |= (1 << MOUT_BITS_MDP_PRZ0);
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ0)) {
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1)) {
		mout_bits |= (1 << MOUT_BITS_MDP_PRZ1);
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ1)) {
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0))
		mout_bits |= (1 << MOUT_BITS_MDP_COLOR);

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0x010, MDP_WROT0_BASE + 0x014,
			0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WDMA)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WDMA,
			MDP_WDMA_BASE + 0x00C, MDP_WDMA_BASE + 0x0A0,
			0x3FF, 0x1, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WDMA);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		/* MDP_CAMIN can only reset by mmsys, */
		/* so this is not a "error" */
		cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN));
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CCORR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CCORR0)) {
			CMDQ_REG_SET32(MDP_CCORR_BASE + 0x04, 0x1);
			CMDQ_REG_SET32(MDP_CCORR_BASE + 0x04, 0x0);
		}
	}

	/*
	 * when MDP engines fail to reset,
	 * 1. print SMI debug log
	 * 2. try resetting from MMSYS to restore system state
	 * 3. report to QA by raising AEE warning
	 * this reset will reset all registers to power on state.
	 * but DpFramework always reconfigures register values,
	 * so there is no need to backup registers.
	 */
	if (engineToResetAgain != 0) {
		/* check SMI state immediately */
		/* if (1 == is_smi_larb_busy(0)) */
		/* { */
		/* smi_hanging_debug(5); */
		/* } */

		CMDQ_ERR(
			"Reset failed MDP engines(0x%llx), reset again with MMSYS_SW0_RST_B\n",
			 engineToResetAgain);

		cmdq_mdp_reset_with_mmsys(engineToResetAgain);

		/* finally, raise AEE warning to report normal reset fail. */
		/* we hope that reset MMSYS. */
		CMDQ_AEE("MDP", "Disable 0x%llx engine failed\n",
			engineToResetAgain);

		status = -EFAULT;
	}
	/* MOUT configuration reset */
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old & (~mout_bits)));
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old | mout_bits));
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old & (~mout_bits)));

	CMDQ_MSG("Reset MDP(0x%llx) end\n", engineFlag);
	CMDQ_PROF_END(0, "MDP_Rst");

	return status;

#endif				/* #ifdef CMDQ_PWR_AWARE */

}

int32_t cmdqMdpClockOff(uint64_t engineFlag)
{
/* #ifdef CMDQ_PWR_AWARE */
	#if 0

	CMDQ_MSG("Disable MDP(0x%llx) clock begin\n", engineFlag);
	if (engineFlag & (1LL << CMDQ_ENG_MDP_WDMA)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WDMA,
			MDP_WDMA_BASE + 0x00C, MDP_WDMA_BASE + 0X0A0,
			0x3FF, 0x1, false);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0X010, MDP_WROT0_BASE + 0X014,
			0x1, 0x1, true);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP_BASE + 0x100, 0x0);
			CMDQ_MSG("Disable MDP_TDSHP0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_TDSHP0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CCORR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CCORR0)) {
			CMDQ_MSG("Disable MDP_CCORR clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_CCORR0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ1)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ1)) {
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ1_BASE, 0x0);

			CMDQ_MSG("Disable MDP_RSZ1 clock\n");

			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ1);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ0)) {
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);

			CMDQ_MSG("Disable MDP_RSZ0 clock\n");

			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_RDMA0, MDP_RDMA0_BASE + 0x008,
			MDP_RDMA0_BASE + 0x408, 0x7FF00, 0x100, false);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CAMIN)) {
			cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN));
			CMDQ_MSG("Disable MDP_CAMIN clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_CAMIN);
		}
	}
#ifdef CMDQ_MDP_COLOR
	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_COLOR0)) {
			CMDQ_MSG("Disable MDP_COLOR0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_COLOR0);
		}
	}
#endif
	CMDQ_MSG("Disable MDP(0x%llx) clock end\n", engineFlag);
#endif				/* #ifdef CMDQ_PWR_AWARE */

	return 0;
}


void cmdqMdpInitialSetting(void)
{
#ifdef COFNIG_MTK_IOMMU
	char *data = kzalloc(MDP_DISPATCH_KEY_STR_LEN, GFP_KERNEL);

	/* Register ION Translation Fault function */
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_RDMA0,
		cmdq_TranslationFault_callback, (void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_WDMA0,
		cmdq_TranslationFault_callback, (void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_WROT0,
		cmdq_TranslationFault_callback, (void *)data);
#elif defined(CONFIG_MTK_M4U)
	char *data = kzalloc(MDP_DISPATCH_KEY_STR_LEN, GFP_KERNEL);

	/* Register M4U Translation Fault function */
	m4u_register_fault_callback(M4U_PORT_MDP_RDMA0,
		cmdq_TranslationFault_callback, (void *)data);
	m4u_register_fault_callback(M4U_PORT_MDP_WDMA0,
		cmdq_TranslationFault_callback, (void *)data);
	m4u_register_fault_callback(M4U_PORT_MDP_WROT0,
		cmdq_TranslationFault_callback, (void *)data);
#endif
}

uint32_t cmdq_mdp_rdma_get_reg_offset_src_addr(void)
{
	return 0xF00;
}

uint32_t cmdq_mdp_wrot_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

uint32_t cmdq_mdp_wdma_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

const char *cmdq_mdp_parse_error_module(const struct cmdqRecStruct *task)
{
	return cmdq_virtual_module_from_hw_engine(task->engineFlag);
}

u64 cmdq_mdp_get_engine_group_bits(u32 engine_group)
{
	return gCmdqEngineGroupBits[engine_group];
}

void testcase_clkmgr_mdp(void)
{
#if 0
#if defined(CMDQ_PWR_AWARE)
	/* RDMA clk test with src buffer addr */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_RDMA0, "CMDQ_TEST_MDP_RDMA0",
		MDP_RDMA0_BASE + cmdq_mdp_rdma_get_reg_offset_src_addr(),
		0xAACCBBDD,
		MDP_RDMA0_BASE + cmdq_mdp_rdma_get_reg_offset_src_addr(),
		true);

	/* WDMA clk test with dst buffer addr */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_WDMA, "CMDQ_TEST_MDP_WDMA",
		MDP_WDMA_BASE + cmdq_mdp_wdma_get_reg_offset_dst_addr(),
		0xAACCBBDD,
		MDP_WDMA_BASE + cmdq_mdp_wdma_get_reg_offset_dst_addr(), true);

	/* WROT clk test with dst buffer addr */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_WROT0, "CMDQ_TEST_MDP_WROT0",
		MDP_WROT0_BASE + cmdq_mdp_wrot_get_reg_offset_dst_addr(),
		0xAACCBBDD,
		MDP_WROT0_BASE + cmdq_mdp_wrot_get_reg_offset_dst_addr(),
		true);

	/* TDSHP clk test with input size */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_TDSHP0, "CMDQ_TEST_MDP_TDSHP",
		MDP_TDSHP_BASE + 0x40, 0xAACCBBDD, MDP_TDSHP_BASE + 0x40,
		true);

	/* RSZ clk test with debug port */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_RSZ0, "CMDQ_TEST_MDP_RSZ0",
		MDP_RSZ0_BASE + 0x040, 0x00000001, MDP_RSZ0_BASE + 0x044,
		false);

	testcase_clkmgr_impl(CMDQ_ENG_MDP_RSZ1, "CMDQ_TEST_MDP_RSZ1",
		MDP_RSZ1_BASE + 0x040, 0x00000001, MDP_RSZ1_BASE + 0x044,
		false);

	/* COLOR clk test with debug port */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_COLOR0, "CMDQ_TEST_MDP_COLOR",
		MDP_COLOR_BASE + 0x438, 0x000001AB, MDP_COLOR_BASE + 0x438,
		true);

	/* CCORR clk test with debug port */
	testcase_clkmgr_impl(CMDQ_ENG_MDP_CCORR0, "CMDQ_TEST_MDP_CCORR",
		MDP_CCORR_BASE + 0x30, 0x1FFF1FFF, MDP_CCORR_BASE + 0x30,
		true);

#endif
#endif
}

static void cmdq_mdp_enable_common_clock(bool enable)
{
#if 0
#ifdef CMDQ_PWR_AWARE
#ifdef CONFIG_MTK_SMI_EXT
	if (enable) {
		/* Use SMI clock API */
		smi_bus_prepare_enable(SMI_LARB0_REG_INDX, "MDP", true);

	} else {
		/* disable, reverse the sequence */
		smi_bus_disable_unprepare(SMI_LARB0_REG_INDX, "MDP", true);
	}
#endif
#endif	/* CMDQ_PWR_AWARE */
#endif
}

void cmdq_mdp_platform_function_setting(void)
{
	struct cmdqMDPFuncStruct *pFunc = cmdq_mdp_get_func();

	pFunc->dumpMMSYSConfig = cmdq_mdp_dump_mmsys_config;

#if 0
	pFunc->vEncDumpInfo = cmdqVEncDumpInfo;
#endif

	pFunc->initModuleBaseVA = cmdq_mdp_init_module_base_VA;
	pFunc->deinitModuleBaseVA = cmdq_mdp_deinit_module_base_VA;

	pFunc->mdpClockIsOn = cmdq_mdp_clock_is_on;
	pFunc->enableMdpClock = cmdq_mdp_enable_clock;
	pFunc->initModuleCLK = cmdq_mdp_init_module_clk;
	pFunc->mdpDumpRsz = cmdq_mdp_dump_rsz;
	pFunc->mdpDumpTdshp = cmdq_mdp_dump_tdshp;
	pFunc->mdpClockOn = cmdqMdpClockOn;
	pFunc->mdpDumpInfo = cmdqMdpDumpInfo;

	pFunc->mdpResetEng = cmdqMdpResetEng;
	pFunc->mdpClockOff = cmdqMdpClockOff;

	pFunc->mdpInitialSet = cmdqMdpInitialSetting;

	pFunc->rdmaGetRegOffsetSrcAddr = cmdq_mdp_rdma_get_reg_offset_src_addr;
	pFunc->wrotGetRegOffsetDstAddr = cmdq_mdp_wrot_get_reg_offset_dst_addr;
	pFunc->wdmaGetRegOffsetDstAddr = cmdq_mdp_wdma_get_reg_offset_dst_addr;
	pFunc->parseErrModByEngFlag = cmdq_mdp_parse_error_module;
	pFunc->getEngineGroupBits = cmdq_mdp_get_engine_group_bits;
	pFunc->testcaseClkmgrMdp = testcase_clkmgr_mdp;
	pFunc->mdpEnableCommonClock = cmdq_mdp_enable_common_clock;
}

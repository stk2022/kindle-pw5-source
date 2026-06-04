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

#ifndef __CMDQ_ENGINE_H__
#define __CMDQ_ENGINE_H__

enum CMDQ_ENG_ENUM {
	/* MDP */
	CMDQ_ENG_MDP_RDMA0,
	CMDQ_ENG_MDP_GAMMA,
	CMDQ_ENG_MDP_DTH,
	CMDQ_ENG_MDP_RSZ0,
	CMDQ_ENG_MDP_TDSHP0,
	CMDQ_ENG_MDP_WROT0,

	/* PIPELINE */
	CMDQ_ENG_IMG_RDMA,
	CMDQ_ENG_WB_RDMA,
	CMDQ_ENG_WB_WDMA,
	CMDQ_ENG_PIPELINE,
	CMDQ_ENG_REGAL,
	CMDQ_ENG_PAPER_TOP,

	/* WF_LUT */
	CMDQ_ENG_WF_LUT,
	CMDQ_ENG_DISP_RDMA,
	CMDQ_ENG_TCON,
	CMDQ_ENG_DISP_DPI,

	/* display */
	CMDQ_ENG_DISP_OVL_2L,
	CMDQ_ENG_DISP_WDMA,

	/* imgresz */
	CMDQ_ENG_IMGRESZ,

	/* PNG DEC */
	CMDQ_ENG_PNG_DEC,

	/* JPEG DEC */
	CMDQ_ENG_JPEG_DEC,

	CMDQ_MAX_ENGINE_COUNT	/* ALWAYS keep at the end */
};

#define CMDQ_ENG_HWTCON_GROUP_BITS ((1LL << CMDQ_ENG_IMG_RDMA) | \
			(1LL << CMDQ_ENG_WB_RDMA) | \
			(1LL << CMDQ_ENG_WB_WDMA) | \
			(1LL << CMDQ_ENG_PIPELINE) | \
			(1LL << CMDQ_ENG_REGAL) | \
			(1LL << CMDQ_ENG_PAPER_TOP) | \
			(1LL << CMDQ_ENG_WF_LUT) | \
			(1LL << CMDQ_ENG_DISP_RDMA) | \
			(1LL << CMDQ_ENG_TCON) | \
			(1LL << CMDQ_ENG_DISP_DPI))

#define CMDQ_ENG_MDP_GROUP_BITS	((1LL << CMDQ_ENG_MDP_RDMA0) |	\
				 (1LL << CMDQ_ENG_MDP_GAMMA) |	\
				 (1LL << CMDQ_ENG_MDP_RSZ0) |	\
				 (1LL << CMDQ_ENG_MDP_DTH) |	\
				 (1LL << CMDQ_ENG_MDP_TDSHP0) |	\
				 (1LL << CMDQ_ENG_MDP_WROT0))

#define CMDQ_ENG_IMGRESZ_GROUP_BITS (1LL << CMDQ_ENG_IMGRESZ)

#define CMDQ_ENG_JPEGDEC_GROUP_BITS (1LL << CMDQ_ENG_JPEG_DEC)

#define CMDQ_ENG_PNGDEC_GROPU_BITS (1LL << CMDQ_ENG_PNG_DEC)

#define CMDQ_ENG_HWTCON_GROUP_FLAG(flag) \
	((flag) & (CMDQ_ENG_HWTCON_GROUP_BITS))
#define CMDQ_ENG_MDP_GROUP_FLAG(flag) \
	((flag) & (CMDQ_ENG_MDP_GROUP_BITS))
#define CMDQ_ENG_IMGRESZ_GROUP_FLAG(flag) \
	((flag) & (CMDQ_ENG_IMGRESZ_GROUP_BITS))
#define CMDQ_ENG_JPEGDEC_GROUP_FlAG(flag) \
	((flag) & (CMDQ_ENG_JPEGDEC_GROUP_BITS))
#define CMDQ_ENG_PNGDEC_GROUP_FLAG(flag) \
	((flag) & (CMDQ_ENG_PNGDEC_GROPU_BITS))

#define CMDQ_FOREACH_GROUP(ACTION_struct)\
	ACTION_struct(CMDQ_GROUP_HWTCON, HWTCON)	\
	ACTION_struct(CMDQ_GROUP_MDP, MDP)	\
	ACTION_struct(CMDQ_GROUP_DISP, DISP)	\
	ACTION_struct(CMDQ_GROUP_JPEG, JPEG)	\
	ACTION_struct(CMDQ_GROUP_PNG, PNG)	\
	ACTION_struct(CMDQ_GROUP_IMGRESZ, IMGRESZ)

#define MDP_GENERATE_ENUM(_enum, _string) _enum,

enum CMDQ_GROUP_ENUM {
	CMDQ_FOREACH_GROUP(MDP_GENERATE_ENUM)
	CMDQ_MAX_GROUP_COUNT,	/* ALWAYS keep at the end */
};

enum MDP_ENG_BASE {
	 ENGBASE_IMGSYS_CONFIG = 0,
	 ENGBASE_MDP_RDMA0,
	 ENGBASE_MDP_RSZ0,
	 ENGBASE_MDP_TDSHP0,
	 ENGBASE_MDP_GAMMA,
	 ENGBASE_MDP_DITHER,
	 ENGBASE_MDP_WROT0,
	 ENGBASE_MMSYS_MUTEX,
	 ENGBASE_COUNT
};

#endif				/* __CMDQ_ENGINE_H__ */

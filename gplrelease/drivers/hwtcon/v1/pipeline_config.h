#ifndef __PIPELINE_CONFIG_H__
#define __PIPELINE_CONFIG_H__
#include "hwtcon_def.h"
#include "cmdq.h"
#include "v1/wf_lut_config.h"

#define BIT_ENABLE(x) (1 << (x))
#define BIT_DISABLE(x) (0 << (x))
#define BIT_USE_AUTO_SOF(x) (0 << (x))
#define BIT_USE_SW_SOF(x) (1 << (x))


 
 enum SOF_SEL_HW_BIT_ENUM {
	 SOF_SEL_HW_BIT_PIPELINE = 0,
	 SOF_SEL_HW_BIT_IMG_RDMA = 1,
	 SOF_SEL_HW_BIT_WB_RDMA = 2,
	 SOF_SEL_HW_BIT_WB_WDMA = 3, 
	 SOF_SEL_HW_BIT_WF_LUT = 6,
	 SOF_SEL_HW_BIT_LUT_MERGE = 7,
 };

 enum WB_RDMA_SOF_SEL_ENUM {
	 WB_RDMA_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WB_RDMA),
	 WB_RDMA_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WB_RDMA),
 };

 enum WB_WDMA_SOF_SEL_ENUM {
	 WB_WDMA_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WB_WDMA),
	 WB_WDMA_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WB_WDMA),	 
 };

 enum PIPELINE_SOF_SEL_ENUM {
	 PIPELINE_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_PIPELINE),
	 PIPELINE_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_PIPELINE),
 };

 enum WF_LUT_SOF_SEL_ENUM {
	 WF_LUT_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_WF_LUT),
	 WF_LUT_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_WF_LUT),  
 };

 enum LUT_MERGE_SOF_SEL_ENUM {
	LUT_MERGE_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_LUT_MERGE),
	LUT_MERGE_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_LUT_MERGE),
 };

 enum IMG_RDMA_SOF_SEL_ENUM {
	IMG_RDMA_SOF_SEL_AUTO = BIT_USE_AUTO_SOF(SOF_SEL_HW_BIT_IMG_RDMA),
	IMG_RDMA_SOF_SEL_SW = BIT_USE_SW_SOF(SOF_SEL_HW_BIT_IMG_RDMA),
 };

enum DPI_VSYNC_SEL_ENUM {
	/* DPI VSYNC internal enable, config DPI register to enable. */
	DPI_VSYNC_SEL_SW = 0 << 0,
	/* DPI VSYNC auto. PIPELINE WDMA frame done enable DPI vsync,  wf lut frame done disable dpi vsync */
	DPI_VSYNC_SEL_AUTO = 1 << 0,
};

enum WB_READ_INDEX_ENUM {
	/* wb rdma read index 0 && wb wdma write index 1 */
	WB_READ_INDEX_ADDR0 = 0 << 3,
	/* wb rdma read index 1 && wb wdma write index 0 */
	WB_READ_INDEX_ADDR1 = 1 << 3,
};

enum DMA_CONFIG_SOURCE_SEL_ENUM {
	/* use image RDMA / WB_RDMA / WB_WDMA internel config parameter. */
	DMA_CONFIG_SOURCE_SEL_INTERNAL_CONFIG = 0 << 0,
	/* use HW auto calclulate. */
	DMA_CONFIG_SOURCE_SEL_AUTO_CONFIG = 1 << 0,
};

enum PRE_BUF_UPDATE_MODE_ENUM {
	/* previous buffer only update lut region with current buffer */
	PRE_BUF_UPDATE_MODE_ONLY_UPDATE_LUT_REGION = 0 << 1,
	/* previous buffer update whole region with current buffer same as: previous = current. */
	PRE_BUF_UPDATE_MODE_UPDATE_WHOLE_REGION = 1 << 1,
};

enum PAPER_UPDATE_MODE_ENUM {
	PAPER_UPDATE_MODE_FULL = 0, /* default value */
	PAPER_UPDATE_MODE_PARTITIAL = 1,
};

struct paper_top_fifo_config {
	int FIFO_READ_START_TH; /* bit[7:0] */
	int FIFO_SIZE;	/* bit[15:8] */
	int FIFO_EN;	/* bit[16] */
};


#define UPDATE_MODE_PARTIAL			0x0
#define UPDATE_MODE_FULL			0x1

enum MAIN_SOF_MODE_ENUM {
	/* first main sof frame use img buffer last update trigger.
	** then when wb_wdma first frame done will trigger dpi vsync.
	** the following main sof frame will use dpi vsync trigger.
	** DPI_EN_SEL = DPI_VSYNC_SEL_AUTO.
	*/
	MAIN_SOF_MODE_IMG_LAST_UPDATE_AND_DPI_VSYNC = 0 << 1,
	/* use dpi vsync for main sof trigger.
	** software need to enable dpi vsync first in this mode. DPI_EN_SEL = DPI_VSYNC_SEL_SW.
	*/
	MAIN_SOF_MODE_DPI_VSYNC = 1 << 1,
	/*
	** use LUT region last update trigger main sof.
	*/
	MAIN_SOF_MODE_LAST_LUT_UPDATE = 2 << 1,
};

struct update_lut_config {
	enum WF_MODE_ENUM waveform_mode;
	struct pp_rect lut_region;
	bool is_last_lut;
};



/* update a region */
void paper_config_update_lut(struct cmdq_pkt *pkt, const struct update_lut_config *lut_config);
void pp_func_init_pipeline_and_dpi_setting(u32 img_addr, u32 wb_addr_0, u32 wb_addr_1,
		u32 wf_lut_addr,
		int panel_width, int panel_height,
		enum MAIN_SOF_MODE_ENUM mode,
		enum WF_LUT_MOUT_ENUM mout);
/* config update mode: partitial update or full update */
void paper_config_update_mode(struct cmdq_pkt *pkt, enum PAPER_UPDATE_MODE_ENUM mode);
/* wait pipeline write working buffer done. */
void paper_wait_pipeline_write_wb_done(void);
u64 pipeline_get_assigned_lut_status(void);
void hwtcon_update_region_v1(int waveform_mode, int update_mode, struct pp_rect *region);



#endif /* __PIPELINE_CONFIG_H__ */


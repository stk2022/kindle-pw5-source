#include "v1/pipeline_config.h"
#include "v1/wf_lut_config.h"
#include "hwtcon_def.h"
#include "hwtcon_reg.h"
#include "cmdq.h"
#include "panel_setting.h"

/* config main sof : how main sof be triggered. */
void paper_config_main_sof_mode(struct cmdq_pkt *pkt, enum MAIN_SOF_MODE_ENUM main_sof_mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_DPI_CFG, main_sof_mode, BIT_MASKS(2, 1));
}

/* config dpi vsync trigger mode: trigger by WB WDMA SOF or HW auto. */
void paper_config_dpi_vsync_trigger_mode(struct cmdq_pkt *pkt, enum DPI_VSYNC_SEL_ENUM mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_DPI_CFG, mode, BIT_MASK(0));
}

/* wf lut sof position. Delay cycle of main sof */
void paper_config_wf_lut_sof_position(struct cmdq_pkt *pkt, u32 wf_lut_sof_position)
{
	pp_write(pkt, PAPER_TCTOP_SOF_WF_LUT_CTL, wf_lut_sof_position);
}

/* lut merge sof position. must ready before wf_lut work, so must befor than wf_lut sof */
void paper_config_lut_merge_sof_position(struct cmdq_pkt *pkt, u32 lut_merge_sof_position)
{
	pp_write(pkt, PAPER_TCTOP_SOF_MERGE_CTL, lut_merge_sof_position);
}

/* pipeline sof position.  Delay cycle of main sof */
void paper_config_pipeline_sof_position(struct cmdq_pkt *pkt, u32 pipeline_sof_position)
{
	pp_write_mask(pkt, PAPER_TCTOP_SOF_CTL, pipeline_sof_position << 16, BIT_MASKS(31,16));
}

/* config the max counter of cycle 
** the timer will start when main sof comes, and end when counter = this max counter
** other hw sof position must < max counter. otherwise this hw sof will never come.
*/
void paper_config_main_sof_max_counter(struct cmdq_pkt *pkt, u32 max_counter)
{
	pp_write(pkt, PAPER_TCTOP_SOF_MAIN_CTL, max_counter);
}

/* config hw sof select from HW or software  */
void paper_config_sof_sel(struct cmdq_pkt *pkt, enum IMG_RDMA_SOF_SEL_ENUM img_rdma_sof_sel,
	enum WB_RDMA_SOF_SEL_ENUM wb_rdma_sof_sel,
	enum WB_WDMA_SOF_SEL_ENUM wb_wdma_sof_sel,
	enum PIPELINE_SOF_SEL_ENUM pipeline_sof_sel,
	enum WF_LUT_SOF_SEL_ENUM wf_lut_sof_sel,
	enum LUT_MERGE_SOF_SEL_ENUM lut_merge_sof_sel)
{
	pp_write_mask(pkt, PAPER_TCTOP_SOF_CTL,
		img_rdma_sof_sel |
		wb_rdma_sof_sel |
		wb_wdma_sof_sel |
		pipeline_sof_sel |
		wf_lut_sof_sel |
		lut_merge_sof_sel,
		0xFF);
}

/* config panel width & height */
void paper_config_panel_size(struct cmdq_pkt *pkt, int panel_width, int panel_height)
{
	pp_write(pkt, PAPER_TCTOP_PANEL_SIZE, panel_height << 16 | panel_width);
}

/*
** config img buffer address
*/
void paper_config_img_buffer_addr(struct cmdq_pkt *pkt, u32 addr)
{
	pp_write(pkt, PAPER_TCTOP_IMG_ST_ADDR, addr);
}

/*
** config working buffer address
*/
void paper_config_working_buffer_addr(struct cmdq_pkt *pkt, u32 addr0, u32 addr1)
{
	pp_write(pkt, PAPER_TCTOP_WB_ST_ADDR0, addr0);
	pp_write(pkt, PAPER_TCTOP_WB_ST_ADDR1, addr1);
}

void paper_config_pre_buffer_region(struct cmdq_pkt *pkt, enum PRE_BUF_UPDATE_MODE_ENUM mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, mode, BIT_MASK(1));
}

void wf_lut_config_partial_update(struct cmdq_pkt *pkt, unsigned int partial_update)
{
	pp_write_mask(pkt, WF_LUT_CON, partial_update<<19, BIT_MASK(19));
}


/* config update mode: partitial update or full update */
void paper_config_update_mode(struct cmdq_pkt *pkt, enum PAPER_UPDATE_MODE_ENUM mode)
{
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, mode, BIT_MASK(0));
	if (mode == PAPER_UPDATE_MODE_PARTITIAL)
		paper_config_pre_buffer_region(pkt, PRE_BUF_UPDATE_MODE_UPDATE_WHOLE_REGION);

	#if 1
	/* set wf_lut to full update mode */
	wf_lut_config_partial_update(pkt, mode);
	#endif
}


/*
** config working buffer read buffer index.
** if index = WB_READ_INDEX_ADDR0: rdma read 0, wdma write 1.
** if index = WB_READ_INDEX_ADDR1: rdma read 1, wdma write 0.
*/
void paper_config_working_buffer_start_index(struct cmdq_pkt *pkt, enum WB_READ_INDEX_ENUM index)
{
	//pp_write_mask(PAPER_TCTOP_BUF_CFG0, 1 << 2 | index, BIT_MASKS(3, 2));
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, index, BIT_MASK(3));
	/* trigger */
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 1 << 2, BIT_MASK(2));
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 0 << 2, BIT_MASK(2));
}

/*
** config dma(img rdma / wb rdma / wb wdma) method.
** CONFIG_DMA_SOURCE_SEL_INTERNAL_CONFIG: use img rdma / wb rdma / wb wdma internal config parameter.
** CONFIG_DMA_SOURCE_SEL_AUTO_CONFIG: paper top auto calculate. will not use DMA hw internel config param.
*/
void paper_config_dma_source_select(struct cmdq_pkt *pkt, enum DMA_CONFIG_SOURCE_SEL_ENUM config_source)
{
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, config_source, BIT_MASK(0));
}

/* config data process fifo */
void paper_config_data_process_fifo(struct cmdq_pkt *pkt, bool enable_fifo, int fifo_size, int fifo_read_start_threshold)
{
	struct paper_top_fifo_config fifo_config = {0};

	fifo_config.FIFO_READ_START_TH = fifo_read_start_threshold;
	fifo_config.FIFO_SIZE = fifo_size;
	fifo_config.FIFO_EN = enable_fifo;

	pp_write(pkt, PAPET_TCTOP_FIFO_CFG,
		fifo_config.FIFO_READ_START_TH << 0 |
		fifo_config.FIFO_SIZE << 8 |
		fifo_config.FIFO_EN << 16);
}

/* wait pipeline write working buffer done. */
void paper_wait_pipeline_write_wb_done(void)
{
	int i = 0;

	TCON_LOG("begin to wait pipeline done:0x%x", pp_read(PP_WDMA_INTSTA));

	while (i++ < 1000) {
		if (pp_read(PP_WDMA_INTSTA) & 0x1)
			break;
		mdelay(1);
	}
	if (i > 1000)
		TCON_ERR("wait pipeline timeout:0x%x",
			pp_read(PP_WDMA_INTSTA));
	else
		TCON_LOG("wait pipeline end:0x%x",
			pp_read(PP_WDMA_INTSTA));

	pp_write(NULL, PP_WDMA_INTSTA, 0);
}

u64 pipeline_get_assigned_lut_status(void)
{
	u32 readback0 = pp_read(PIPELINE_ASSIGN_STATUS0);
	u32 readback1 = pp_read(PIPELINE_ASSIGN_STATUS1);

	return ((u64)readback1 << 32 | readback0);
}


void config_paper_top_sof(struct cmdq_pkt *pkt, enum MAIN_SOF_MODE_ENUM main_sof_mode,
	enum WF_LUT_SOF_SEL_ENUM wf_lut_sof_sel,
	enum LUT_MERGE_SOF_SEL_ENUM lut_merge_sof_sel,
	int lut_merge_sof_position,
	int wf_lut_sof_position,
	int pipeline_sof_position)
{
	int max_sof_position = lut_merge_sof_position > wf_lut_sof_position ? lut_merge_sof_position : wf_lut_sof_position;
	max_sof_position = max_sof_position > pipeline_sof_position ? max_sof_position : pipeline_sof_position;
	max_sof_position += 0x100;
	
	/* main sof select from mode. */
	paper_config_main_sof_mode(pkt, main_sof_mode);
	
	/* DPI VSYNC triggered by working buffer WDMA sof */
	paper_config_dpi_vsync_trigger_mode(pkt, DPI_VSYNC_SEL_AUTO);

	/* wf_lut sof position */
	paper_config_wf_lut_sof_position(pkt, wf_lut_sof_position);

	/* lut merge sof position. must ready before wf_lut work, so must befor than wf_lut sof */
	paper_config_lut_merge_sof_position(pkt, lut_merge_sof_position);

	/* pipeline sof position */
	paper_config_pipeline_sof_position(pkt, pipeline_sof_position);

	paper_config_main_sof_max_counter(pkt, max_sof_position);

	/* config hw sof select from HW or software  */
	/* SOF_SEL only need to update bit 6 / 7, others not care for now. */
	paper_config_sof_sel(pkt,
		IMG_RDMA_SOF_SEL_AUTO,
		WB_RDMA_SOF_SEL_AUTO,
		WB_WDMA_SOF_SEL_AUTO,
		PIPELINE_SOF_SEL_AUTO,
		wf_lut_sof_sel,
		lut_merge_sof_sel);
}

void pp_func_init_pipeline_and_dpi_setting(u32 img_addr, u32 wb_addr_0, u32 wb_addr_1,
		u32 wf_lut_addr,
		int panel_width, int panel_height,
		enum MAIN_SOF_MODE_ENUM mode,
		enum WF_LUT_MOUT_ENUM mout)
{
	struct cmdq_pkt *pkt = NULL;

	wf_lut_for_pipeline_interface(wb_addr_0, wb_addr_1, 0, wf_lut_addr, false, mout, platform->PANEL_8_BIT);

	config_paper_top_sof(pkt, mode,
		WF_LUT_SOF_SEL_SW,
		LUT_MERGE_SOF_SEL_AUTO,
		0x20, 0x50, 0x200);
	if (mode == MAIN_SOF_MODE_DPI_VSYNC)
		paper_config_dpi_vsync_trigger_mode(pkt, DPI_VSYNC_SEL_SW);
	else
		paper_config_dpi_vsync_trigger_mode(pkt, DPI_VSYNC_SEL_AUTO);

	if (mode == MAIN_SOF_MODE_LAST_LUT_UPDATE) {
		/* enable softeware release lut */
		pp_write_mask(NULL, PIPELINE_COL_UPD_CFG0, 1 << 28, BIT_MASK(28));
	} else {
		/* disable software release lut */
		pp_write_mask(NULL, PIPELINE_COL_UPD_CFG0, 0 << 28, BIT_MASK(28));
	}

	paper_config_panel_size(pkt, panel_width, panel_height);
	paper_config_img_buffer_addr(pkt, img_addr);
	paper_config_working_buffer_addr(pkt, wb_addr_0, wb_addr_1);
	paper_config_working_buffer_start_index(pkt, WB_READ_INDEX_ADDR1);
	paper_config_dma_source_select(pkt, DMA_CONFIG_SOURCE_SEL_AUTO_CONFIG);/* bit 0 use auto calculate. */
	paper_config_data_process_fifo(pkt, true, 0xFF, 0x10);

	config_image_rdma(pkt);
	config_wb_rdma(pkt);
	config_wb_wdma(pkt, panel_width, panel_height, wb_addr_0);
}

/* update a region */
void paper_config_update_lut(struct cmdq_pkt *pkt, const struct update_lut_config *lut_config)
{
	pp_write(pkt, PAPER_TCTOP_UPD_CFG0,
		lut_config->lut_region.rect_y << 17 |
		lut_config->lut_region.rect_x << 4 |
		lut_config->waveform_mode << 0);

	pp_write(pkt, PAPER_TCTOP_UPD_CFG1,
		lut_config->lut_region.rect_height << 13 |
		lut_config->lut_region.rect_width << 0);

	if (lut_config->is_last_lut) {
		/* write 1 then 0 to trigger update & last_update */
		pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
			lut_config->is_last_lut << 1 |	/* is the last lut need to update. trigger HW work. */
			1 << 0,	/* request to update a lut */
			BIT_MASKS(1,0));
		pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
			0 << 1 |	/* is the last lut need to update. trigger HW work. */
			0 << 0,	/* request to update a lut */
			BIT_MASKS(1,0));
	} else {
		/* write 1 then 0 to trigger update. */
		pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
			1 << 0,	/* request to update a lut */
			BIT_MASK(0));
		pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2,
			0 << 0,	/* request to update a lut */
			BIT_MASK(0));
	}
}

void hwtcon_update_region_v1(int waveform_mode, int update_mode, struct pp_rect *region)
{
	struct update_lut_config lut_config = {0};
	struct cmdq_pkt *pkt = NULL;
	int slot_index = 0;

	/* adjust waveform mode */
	if (update_mode == UPDATE_MODE_PARTIAL) {
		if (waveform_mode == WAVEFORM_MODE_GC16)
			waveform_mode = WAVEFORM_MODE_GC16_PARTIAL;
		if (waveform_mode == WAVEFORM_MODE_GCK16)
			waveform_mode = WAVEFORM_MODE_GCK16_PARTIAL;
	}
	slot_index = wf_lut_map_waveform_with_index_v1(waveform_mode, false);

	/* set pipeline to full update mode. */ 
	paper_config_update_mode(pkt, PAPER_UPDATE_MODE_FULL);

	/* trigger lut info  */
	memset(&lut_config, 0, sizeof(lut_config));
	lut_config.waveform_mode = slot_index;
	lut_config.lut_region.rect_x = region->rect_x;
	lut_config.lut_region.rect_y = region->rect_y;
	lut_config.lut_region.rect_width = region->rect_width;
	lut_config.lut_region.rect_height = region->rect_height;
	lut_config.is_last_lut = true;
	paper_config_update_lut(pkt, &lut_config);
	paper_wait_pipeline_write_wb_done();

	tcon_config_global_register(NULL);

	TCON_LOG("pipeline assigned lut:0x%016llx slot:%d",
		pipeline_get_assigned_lut_status(), slot_index);
}



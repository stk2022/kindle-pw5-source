#include "v2/pipeline_config.h"
#include "hwtcon.h"
#include "hwtcon_hal.h"
#include "hwtcon_reg_v2.h"
#include "panel_setting.h"

void pipeline_config_enable_sw_wb_wdma(struct cmdq_pkt *pkt, bool enable)
{
	/* sw config working buffer wdma hardwware reg(size/addr/pitch), not pipeline hardware auto config.*/
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, enable << 2, BIT_MASK(2));
}

void pipeline_config_enable_sw_wb_rdma(struct cmdq_pkt *pkt, bool enable)
{
	/* sw config working buffer rdma hardwware reg(size/addr/pitch), not pipeline hardware auto config.*/
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, enable << 1, BIT_MASK(1));
}

void pipeline_config_enable_sw_img_rdma(struct cmdq_pkt *pkt, bool enable)
{
	/* sw config image buffer rdma hardwware reg(size/addr/pitch), not pipeline hardware auto config.*/
	pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, enable << 0, BIT_MASK(0));
}

void pipeline_config_img_buffer_pitch(struct cmdq_pkt * pkt,
	enum PITCH_SELECT pitch_select_type, u32 pitch)
{
	if (pitch_select_type == PITCH_SELECT_HW_AUTO) {
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 0 << 6, BIT_MASK(6));
	} else {
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 1 << 6, BIT_MASK(6));
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG1, pitch, GENMASK(15, 0));
	}
}

void pipeline_config_wb_buffer_pitch(struct cmdq_pkt * pkt,
	enum PITCH_SELECT pitch_select_type, u32 pitch)
{
	/* This is working buffer RDMA pitch
	 * Hardware will auto calculate working buffer WDMA pitch.
	 * for Non regal case: WB WDMA pitch = WB RDMA pitch
	 * for regal case: WB WDMA pitch = WB RDMA pitch / 2
	 */
	if (pitch_select_type == PITCH_SELECT_HW_AUTO) {
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 0 << 3, BIT_MASK(3));
	} else {
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG0, 1 << 3, BIT_MASK(3));
		pp_write_mask(pkt, PAPER_TCTOP_BUF_CFG1, pitch << 16, GENMASK(31, 16));
	}
}

void pipeline_config_enable_irq_v2(struct cmdq_pkt * pkt, enum HWTCON_IRQ_TYPE irq)
{
	switch (irq) {
	case IRQ_PIPELINE_COLLISION:
		/* collision irq must enable. if disable, lut_col event will not come.
		 * if we clear collsion irq in irq handler, the COLLISION_LUT status will reset to 0.
		 * Normal driver flow, we need to enable collision irq, but don't register this irq to kernel.
		 */
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 0, BIT_MASK(0));
		break;
	case IRQ_PIPELINE_LUT_ASSIGN_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 1, BIT_MASK(1));
		break;
	case IRQ_PIPELINE_DPI_UPDATE_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 2, BIT_MASK(2));
		break;
	case IRQ_PIPELINE_LUT_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 3, BIT_MASK(3));
		break;
	case IRQ_PIPELINE_REGION_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 4, BIT_MASK(4));
		break;
	case IRQ_WF_LUT_TCON_END:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 5, BIT_MASK(5));
		break;
	case IRQ_PIPELINE_PIXEL_LUT_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 6, BIT_MASK(6));
		break;
	case IRQ_PIPELINE_WB_FRAME_DONE:
		pp_write(pkt, WB_WDMA_INTEN, 3);
		break;
	default:
		TCON_ERR("invalid irq type:%d", irq);
	}
	
}

void pipeline_config_disable_irq(struct cmdq_pkt * pkt, enum HWTCON_IRQ_TYPE irq)
{
	switch (irq) {
	case IRQ_PIPELINE_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 0, BIT_MASK(0));
		break;
	case IRQ_PIPELINE_LUT_ASSIGN_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 1, BIT_MASK(1));
		break;
	case IRQ_PIPELINE_DPI_UPDATE_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 2, BIT_MASK(2));
		break;
	case IRQ_PIPELINE_LUT_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 3, BIT_MASK(3));
		break;
	case IRQ_PIPELINE_REGION_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 4, BIT_MASK(4));
		break;
	case IRQ_WF_LUT_TCON_END:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 5, BIT_MASK(5));
		break;
	case IRQ_PIPELINE_PIXEL_LUT_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 6, BIT_MASK(6));
		break;
	case IRQ_PIPELINE_WB_FRAME_DONE:
		pp_write(pkt, WB_WDMA_INTEN, 0);
		break;
	default:
		TCON_ERR("invalid irq type:%d", irq);
	}
	
}


void pipeline_config_clear_irq_v2(struct cmdq_pkt * pkt, enum HWTCON_IRQ_TYPE irq)
{
	switch (irq) {
	case IRQ_PIPELINE_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 16, BIT_MASK(16));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 16, BIT_MASK(16));
		break;
	case IRQ_PIPELINE_LUT_ASSIGN_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 17, BIT_MASK(17));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 17, BIT_MASK(17));
		break;
	case IRQ_PIPELINE_DPI_UPDATE_DONE:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 18, BIT_MASK(18));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 18, BIT_MASK(18));
		break;
	case IRQ_PIPELINE_LUT_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 19, BIT_MASK(19));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 19, BIT_MASK(19));
		break;
	case IRQ_PIPELINE_REGION_ILLEGAL:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 20, BIT_MASK(20));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 20, BIT_MASK(20));
		break;
	case IRQ_PIPELINE_PIXEL_LUT_COLLISION:
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 1 << 22, BIT_MASK(22));
		pp_write_mask(pkt, PAPER_TCTOP_IRQ_CTL, 0 << 22, BIT_MASK(22));
		break;
	case IRQ_PIPELINE_WB_FRAME_DONE:
		pp_write(pkt, WB_WDMA_INTSTA, 0);
		break;
	default:
		TCON_ERR("invalid irq type:%d", irq);
	}
}

void pipeline_config_reset_wb_rdma(struct cmdq_pkt *pkt)
{
	pp_write_mask(pkt, WB_RDMA_GLOBAL_CON, 1 << 4, BIT_MASK(4));
	pp_write_mask(pkt, WB_RDMA_GLOBAL_CON, 0 << 4, BIT_MASK(4));
}

void pipeline_config_enable_wb_rdma(struct cmdq_pkt *pkt, bool enable)
{
	if (enable)
		pp_write(pkt, WB_RDMA_GLOBAL_CON, 0x103);
	else
		pp_write(pkt, WB_RDMA_GLOBAL_CON, 0x0);
	
	/* config wb rdma fifo */
	pp_write_mask(pkt, WB_RDMA_FIFO_CON, 1, GENMASK(9, 0));
}

void pipeline_config_reset_img_rdma(struct cmdq_pkt *pkt)
{
	pp_write_mask(pkt, IMG_RDMA_GLOBAL_CON, 1 << 4, BIT_MASK(4));
	pp_write_mask(pkt, IMG_RDMA_GLOBAL_CON, 0 << 4, BIT_MASK(4));
}

void pipeline_config_enable_img_rdma(struct cmdq_pkt *pkt, bool enable)
{
	if (enable)
		pp_write(pkt, IMG_RDMA_GLOBAL_CON, 0x103);
	else
		pp_write(pkt, IMG_RDMA_GLOBAL_CON, 0x0);
	/* config img rdma fifo */
	pp_write_mask(pkt, IMG_RDMA_FIFO_CON, 1, GENMASK(9, 0));
}

void pipeline_config_reset_wb_wdma(struct cmdq_pkt *pkt)
{
	pp_write(pkt, WB_WDMA_RST, 1);
	pp_write(pkt, WB_WDMA_RST, 0);
}

void pipeline_config_enable_wb_wdma(struct cmdq_pkt *pkt, bool enable)
{
	if (enable) {
		/* reset WDMA */
		pp_write(pkt, WB_WDMA_RST, 1);
		/* set output format */
		pp_write(pkt, WB_WDMA_CFG, 0x50);
		/* enable WB WDMA */
		pp_write(pkt, WB_WDMA_EN, 1);
	} else {
		pp_write(pkt, WB_WDMA_EN, 0);
	}

	/* config wb wdma fifo */
	pp_write_mask(pkt, WB_WDMA_BUF_CON1, 1 << 16, GENMASK(25, 16));
}

void pipeline_config_enable_illegal_setting_buffer_write(struct cmdq_pkt *pkt, bool enable)
{	/* 0x1400D004[29]: 1: disable working buffer write when illegal setting. 
	 * 0x1400D004[29]: 0: enable working buffer write when illegal setting.
	 */
	pp_write_mask(pkt, PAPER_TCTOP_MAIN_CTL, (!enable) << 29, BIT_MASK(29));
}

void pipeline_config_fifo(struct cmdq_pkt * pkt, bool enable, u32 fifo_size, u32 read_threshold)
{
	pp_write_mask(pkt, PAPET_TCTOP_FIFO_CFG, enable << 16 | fifo_size << 8 | read_threshold << 0,
		BIT_MASK(16) |GENMASK(15, 8) | GENMASK(7, 0));
}

void pipeline_config_enable_histogram(struct cmdq_pkt *pkt, bool enable)
{
	if (enable)
		pp_write_mask(pkt, PAPER_TCTOP_HIST_CFG0, 1 << 0, BIT_MASK(0));
	else
		pp_write_mask(pkt, PAPER_TCTOP_HIST_CFG0, 0 << 0, BIT_MASK(0));
}

void pipeline_config_gray_value(struct cmdq_pkt *pkt,
	u32 y2_gray_val, u32 y4_gray_val,
	u32 y8_gray_val, u32 y16_gray_val)
{
	pp_write(pkt, PAPER_TCTOP_HIST_CFG1, y2_gray_val);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG2, y4_gray_val);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG3, y8_gray_val);
	pp_write(pkt, PAPER_TCTOP_HIST_CFG4, y16_gray_val);
}

void pipeline_config_wb_merge_region(struct cmdq_pkt *pkt,
	bool enable, struct pp_rect *merge_region)
{
	pp_write_mask(pkt, PAPER_TCTOP_WB_BUF_XY_INFO, enable << 31, BIT_MASK(31));
	if (enable && merge_region) {
		pp_write_mask(pkt, PAPER_TCTOP_WB_BUF_XY_INFO,
			merge_region->rect_x << 0 | merge_region->rect_y << 16,
			GENMASK(13, 0) | GENMASK(29, 16));
		pp_write_mask(pkt, PAPER_TCTOP_WB_BUF_WH_INFO,
			merge_region->rect_width << 0 | merge_region->rect_height << 16,
			GENMASK(13, 0) | GENMASK(29, 16));
	}
}

void pipeline_config_img_rdma_addr(struct cmdq_pkt *pkt, u32 img_rdma_pa)
{
	pp_write(pkt, PAPER_TCTOP_IMG_ST_ADDR, img_rdma_pa);
}

void pipeline_config_wb_rdma_addr(struct cmdq_pkt *pkt, u32 wb_rdma_pa)
{
	pp_write(pkt, PAPER_TCTOP_WB_ST_ADDR1, wb_rdma_pa);
}

void pipeline_config_wb_wdma_addr(struct cmdq_pkt *pkt, u32 wb_wdma_pa)
{
	pp_write(pkt, PAPER_TCTOP_WB_ST_ADDR0, wb_wdma_pa);
}

void pipeline_config_panel_resolution(struct cmdq_pkt *pkt, int panel_width, int panel_height)
{
	panel_width &= 0x3FFF;
	panel_height &= 0x3FFF;
	pp_write(pkt, PAPER_TCTOP_PANEL_SIZE, panel_height << 16 | panel_width);
}

/* enum PIPELINE_FLAG_ENUM */
void pipeline_config_update_flag(struct cmdq_pkt *pkt, u32 flag)
{
	pp_write(pkt, PIPELINE_FLAG, flag);
}

void pipeline_config_update_lut(struct cmdq_pkt *pkt, int lut_id)
{
	lut_id &= GENMASK(5, 0);
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2, lut_id << 4, GENMASK(9, 4));
}

void pipeline_config_trigger_hw(struct cmdq_pkt *pkt)
{
	/* trigger pipeline start to work */
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2, 1 << 0, BIT_MASK(0));
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG2, 0 << 0, BIT_MASK(0));
}

void pipeline_config_update_region(struct cmdq_pkt *pkt, struct pp_rect region)
{
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG0,
		(region.rect_x & 0x3FFF) << 0 |
		(region.rect_y &0x3FFF) << 16,
		GENMASK(13, 0) | GENMASK(29, 16));
	pp_write_mask(pkt, PAPER_TCTOP_UPD_CFG1,
		(region.rect_width & 0x3FFF) << 0 |
		(region.rect_height & 0x3FFF) << 16,
		GENMASK(13, 0) | GENMASK(29, 16));
}

void pipeline_config_active_lut(struct cmdq_pkt *pkt, u32 active_lut1, u32 active_lut0)
{
	pp_write(pkt, PAPER_TCTOP_LUT_ACTIVE0, active_lut0);
	pp_write(pkt, PAPER_TCTOP_LUT_ACTIVE1, active_lut1);
}

void pipeline_print_irq_status(void)
{
	u32 irq_status = pp_read(PAPER_TCTOP_UPD_CFG3_VA);
	int i = 0;
	char *irq_name[] = {
		"lut_illegal",				/* bit 0: lut id >= 63 */
		"region_illegal",			/* bit 1: region > resolution */
		"lut_assign_done",			/* bit 2: wb wdma frame done */
		"lut_collision",			/* bit 3: request lut_id has collision with active lut id */
		"reserved",					/* bit 4 */
		"pixel_lut_id_collision",	/* bit 5: the update region has the same LUT ID number(no matter the LUT ID is active or not) with current update LUT ID */
		"wb_wdma_done",				/* bit 6: wb wdma frame done */
		"image_rdma_done",			/* bit 7: image rdma frame done */
		"wb_rdma_done",				/* bit 8: wb rdma frame done */
		"dpi_frame_done",			/* bit 9: dpi frame done */
		"tcon_end",					/* bit 10: tcon end */
	};

	for (i = 0; i < sizeof(irq_name) / sizeof(irq_name[0]); i++) {
		if (irq_status & BIT_MASK(i))
			TCON_LOG("irq status:%d -> %s", i, irq_name[i]);
	}
}

void pipeline_get_collision_region_v2(struct pp_rect *region)
{
	u32 val = 0;

	val = pp_read(PAPER_TCTOP_PXL_COL_RGN0_VA);
	region->rect_x = val & GENMASK(13, 0);
	region->rect_y = (val & GENMASK(29, 16)) >> 16;

	val = pp_read(PAPER_TCTOP_PXL_COL_RGN1_VA);
	region->rect_width = val & GENMASK(13, 0);
	region->rect_height = (val & GENMASK(29, 16)) >> 16;
}

void pipeline_get_collision_lut_mask(u32 *col_lut1, u32 *col_lut0)
{
	*col_lut1 = pp_read(PAPER_TCTOP_COL_STATUS1_VA);
	*col_lut0 = pp_read(PAPER_TCTOP_COL_STATUS0_VA);
}

void pipeline_get_pixel_update_region(struct pp_rect *region)
{
	u32 val = 0;

	val = pp_read(PAPER_TCTOP_PXL_UPD_RGN0_VA);
	region->rect_x = val & GENMASK(13, 0);
	region->rect_y = (val & GENMASK(29, 16)) >> 16;

	val = pp_read(PAPER_TCTOP_PXL_UPD_RGN1_VA);
	region->rect_width = val & GENMASK(13, 0);
	region->rect_height = (val & GENMASK(29, 16)) >> 16;
}

bool pipeline_get_update_void_status(void)
{
	u32 val = 0;

	val = pp_read(PAPER_TCTOP_VOID_LUT_VA);
	return val & BIT_MASK(0);
}

bool pipeline_get_do_clear_status(void)
{
	u32 val = 0;

	val = pp_read(PAPER_TCTOP_VOID_LUT_VA);
	return (val & BIT_MASK(1)) >> 1;
}

void pipeline_get_histogram(u32 *next_histogram, u32 *current_histogram)
{
	if (next_histogram)
		*next_histogram = pp_read(PAPER_TCTOP_HIST_STA1_VA);
	if (current_histogram)
		*current_histogram = pp_read(PAPER_TCTOP_HIST_STA0_VA);
}

void pipeline_get_gray_value(u32 *next_gray_value, u32 *current_gray_value)
{
	u32 gray_value = pp_read(PAPER_TCTOP_HIST_STA2_VA);

	if (next_gray_value)
		*next_gray_value = (gray_value & GENMASK(6, 4)) >> 4;
	if (current_gray_value)
		*current_gray_value = gray_value & GENMASK(2, 0);
}

int pipeline_poll_task_done(struct cmdq_pkt *pkt)
{
	/* TODO: wait for pipeline working buffer frame done */
	u32 status = 0;

	TCON_LOG("poll pipeline working buffer frame done begin");
	while (((pp_read(WB_WDMA_INTSTA_VA) & BIT_MASK(0)) == 0) && status++ < 500000);

	if (status >= 500000) {
		TCON_ERR("wait poll task done timeout reg: 0x%08x",
			pp_read(WB_WDMA_INTSTA_VA));
		return -1;
	}

	//cmdqCoreClearEvent(CMDQ_EVENT_WB_WDMA_DONE);
	TCON_LOG("wait pipeline wdma done end:0x%x", pp_read(WB_WDMA_INTSTA_VA));
	/* clear wb frame done status */
	pp_write(NULL, WB_WDMA_INTSTA, 0);
	return 0;	
}

int pipeline_config_trigger(	u32 img_addr_pa,
	u32 input_wb_addr_pa,
	u32 output_wb_addr_pa,
	struct pipeline_config_info config,
	struct pipeline_info *info)
{
	int status = 0;
	struct cmdq_pkt *pkt = NULL;

	TCON_LOG("trigger pipeline lut:%d regoin[%d %d %d %d], flag:0x%08x active lut[0x%08x 0x%08x] regal_mode:%d",
		config.update_lut,
		config.update_region.rect_x,
		config.update_region.rect_y,
		config.update_region.rect_width,
		config.update_region.rect_height,
		config.pipeline_ctl_flag,
		config.active_lut1,
		config.active_lut0,
		config.regal_mode);

	pipeline_config_enable_img_rdma(pkt, true);
	pipeline_config_enable_wb_rdma(pkt, true);
	pipeline_config_enable_wb_wdma(pkt, true);
	pipeline_config_enable_histogram(pkt, true);
	pipeline_config_enable_illegal_setting_buffer_write(pkt, false);

	#if 0
	/* enable irq */
	pipeline_config_enable_irq_v2(pkt, IRQ_PIPELINE_WB_FRAME_DONE);
	pipeline_config_enable_irq_v2(pkt, IRQ_PIPELINE_COLLISION);
	pipeline_config_enable_irq_v2(pkt, IRQ_PIPELINE_LUT_ASSIGN_DONE);
	pipeline_config_enable_irq_v2(pkt, IRQ_PIPELINE_LUT_ILLEGAL);
	pipeline_config_enable_irq_v2(pkt, IRQ_PIPELINE_REGION_ILLEGAL);
	pipeline_config_enable_irq_v2(pkt, IRQ_PIPELINE_PIXEL_LUT_COLLISION);
	#endif

	pipeline_config_fifo(pkt, true, 0xFF, 0x10);

	pipeline_config_img_rdma_addr(pkt, img_addr_pa);
	pipeline_config_wb_rdma_addr(pkt, input_wb_addr_pa);
	pipeline_config_wb_wdma_addr(pkt, output_wb_addr_pa);

	pipeline_config_panel_resolution(pkt,
		platform->PANEL_WIDTH,
		platform->PANEL_HEIGHT);

	pipeline_config_update_flag(pkt, config.pipeline_ctl_flag);
	#if 0
	if (config.pipeline_ctl_flag & PIPELINE_FLAG_REGAL)
		hwtcon_regal_config_regal_mode(pkt,
			config.regal_mode,
			platform->PANEL_WIDTH,
			platform->PANEL_HEIGHT);
	#endif

	if (config.use_wb_merge_region) {
		/* sw config working merge region */
		pipeline_config_wb_merge_region(pkt, true, &config.wb_merge_region);
	} else
		pipeline_config_wb_merge_region(pkt, false, NULL);

	if (config.use_sw_config_img_pitch)
		pipeline_config_img_buffer_pitch(pkt, PITCH_SELECT_SW_CONFIG, config.img_pitch);
	else
		pipeline_config_img_buffer_pitch(pkt, PITCH_SELECT_HW_AUTO, 0);

	if (config.use_sw_config_wb_pitch)
		pipeline_config_wb_buffer_pitch(pkt, PITCH_SELECT_SW_CONFIG, config.wb_pitch);
	else
		pipeline_config_wb_buffer_pitch(pkt, PITCH_SELECT_HW_AUTO, 0);

	pipeline_config_update_lut(pkt, config.update_lut);
	pipeline_config_update_region(pkt, config.update_region);

	pipeline_config_active_lut(pkt, config.active_lut1, config.active_lut0);

	pipeline_config_trigger_hw(pkt);

	status = pipeline_poll_task_done(pkt);
	if (status != 0)
		return status;

	pipeline_get_collision_lut_mask(&info->collision_lut_1, &info->collision_lut_0);
	pipeline_get_collision_region_v2(&info->collision_region);
	pipeline_get_histogram(&info->next_histogram, &info->current_histogram);
	info->do_clear = pipeline_get_do_clear_status();
	info->update_void = pipeline_get_update_void_status();
	info->panel_width = pp_read(PAPER_TCTOP_PANEL_SIZE_VA) & GENMASK(12, 0);
	info->panel_height = (pp_read(PAPER_TCTOP_PANEL_SIZE_VA) & GENMASK(28, 16)) >> 16;

	
	TCON_LOG("pipeline info void:%d clear:%d histo[0x%08x 0x%08x] col_lut[0x%08x 0x%08x] col_region[%d %d %d %d]",
		info->update_void,
		info->do_clear,
		info->next_histogram,
		info->current_histogram,
		info->collision_lut_1,
		info->collision_lut_0,
		info->collision_region.rect_x,
		info->collision_region.rect_y,
		info->collision_region.rect_width,
		info->collision_region.rect_height);

	return 0;
}

void hwtcon_update_region_v2(int waveform_mode, int update_mode, struct pp_rect *region)
{
	struct pipeline_config_info config;
	struct pipeline_info info;
	int status = 0;
	static int lut_id;

	memset(&config, 0, sizeof(config));
	memset(&info, 0, sizeof(info));

	/* first update */
	config.active_lut0 = 0;
	config.active_lut1 = 0;

	config.update_lut = lut_id++ % 63;

	config.update_region.rect_x = region->rect_x;
	config.update_region.rect_y = region->rect_y;
	config.update_region.rect_width = region->rect_width;
	config.update_region.rect_height = region->rect_height;
	
	config.pipeline_ctl_flag = PIPELINE_FLAG_FULL_UPDATE | PIPELINE_FLAG_Y5_INPUT;

	status = pipeline_config_trigger(g_buffer_info.image_buffer,
		g_buffer_info.wb_buffer_1,
		g_buffer_info.wb_buffer_1,
		config,
		&info);

	TS_WF_LUT_set_lut_info(NULL,
		config.update_region.rect_x,
		config.update_region.rect_y,
		config.update_region.rect_width,
		config.update_region.rect_height,
		config.update_lut,
		waveform_mode);

	return;
}


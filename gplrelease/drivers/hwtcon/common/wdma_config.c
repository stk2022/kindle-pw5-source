#include "wdma_config.h"
#include "hwtcon_reg.h"


void wdma_reset(struct cmdq_pkt *pkt)
{
	u32 wdma_state = 0;

	/* reset WB WDMA */
	pp_write(pkt, PP_WDMA_RST, 0x1); 
	pp_write(pkt, PP_WDMA_RST, 0x0);
	TCON_LOG("reset WDMA");
	while((pp_read(PP_WDMA_FLOW_CTRL_DBG) & BIT_MASKS(9,0)) != 1);
	TCON_LOG("reset WDMA done");
	
}
void wdma_config_color_format(struct cmdq_pkt *pkt)
{
	/* config WB WDMA output format: 0x5 YUY2*/
	pp_write(pkt, PP_WDMA_CFG, 0x50);
}

void wdma_config_enable_ultra(struct cmdq_pkt *pkt, bool enable)
{
	pp_write_mask(pkt, PP_WDMA_BUF_CON1, enable << 31, BIT_MASK(31));
}

void wdma_config_enable_preultra(struct cmdq_pkt *pkt, bool enable)
{
	pp_write_mask(pkt, PP_WDMA_BUF_CON1, enable << 30, BIT_MASK(30));
}

void wdma_config_buffer_pitch(struct cmdq_pkt *pkt, u32 pitch)
{
	pp_write(pkt, PP_WDMA_DST_W_IN_BYTE, pitch);
}

void wdma_config_buffer_size(struct cmdq_pkt *pkt, u32 width, u32 height)
{
	pp_write(pkt, PP_WDMA_SRC_SIZE, height << 16 | width);
}

void wdma_config_crop_size(struct cmdq_pkt *pkt, u32 x, u32 y, u32 width, u32 height)
{
	pp_write(pkt, PP_WDMA_CLIP_SIZE, height << 16 | width);
	pp_write(pkt, PP_WDMA_CLIP_COORD, y << 16 | x);
}

void wdma_config_buffer_addr(struct cmdq_pkt *pkt, u32 addr)
{
	pp_write(pkt, PP_WDMA_DST_ADDR0, addr);
}

void wdma_config_enable_interrupt(struct cmdq_pkt *pkt, bool enable)
{
	if (enable)
		pp_write(pkt, PP_WDMA_INTEN, 0x3);	/* enable WDMA irq */
	else
		pp_write(pkt, PP_WDMA_INTEN, 0x0);	/* disable WDMA irq */
}

void wdma_enable(struct cmdq_pkt *pkt)
{
	pp_write(pkt, PP_WDMA_EN, 1);
}

void config_wb_wdma(struct cmdq_pkt *pkt, int panel_width, int panel_height, u32 addr)
{
	wdma_reset(pkt);
	wdma_config_color_format(pkt);
	//wdma_config_fifo(pkt);
	wdma_config_enable_ultra(pkt, true);
	wdma_config_enable_preultra(pkt, true);
	wdma_config_buffer_pitch(pkt, panel_width);
	wdma_config_buffer_size(pkt, panel_width, panel_height);
	wdma_config_crop_size(pkt, 0, 0, panel_width, panel_height);
	wdma_config_buffer_addr(pkt, addr);
	wdma_config_enable_interrupt(pkt, true);
	wdma_enable(pkt);

}


#include "wf_lut_config.h"
#include "wf_lut_rdma_config.h"
#include "tcon_config.h"
#include "dpi_config.h"
#include "hwtcon_hal.h"
#include "hwtcon_reg_v2.h"
#include "hwtcon.h"
#include "panel_setting.h"

#include <linux/delay.h>
struct wf_lut_waveform g_waveform_table[TEMPERATURE_NUM][WAVEFORM_MODE_TOTAL_NUM];
static int g_current_temperature = 8;

void wf_lut_common_8bit_setting(struct wf_lut_con_config *wf_lut_config)
{
	memset((char*)wf_lut_config,0x00,sizeof(struct wf_lut_con_config));

	wf_lut_config->base_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->base_addr1 = g_buffer_info.wb_buffer_1;
	wf_lut_config->gray_mode = GRAY_MODE_32_GRAY_LEVEL;		/*y4 or y5*/
	wf_lut_config->width = platform->PANEL_WIDTH;
	wf_lut_config->height = platform->PANEL_HEIGHT;
	wf_lut_config->rdma_enable_mask = 0xf;
	wf_lut_config->DECFMT = 0x0;
	wf_lut_config->checksum_en = 0x01;
	wf_lut_config->checksum_sel = 0x00;
	wf_lut_config->checksum_mode = 0x00;
	wf_lut_config->H_FLIP_EN = 0x00;
	wf_lut_config->V_FLIP_EN = 0x00;
	wf_lut_config->wf_lut_en = 0x01;
	wf_lut_config->wf_lut_inten = 0x02;
	wf_lut_config->rg_default_val = 0x00;
	wf_lut_config->rg_partial_up_en = 0x00;
	wf_lut_config->rg_partial_up_val = 0x00;
	wf_lut_config->layer_greq_num = 0x10;
	wf_lut_config->layer_smi_id_en = 0x01;
	wf_lut_config->rg_8b_out = 0x01; 			/*8bit or 16bit*/
	wf_lut_config->byte_swap = 0x02;
	wf_lut_config->rg_lut_end_sel = 0x01;
	wf_lut_config->direct_link = 0x01;
	wf_lut_config->dpi_enable_mode = 0;	/* sw enable dpi clock */


	/*bit 0 output to dpi,bit1 output to wdma*/
	wf_lut_config->wf_lut_mout = WF_LUT_MOUT_WDMA;

	wf_lut_config->temperature_index = g_current_temperature;

	/* TODO */
	wf_lut_config->waveform_table_current =
		(struct wf_lut_waveform *)&g_waveform_table[wf_lut_config->temperature_index][0];

	wf_lut_config->wb_rdma[0].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[0].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[0].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;

	wf_lut_config->wb_rdma[1].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[1].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[1].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;

	wf_lut_config->wb_rdma[2].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[2].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[2].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;

	wf_lut_config->wb_rdma[3].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[3].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[3].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;

}

void wf_lut_common_16bit_setting(struct wf_lut_con_config *wf_lut_config)
{
	memset((char*)wf_lut_config,0x00,sizeof(struct wf_lut_con_config));
	
	wf_lut_config->base_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->base_addr1 = g_buffer_info.wb_buffer_1;
	wf_lut_config->gray_mode = GRAY_MODE_32_GRAY_LEVEL;		/*y4 or y5*/
	wf_lut_config->width = platform->PANEL_WIDTH;
	wf_lut_config->height = platform->PANEL_HEIGHT;
	/* this one be careful */
	wf_lut_config->rdma_enable_mask = 0xF;
	wf_lut_config->DECFMT = 0x0;
	wf_lut_config->checksum_en = 0x01;
	wf_lut_config->checksum_sel = 0x00;
	wf_lut_config->checksum_mode = 0x00;
	wf_lut_config->H_FLIP_EN = 0x00;
	wf_lut_config->V_FLIP_EN = 0x00;
	wf_lut_config->wf_lut_en = 0x01;
	wf_lut_config->wf_lut_inten = 0x02;
	wf_lut_config->rg_default_val = 0x00;
	wf_lut_config->rg_partial_up_en = 0x00;
	wf_lut_config->rg_partial_up_val = 0x00;
	wf_lut_config->layer_greq_num = 0x10;
	wf_lut_config->layer_smi_id_en = 0x01;
	wf_lut_config->rg_8b_out = 0x00; 			/*8bit or 16bit*/
	wf_lut_config->byte_swap = 0x02;
	wf_lut_config->rg_lut_end_sel = 0x01;
	wf_lut_config->direct_link = 0x01;
	wf_lut_config->dpi_enable_mode = 0;	/* sw enable dpi clock */

	/*bit 0 output to dpi,bit1 output to wdma*/
	wf_lut_config->wf_lut_mout = WF_LUT_MOUT_WDMA;
	wf_lut_config->temperature_index = g_current_temperature;
	wf_lut_config->waveform_table_current =
		(struct wf_lut_waveform *)&g_waveform_table[wf_lut_config->temperature_index][0];

	#ifdef WAVEFORM_M4U_TEST
	wf_lut_config->wb_rdma[0].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[0].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[0].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;
	
	wf_lut_config->wb_rdma[1].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[1].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[1].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;
	
	wf_lut_config->wb_rdma[2].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[2].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[2].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;
	
	wf_lut_config->wb_rdma[3].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[3].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[3].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;

	#else
	wf_lut_config->wb_rdma[0].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[0].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[0].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[0].x = 0x00;
	wf_lut_config->wb_rdma[0].y = 0x00;
	
	wf_lut_config->wb_rdma[1].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[1].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[1].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[1].x = 0x00;
	wf_lut_config->wb_rdma[1].y = 0x00;
	
	wf_lut_config->wb_rdma[2].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[2].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[2].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[2].x = 0x00;
	wf_lut_config->wb_rdma[2].y = 0x00;
	
	wf_lut_config->wb_rdma[3].width = platform->PANEL_WIDTH;
	wf_lut_config->wb_rdma[3].height = platform->PANEL_HEIGHT;
	wf_lut_config->wb_rdma[3].start_addr = g_buffer_info.wb_buffer_1;
	wf_lut_config->wb_rdma[3].x = 0x00;
	wf_lut_config->wb_rdma[3].y = 0x00;
	#endif

}


static void wf_lut_config_mmsys(struct cmdq_pkt *pkt,
			 struct wf_lut_con_config *wf_lut_config)
{
	if (wf_lut_config->rg_8b_out) {
		/*pmic control */
		//pp_write(pkt, MMSYS_DUMMY1, 0x0000007F);
		pp_write_mask(pkt, MMSYS_DUMMY1, 0x1<<6, BIT_MASK(6));
		pp_write(pkt, MMSYS_MDP_DL_CFG_WD,
			 (wf_lut_config->height << 16) |
			 (wf_lut_config->width / 4));
	} else {
		//pp_write(pkt, MMSYS_DUMMY1, 0x0000003F);
		pp_write_mask(pkt, MMSYS_DUMMY1, 0x0<<6, BIT_MASK(6));
		pp_write(pkt, MMSYS_MDP_DL_CFG_WD,
			 (wf_lut_config->height << 16) |
			 (wf_lut_config->width / 8));
	}

}

static void wf_lut_config_waveform(struct cmdq_pkt *pkt,
			    struct wf_lut_waveform *waveform)
{
	int i = 0;
	struct wf_lut_waveform *current_waveform = waveform;

	for (i = 0; i < WAVEFORM_MODE_TOTAL_NUM; i++) {
		switch (current_waveform->waveform_mode) {
		case 0:
			pp_write(pkt, WF_LUT_ADDR_0,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_0, current_waveform->len);
			break;
		case 1:
			pp_write(pkt, WF_LUT_ADDR_1,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_1, current_waveform->len);
			break;
		case 2:
			pp_write(pkt, WF_LUT_ADDR_2,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_2, current_waveform->len);
			break;
		case 3:
			pp_write(pkt, WF_LUT_ADDR_3,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_3, current_waveform->len);
			break;
		case 4:
			pp_write(pkt, WF_LUT_ADDR_4,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_4, current_waveform->len);
			break;
		case 5:
			pp_write(pkt, WF_LUT_ADDR_5,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_5, current_waveform->len);
			break;
		case 6:
			pp_write(pkt, WF_LUT_ADDR_6,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_6, current_waveform->len);
			break;
		case 7:
			pp_write(pkt, WF_LUT_ADDR_7,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_7, current_waveform->len);
			break;
		case 8:
			pp_write(pkt, WF_LUT_ADDR_8,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_8, current_waveform->len);
			break;
		case 9:
			pp_write(pkt, WF_LUT_ADDR_9,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_9, current_waveform->len);
			break;
		case 10:
			pp_write(pkt, WF_LUT_ADDR_10,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_10, current_waveform->len);
			break;
		case 11:
			pp_write(pkt, WF_LUT_ADDR_11,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_11, current_waveform->len);
			break;
		case 12:
			pp_write(pkt, WF_LUT_ADDR_12,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_12, current_waveform->len);
			break;
		case 13:
			pp_write(pkt, WF_LUT_ADDR_13,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_13, current_waveform->len);
			break;
		case 14:
			pp_write(pkt, WF_LUT_ADDR_14,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_14, current_waveform->len);
			break;
		case 15:
			pp_write(pkt, WF_LUT_ADDR_15,
				 current_waveform->start_addr);
			pp_write(pkt, WF_LUT_LEN_15, current_waveform->len);
			break;
		default:
			break;
		}
		current_waveform = current_waveform + 1;
	}
}

static void wf_lut_config_link_mode(struct cmdq_pkt *pkt,
			     struct wf_lut_con_config *wf_lut_config)
{
	/* not setting,direct link alway using this setting */
	if (wf_lut_config->direct_link)
		pp_write(pkt, WF_LUT_LINK_MODE, 0xE4380ff2);
	else
		pp_write(pkt, WF_LUT_LINK_MODE, 0x00000002);

}

static void wf_lut_config_base_addr(struct cmdq_pkt *pkt,
			     struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_LINK_BASE_ADDR, wf_lut_config->base_addr);
	pp_write(pkt, WF_LUT_LINK_BASE_ADDR_1, wf_lut_config->base_addr1);
}

static void wf_lut_config_mout(struct cmdq_pkt *pkt,
			struct wf_lut_con_config *wf_lut_config)
{
	if (wf_lut_config->rg_8b_out) {
		pp_write(pkt, WF_LUT_MOUT, 0x00107001);
		pp_write_mask(pkt, WF_LUT_MOUT,
			      wf_lut_config->wf_lut_mout, GENMASK(1, 0));
	} else {
		pp_write(pkt, WF_LUT_MOUT, 0x00107241);
		pp_write_mask(pkt, WF_LUT_MOUT,
			      wf_lut_config->wf_lut_mout, GENMASK(1, 0));
	}
}

static void wf_lut_config_common(struct cmdq_pkt *pkt,
			  struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_ROI_SIZE,
		 (wf_lut_config->height << 16) | wf_lut_config->width);
	pp_write(pkt, WF_LUT_SRC_CON, wf_lut_config->rdma_enable_mask & 0xf);

	pp_write(pkt, WF_LUT_DATAPATH_CON, 0x40000091);
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->layer_greq_num << 26, GENMASK(31, 26));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->checksum_sel << 8, GENMASK(10, 8));
	/* 0 use dpi crc, 1 use wf_lut crc */
	if (wf_lut_config->checksum_mode)
		pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      0x0 << 11, BIT_MASK(11));
	else
		pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      0x1 << 11, BIT_MASK(11));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->rg_lut_end_sel << 7, BIT_MASK(7));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->rg_de_sel << 6, BIT_MASK(6));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->checksum_en << 4, BIT_MASK(4));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON,
		      wf_lut_config->layer_smi_id_en, BIT_MASK(0));
	/* pp_write(pkt, WF_LUT_DATAPATH_CON, 0x40000891); */

}

static void wf_lut_config_inter_rdma(struct cmdq_pkt *pkt,
			      struct wf_lut_con_config *wf_lut_config)
{
	unsigned int rdma_control_value = 0x0;

	/* bit8 need set to 1 */
	rdma_control_value = (wf_lut_config->byte_swap << 24) |
	    (wf_lut_config->DECFMT << 12) |
	    (wf_lut_config->H_FLIP_EN << 10) |
	    (wf_lut_config->V_FLIP_EN << 9) | 0x00000100;

	pp_write(pkt, WF_LUT_L0_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L1_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L2_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L3_CON, rdma_control_value);

	if (!wf_lut_config->direct_link) {
		pp_write(pkt, WF_LUT_L0_SRC_SIZE,
			 wf_lut_config->wb_rdma[0].
			 height << 16 | wf_lut_config->wb_rdma[0].width);
		pp_write(pkt, WF_LUT_L0_OFFSET,
			 wf_lut_config->wb_rdma[0].y << 16 | wf_lut_config->
			 wb_rdma[0].x);
		pp_write(pkt, WF_LUT_L0_ADDR,
			 wf_lut_config->wb_rdma[0].start_addr);
		
		pp_write(pkt, WF_LUT_L1_SRC_SIZE,
			 wf_lut_config->wb_rdma[1].
			 height << 16 | wf_lut_config->wb_rdma[1].width);
		pp_write(pkt, WF_LUT_L1_OFFSET,
			 wf_lut_config->wb_rdma[1].y << 16 | wf_lut_config->
			 wb_rdma[1].x);
		pp_write(pkt, WF_LUT_L1_ADDR,
			 wf_lut_config->wb_rdma[1].start_addr);

		pp_write(pkt, WF_LUT_L2_SRC_SIZE,
			 wf_lut_config->wb_rdma[2].
			 height << 16 | wf_lut_config->wb_rdma[2].width);
		pp_write(pkt, WF_LUT_L2_OFFSET,
			 wf_lut_config->wb_rdma[2].y << 16 | wf_lut_config->
			 wb_rdma[2].x);
		pp_write(pkt, WF_LUT_L2_ADDR,
			 wf_lut_config->wb_rdma[2].start_addr);

		pp_write(pkt, WF_LUT_L3_SRC_SIZE,
			 wf_lut_config->wb_rdma[3].
			 height << 16 | wf_lut_config->wb_rdma[3].width);
		pp_write(pkt, WF_LUT_L3_OFFSET,
			 wf_lut_config->wb_rdma[3].y << 16 | wf_lut_config->
			 wb_rdma[3].x);
		pp_write(pkt, WF_LUT_L3_ADDR,
			 wf_lut_config->wb_rdma[3].start_addr);
	}

	pp_write(pkt, WF_LUT_L0_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA0_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA0_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA0_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L1_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA1_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA1_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA1_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L2_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA2_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA2_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA2_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L3_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA3_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA3_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA3_FIFO_CTRL, 0x00800000);

}

static void wf_lut_config_lut_con(struct cmdq_pkt *pkt,
			   struct wf_lut_con_config *wf_lut_config)
{
	pp_write_mask(pkt, WF_LUT_CON, wf_lut_config->gray_mode, GENMASK(2, 0));
	pp_write_mask(pkt, WF_LUT_CON,
		      wf_lut_config->rg_8b_out << 12, BIT_MASK(12));
	pp_write_mask(pkt, WF_LUT_CON,
		      wf_lut_config->rg_partial_up_en << 19, BIT_MASK(19));
	pp_write_mask(pkt, WF_LUT_CON,
		      wf_lut_config->rg_partial_up_val << 20, GENMASK(23, 20));
	pp_write_mask(pkt, WF_LUT_CON,
		      wf_lut_config->rg_default_val << 8, GENMASK(11, 8));
}

static void wf_lut_enable(struct cmdq_pkt *pkt,
		   struct wf_lut_con_config *wf_lut_config)
{
	pp_write_mask(pkt, 0x10005760, 1<<12, GENMASK(14, 12));
	pp_write(pkt, WF_LUT_INTEN, wf_lut_config->wf_lut_inten);
	/* for ctp same */
	pp_write_mask(pkt, WF_LUT_EN, wf_lut_config->wf_lut_en, BIT_MASK(0));
	//pp_write(pkt, WF_LUT_EN, wf_lut_config->wf_lut_en);
}

static void Wf_Lut_Wdma_addr(struct cmdq_pkt *pkt, unsigned int addr)
{
	pp_write(pkt, WDMA_DST_ADDR0, addr);	//WDMA_DST_ADDR0
}

static void Wf_Lut_Wdma_Config(struct cmdq_pkt *pkt,
			unsigned int width, unsigned int height)
{
	pp_write_mask(pkt, DISP_WDMA0_SEL_IN, 0x1, BIT_MASK(0));
	pp_write(pkt, WDMA_INTEN, 0x00000001);
	pp_write(pkt, WDMA_CFG, 0x02020030);
	pp_write(pkt, WDMA_DST_W_IN_BYTE, 4 * width);
	pp_write(pkt, WDMA_DST_UV_PITCH, width);
	pp_write(pkt, WDMA_SRC_SIZE, height << 16 | width);
	pp_write(pkt, WDMA_CLIP_SIZE, height << 16 | width);
	pp_write(pkt, WDMA_CLIP_COORD, 0x00000000);
	Wf_Lut_Wdma_addr(pkt, g_buffer_info.wb_buffer_1);
	pp_write(pkt, WDMA_EN, 0x00000001);
}

static unsigned int wf_lut_convert_order(unsigned char *addr)
{
	unsigned int value = 0x00;

	value = ((*addr)<<24) + (*(addr+1)<<16) + (*(addr+2)<<8) + *(addr+3);
	return value;
}

static void wf_lut_waveform_table_init(void)
{
	int i = 0;
	int j = 0;

	memset((unsigned char *)&g_waveform_table, 0,
	       sizeof(g_waveform_table));

	#if 1
	for (i = 0; i < TEMPERATURE_NUM; i++) {
		for (j = 0; j < WAVEFORM_MODE_TOTAL_NUM; j++) {
			g_waveform_table[i][j].temperature_zone = i;
			g_waveform_table[i][j].waveform_mode = j;
			g_waveform_table[i][j].start_addr =
		    g_buffer_info.wf_file_buffer +
		    wf_lut_convert_order((char*)g_buffer_info.wf_file_buffer
		    + WAVEFORM_ADDR_OFFSET_TO_BEGIN +
		    i * WAVEFORM_ADDR_OFFSET_PER_TEMP + 4 * j);
			/* be carefore for point ++ */
			g_waveform_table[i][j].start_addr_va =
			    (u32*)((char*)g_buffer_info.wf_file_buffer +
			    wf_lut_convert_order((char*)g_buffer_info.wf_file_buffer
			    + WAVEFORM_ADDR_OFFSET_TO_BEGIN +
			    i * WAVEFORM_ADDR_OFFSET_PER_TEMP + 4 * j));
			#if 1
			g_waveform_table[i][j].len =
			wf_lut_convert_order((char*)g_buffer_info.wf_file_buffer
			+ WAVEFORM_LEN_OFFSET_TO_BEGIN +
		    i * WAVEFORM_LEN_OFFSET_PER_TEMP + 4 * j) / 0x100;
			#else
			g_waveform_table[i][j].len = 10 - j;
			#endif
		}
	}
	#endif

	TCON_LOG("wf_lut_waveform_table_init");
}


void wf_lut_config_context_test_without_trigger(struct cmdq_pkt *pkt,
		struct wf_lut_con_config *wf_lut_config)
{
	wf_lut_waveform_table_init();

	wf_lut_config_mmsys(pkt,wf_lut_config);

	wf_lut_config_waveform(pkt,wf_lut_config->waveform_table_current);

	wf_lut_config_link_mode(pkt,wf_lut_config);

	wf_lut_config_base_addr(pkt,wf_lut_config);

	wf_lut_config_mout(pkt,wf_lut_config);

	wf_lut_config_common(pkt,wf_lut_config);

	wf_lut_config_inter_rdma(pkt,wf_lut_config);

	wf_lut_config_lut_con(pkt,wf_lut_config);

	wf_lut_enable(pkt,wf_lut_config);

	/*wdma*/
	if (wf_lut_config->rg_8b_out) {
		if (wf_lut_config->wf_lut_mout == WF_LUT_MOUT_WDMA) {
			Wf_Lut_Wdma_Config(pkt,wf_lut_config->width/4,wf_lut_config->height);
		}

		/*rdma*/
		wf_lut_rdma_config_rdma(pkt, wf_lut_config->width/4, wf_lut_config->height, 2);

		/*dpi config*/
		wf_lut_config_dpi_context(pkt,wf_lut_config->width/4,
			wf_lut_config->height);
	} else {
		if (wf_lut_config->wf_lut_mout == WF_LUT_MOUT_WDMA) {
			Wf_Lut_Wdma_Config(pkt,wf_lut_config->width/8,wf_lut_config->height);
		}
		
		/*rdma*/
		wf_lut_rdma_config_rdma(pkt, wf_lut_config->width/8, wf_lut_config->height, 2);

		/*dpi config*/
		wf_lut_config_dpi_context(pkt,wf_lut_config->width/8,
			wf_lut_config->height);
	}

	pp_write(NULL, PAPET_TCTOP_TCON_POS_CFG, 10<<16 | 10);

	pp_write_mask(NULL, WF_LUT_DATAPATH_CON, 0x1<<25, BIT_MASK(25));

	//pp_write_mask(NULL, PAPER_TCTOP_IRQ_CTL, 0x1<<2, BIT_MASK(2));

	//pp_write_mask(NULL, PAPER_TCTOP_IRQ_CTL, 0x1<<5, BIT_MASK(5));


}

void wf_lut_set_lut_info(struct cmdq_pkt *pkt,int lut_id,int x,int y,int w,int h)
{
	pp_write(pkt,WF_LUT_INFO_XY_CFG, y|(x<<16)); //bit0-12:y bit13-15:id_2_0 bit16-28:x bit29-31:id_5_3 

	pp_write(pkt,WF_LUT_INFO_WH_CFG, h|(w<<16)); //bit0-12:h bit13-15:id_2_0 bit16-28:w bit29-31:id_5_3

}

void wf_lut_set_lut_id_info(struct cmdq_pkt *pkt,int lut_id,int lut_mode)
{
	pp_write(pkt, WF_LUT_INFO_ID_CFG, lut_id << 4 | lut_mode);
}


void wf_lut_config_context_init_for_pipeline(void)
{
	struct wf_lut_con_config wf_lut_config = {0x00};

	if (platform->PANEL_8_BIT)
		wf_lut_common_8bit_setting(&wf_lut_config);
	else
		wf_lut_common_16bit_setting(&wf_lut_config);

	wf_lut_config.wf_lut_mout = WF_LUT_MOUT_DPI;

	/* config TCON module */
	tcon_setting(NULL);

	/*special config should below common setting, because will memset in common setting*/
	wf_lut_config_context_test_without_trigger(NULL,&wf_lut_config);

}

static void wf_lut_waveform_replace(struct cmdq_pkt *pkt,
		int hw_slot, int waveform_mode)
{
	struct wf_lut_waveform *current_waveform = NULL;

	current_waveform = &g_waveform_table[g_current_temperature][waveform_mode];

	switch (hw_slot) {
	case 0:
		pp_write(pkt, WF_LUT_ADDR_0,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_0, current_waveform->len);
		break;
	case 1:
		pp_write(pkt, WF_LUT_ADDR_1,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_1, current_waveform->len);
		break;
	case 2:
		pp_write(pkt, WF_LUT_ADDR_2,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_2, current_waveform->len);
		break;
	case 3:
		pp_write(pkt, WF_LUT_ADDR_3,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_3, current_waveform->len);
		break;
	case 4:
		pp_write(pkt, WF_LUT_ADDR_4,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_4, current_waveform->len);
		break;
	case 5:
		pp_write(pkt, WF_LUT_ADDR_5,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_5, current_waveform->len);
		break;
	case 6:
		pp_write(pkt, WF_LUT_ADDR_6,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_6, current_waveform->len);
		break;
	case 7:
		pp_write(pkt, WF_LUT_ADDR_7,
			 current_waveform->start_addr);
		pp_write(pkt, WF_LUT_LEN_7, current_waveform->len);
		break;
	default:

		break;
	}


}


void wf_lut_waveform_day_mode_slot_v2(struct cmdq_pkt *pkt)
{
	wf_lut_waveform_replace(pkt, 0, 0);
	wf_lut_waveform_replace(pkt, 1, 1);
	wf_lut_waveform_replace(pkt, 2, 2);
	//wf_lut_waveform_replace(pkt, 3, 10);
	wf_lut_waveform_replace(pkt, 4, 3);
	wf_lut_waveform_replace(pkt, 5, 4);
	wf_lut_waveform_replace(pkt, 6, 6);
	//wf_lut_waveform_replace(pkt, 7, 2);
}

void wf_lut_waveform_night_mode_slot(struct cmdq_pkt *pkt)
{
	wf_lut_waveform_replace(pkt, 0, 0);
	wf_lut_waveform_replace(pkt, 1, 1);
	wf_lut_waveform_replace(pkt, 2, 8);
	//wf_lut_waveform_replace(pkt, 3, 11);
	wf_lut_waveform_replace(pkt, 4, 9);
	wf_lut_waveform_replace(pkt, 5, 9);
	wf_lut_waveform_replace(pkt, 6, 6);
	//wf_lut_waveform_replace(pkt, 7, 8);
}


void TS_WF_LUT_set_lut_info(struct cmdq_pkt *pkt,
	int x, int y, int w, int h,
	int lut_id, int mode)
{
	pp_write_mask(pkt, WF_LUT_EN, 0x1<<0, BIT_MASK(0));
	wf_lut_set_lut_info(pkt,lut_id, x, y, w, h);
	wf_lut_set_lut_id_info(pkt, lut_id, mode);
	pp_write_mask(pkt, WF_LUT_SHADOW_UP, 0x1<<0, BIT_MASK(0));
	tcon_config_global_register(pkt);
	/* always set to 0: dpi clock source use software enable */
	wf_lut_dpi_enable(pkt);
}

void TS_WF_LUT_disable_wf_lut(void)
{
	wf_lut_dpi_disable(NULL);
	tcon_disable(NULL);
}

int wf_lut_wait_end_all_irq(void)
{
	u32 status = 0;

	TCON_LOG("poll WF_LUT frame end all begin");
	while (((((pp_read(WF_LUT_STA) & BIT_MASK(8)) >> 8) == 1)) &&
		status++ < 5000) {
		/* delay 1ms*/
		udelay(1000);
	}

	if (status >= 5000) {
		TCON_ERR("wait poll WF_LUT frame end all timeout reg: 0x%08x",
			pp_read(WF_LUT_STA));
		return -1;
	}

	TCON_LOG("wait WF_LUT frame end all done:0x%x", pp_read(WF_LUT_STA));
	TS_WF_LUT_disable_wf_lut();

	/* clear irq  status */
	
	/* clear wf_lut end all irq status */
	pp_write_mask(NULL, WF_LUT_DATAPATH_CON, 0x1<<24, BIT_MASK(24));
	pp_write_mask(NULL, WF_LUT_DATAPATH_CON, 0x0<<24, BIT_MASK(24));


	/* clear pipeline wb wdma irq status */
	pp_write(NULL, WB_WDMA_INTSTA, 0x0);

	/* clear DPI irq */
	pp_write(NULL, WF_LUT_DPI_INTSTA, 0x0);

	/* clear wf_lut frame done irq status */
	pp_write_mask(NULL, WF_LUT_INTSTA, 0x0, GENMASK(1, 0));

	/* clear WF_LUT lut release irq status */
	pp_write_mask(NULL, WF_LUT_CON, 0x01 << 15, BIT_MASK(15));
	pp_write_mask(NULL, WF_LUT_CON, 0x00 << 15, BIT_MASK(15));

	/* clear TCON end irq status */
	pp_write_mask(NULL, PAPER_TCTOP_IRQ_CTL, 1 << 21, BIT_MASK(21));
	pp_write_mask(NULL, PAPER_TCTOP_IRQ_CTL, 0 << 21, BIT_MASK(21));

	/* clear pixel lut collision irq  status */
	pp_write_mask(NULL, PAPER_TCTOP_IRQ_CTL, 1 << 22, BIT_MASK(22));
	pp_write_mask(NULL, PAPER_TCTOP_IRQ_CTL, 0 << 22, BIT_MASK(22));

	return 0;
}

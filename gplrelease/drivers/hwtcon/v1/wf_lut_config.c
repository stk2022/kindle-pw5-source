#include "v1/wf_lut_config.h"
#include "hwtcon_def.h"
#include "hwtcon_reg.h"
#include "cmdq.h"
#include "panel_setting.h"
#include "rdma_config.h"
#include "v1/pipeline_config.h"
#include "tcon_config.h"
#include "dpi_config.h"
#include "wf_lut_rdma_config.h"

struct wf_lut_con_config g_wf_lut_config = {0x00};

static struct wf_lut_waveform g_waveform_table[TEMPERATURE_NUM][WAVEFORM_MODE_TOTAL_NUM];
static int g_current_temperature = 8;
static u32 g_pipeline_addr = 0;
static unsigned int g_wdma_addr = 0x57000000;

static unsigned int wf_lut_convert_order(unsigned char *addr)
{
	unsigned int value = 0x00;

	value = ((*addr)<<24) + (*(addr+1)<<16) + (*(addr+2)<<8) + *(addr+3);
	return value;
}

static void wf_lut_waveform_table_init(struct wf_lut_con_config *wf_lut_config)
{
	#if 1
	
	int i = 0;
	int j = 0;

	memset((unsigned char *)&g_waveform_table, 0x00,
	       TEMPERATURE_NUM * WAVEFORM_MODE_TOTAL_NUM *
	       sizeof(struct wf_lut_waveform));
	for (i = 0; i < TEMPERATURE_NUM; i++) {
		for (j = 0; j < WAVEFORM_MODE_TOTAL_NUM; j++) {
			g_waveform_table[i][j].waveform_mode = j;
			g_waveform_table[i][j].start_addr =
		    g_pipeline_addr +
		    wf_lut_convert_order((char*)g_pipeline_addr
		    + WAVEFORM_ADDR_OFFSET_TO_BEGIN +
		    i * WAVEFORM_ADDR_OFFSET_PER_TEMP + 4 * j);
			g_waveform_table[i][j].len =
			wf_lut_convert_order((char*)g_pipeline_addr
			+ WAVEFORM_LEN_OFFSET_TO_BEGIN +
		    i * WAVEFORM_LEN_OFFSET_PER_TEMP + 4 * j) / 0x100;
		}
	}

	#endif
}


void wf_lut_clk_adjust(void)
{
	/* syspll1_d4(136.5M) -> univpll_d3(416) */
	pp_write_mask(NULL,0x100000d8,0x7<<24,BIT_MASKS(26,24));
	pp_write_mask(NULL,0x100000d4,0x7<<24,BIT_MASKS(26,24));
	pp_write_mask(NULL,0x10000008,0x1<<7,BIT_MASK(7));

	/* TCON_PLL_D2 -> TCON_PLL_D4 */
	pp_write_mask(NULL,0x100000e8,0x7<<0,BIT_MASKS(2,0));
	pp_write_mask(NULL,0x100000e4,0x2<<0,BIT_MASKS(2,0));
	pp_write_mask(NULL,0x10000008,0x1<<8,BIT_MASK(8));

	/* TCONPLL_CON1 300 -> */
	pp_write(NULL, 0x1000C3A4, platform->clock_setting);
}

/***********************************wf lut***********************************************/
static void wf_lut_config_mmsys(struct cmdq_pkt *pkt,struct wf_lut_con_config *wf_lut_config)
{
	wf_lut_clk_adjust();

	if (wf_lut_config->rg_8b_out)
		pp_write(pkt, MMSYS_MDP_DL_CFG_WD,
			(wf_lut_config->height <<16)|(wf_lut_config->width/4));
	else
		pp_write(pkt, MMSYS_MDP_DL_CFG_WD,
			(wf_lut_config->height <<16)|(wf_lut_config->width/8));
}

static void wf_lut_config_waveform(struct cmdq_pkt *pkt, struct wf_lut_waveform *waveform)
{
	int i = 0;
	struct wf_lut_waveform *current_waveform = waveform;

	for(i=0;i<WAVEFORM_MODE_NUM;i++){
		current_waveform = current_waveform + i;
		switch(current_waveform->waveform_mode)
		{
			case 0:
				pp_write(pkt, WF_LUT_ADDR_0, current_waveform->start_addr);
				pp_write(pkt, WF_LUT_LEN_0, current_waveform->len);
				break;
			case 1:
				pp_write(pkt, WF_LUT_ADDR_1, current_waveform->start_addr);
				pp_write(pkt, WF_LUT_LEN_1, current_waveform->len);
				break;
			case 2:
				pp_write(pkt, WF_LUT_ADDR_2, current_waveform->start_addr);
				pp_write(pkt, WF_LUT_LEN_2, current_waveform->len);
				break;
			case 3:
				pp_write(pkt, WF_LUT_ADDR_3, current_waveform->start_addr);
				pp_write(pkt, WF_LUT_LEN_3, current_waveform->len);
				break;
			case 4:
				pp_write(pkt, WF_LUT_ADDR_4, current_waveform->start_addr);
				pp_write(pkt, WF_LUT_LEN_4, current_waveform->len);
				break;
			case 5:
				pp_write(pkt, WF_LUT_ADDR_5, current_waveform->start_addr);
				pp_write(pkt, WF_LUT_LEN_5, current_waveform->len);
				break;
			case 6:
				pp_write(pkt, WF_LUT_ADDR_6, current_waveform->start_addr);
				pp_write(pkt, WF_LUT_LEN_6, current_waveform->len);
				break;
			case 7:
				pp_write(pkt, WF_LUT_ADDR_7, current_waveform->start_addr);
				pp_write(pkt, WF_LUT_LEN_7, current_waveform->len);
				break;
			default:

				break;

		}
	}
}

static void wf_lut_waveform_replace(struct cmdq_pkt *pkt,
		int old_index, int new_mode)
{
	struct wf_lut_waveform *current_waveform = NULL;

	current_waveform =
		((struct wf_lut_waveform *)
		&g_waveform_table[g_current_temperature][0])
			+new_mode;

	switch (old_index) {
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

void wf_lut_mode_select_config(struct cmdq_pkt *pkt, unsigned int value1, unsigned int value2, unsigned int value3, unsigned int value4,
										unsigned int value5, unsigned int value6, unsigned int value7, unsigned int value8)
{
	pp_write(pkt, WF_MODE_SEL_0, value1);	//lut 0-8 select mode bit0-2 select mode
	pp_write(pkt, WF_MODE_SEL_1, value2);
	pp_write(pkt, WF_MODE_SEL_2, value3);
	pp_write(pkt, WF_MODE_SEL_3, value4);
	pp_write(pkt, WF_MODE_SEL_4, value5);
	pp_write(pkt, WF_MODE_SEL_5, value6);
	pp_write(pkt, WF_MODE_SEL_6, value7);
	pp_write(pkt, WF_MODE_SEL_7, value8);	//lut55-63 select mode bit0-2 select mode

}

static void wf_lut_config_link_mode(struct cmdq_pkt *pkt, struct wf_lut_con_config *wf_lut_config)
{
	/* not setting,direct link alway using this setting*/
	if (wf_lut_config->direct_link == 0) {
		pp_write(pkt, WF_LUT_LINK_MODE, 0x00000002);
	}
	else {
		pp_write(pkt, WF_LUT_LINK_MODE, 0xE4380ff2);
	}
}

static void wf_lut_config_base_addr(struct cmdq_pkt *pkt, struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_LINK_BASE_ADDR, wf_lut_config->base_addr);
	pp_write(pkt, WF_LUT_LINK_BASE_ADDR_1, wf_lut_config->base_addr1);
}

static void wf_lut_config_lut_con(struct cmdq_pkt *pkt, struct wf_lut_con_config *wf_lut_config)
{
	pp_write_mask(pkt, WF_LUT_CON, wf_lut_config->gray_mode, BIT_MASKS(2,0));
	pp_write_mask(pkt, WF_LUT_CON, wf_lut_config->rg_8b_out<<12, BIT_MASK(12));
	pp_write_mask(pkt, WF_LUT_CON, wf_lut_config->rg_partial_up_en<<19, BIT_MASK(19));
	pp_write_mask(pkt, WF_LUT_CON, wf_lut_config->rg_partial_up_val<<20, BIT_MASKS(23,20));
	pp_write_mask(pkt, WF_LUT_CON, wf_lut_config->rg_default_val<<8, BIT_MASKS(11,8));
}

static void wf_lut_config_mout(struct cmdq_pkt *pkt, struct wf_lut_con_config *wf_lut_config)
{
	if (wf_lut_config->rg_8b_out) {
		pp_write(pkt, WF_LUT_MOUT, 0x00107001);
		pp_write_mask(pkt, WF_LUT_MOUT, wf_lut_config->wf_lut_mout, BIT_MASKS(1,0));
	} else {
		pp_write(pkt, WF_LUT_MOUT, 0x00107241);
		pp_write_mask(pkt, WF_LUT_MOUT, wf_lut_config->wf_lut_mout, BIT_MASKS(1,0));
	}
}

static void wf_lut_config_common(struct cmdq_pkt *pkt, struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_ROI_SIZE, (wf_lut_config->height<< 16) | wf_lut_config->width);
	pp_write(pkt, WF_LUT_SRC_CON,  wf_lut_config->rdma_enable_mask & 0xf);
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, wf_lut_config->layer_greq_num<<26, BIT_MASKS(31,26));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, wf_lut_config->checksum_sel<<8, BIT_MASKS(11,8));
	/* using wf_lut checksum */
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, 0x1<<11, BIT_MASK(11));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, wf_lut_config->rg_lut_end_sel<<7, BIT_MASK(7));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, wf_lut_config->rg_de_sel<<6, BIT_MASK(6));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, wf_lut_config->checksum_en<<4, BIT_MASK(4));
	pp_write_mask(pkt, WF_LUT_DATAPATH_CON, wf_lut_config->layer_smi_id_en, BIT_MASK(0));
	/* pp_write(pkt, WF_LUT_DATAPATH_CON, 0x40000891); */

}

static void wf_lut_config_inter_rdma(struct cmdq_pkt *pkt, struct wf_lut_con_config *wf_lut_config)
{
	unsigned int rdma_control_value = 0x0;

	/* bit8 need set to 1 */
	rdma_control_value = (wf_lut_config->byte_swap<<24) | (wf_lut_config->DECFMT<<12) |
							(wf_lut_config->H_FLIP_EN << 10) | (wf_lut_config->V_FLIP_EN << 9)|0x00000100;

	pp_write(pkt, WF_LUT_L0_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L1_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L2_CON, rdma_control_value);
	pp_write(pkt, WF_LUT_L3_CON, rdma_control_value);

	pp_write(pkt, WF_LUT_L0_SRC_SIZE, wf_lut_config->wb_rdma[0].height<<16|wf_lut_config->wb_rdma[0].width);
	pp_write(pkt, WF_LUT_L0_OFFSET, wf_lut_config->wb_rdma[0].y<<16|wf_lut_config->wb_rdma[0].x);
	pp_write(pkt, WF_LUT_L0_ADDR, wf_lut_config->wb_rdma[0].start_addr);
	pp_write(pkt, WF_LUT_L0_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA0_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA0_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA0_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA0_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L1_SRC_SIZE, wf_lut_config->wb_rdma[1].height<<16|wf_lut_config->wb_rdma[1].width);
	pp_write(pkt, WF_LUT_L1_OFFSET, wf_lut_config->wb_rdma[1].y<<16|wf_lut_config->wb_rdma[1].x);
	pp_write(pkt, WF_LUT_L1_ADDR, wf_lut_config->wb_rdma[1].start_addr);
	pp_write(pkt, WF_LUT_L1_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA1_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA1_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA1_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA1_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L2_SRC_SIZE, wf_lut_config->wb_rdma[2].height<<16|wf_lut_config->wb_rdma[2].width);
	pp_write(pkt, WF_LUT_L2_OFFSET, wf_lut_config->wb_rdma[2].y<<16|wf_lut_config->wb_rdma[2].x);
	pp_write(pkt, WF_LUT_L2_ADDR, wf_lut_config->wb_rdma[2].start_addr);
	pp_write(pkt, WF_LUT_L2_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA2_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA2_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA2_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA2_FIFO_CTRL, 0x00800000);

	pp_write(pkt, WF_LUT_L3_SRC_SIZE, wf_lut_config->wb_rdma[3].height<<16|wf_lut_config->wb_rdma[3].width);
	pp_write(pkt, WF_LUT_L3_OFFSET, wf_lut_config->wb_rdma[3].y<<16|wf_lut_config->wb_rdma[3].x);
	pp_write(pkt, WF_LUT_L3_ADDR, wf_lut_config->wb_rdma[3].start_addr);
	pp_write(pkt, WF_LUT_L3_PITCH, wf_lut_config->width * 2);
	pp_write(pkt, WF_LUT_RDMA3_CTRL, 0x00000001);
	pp_write(pkt, WF_LUT_RDMA3_MEM_GMC_SETTING1, 0x00005860);
	pp_write(pkt, WF_LUT_RDMA3_MEM_SLOW_CON, 0x00100000);
	pp_write(pkt, WF_LUT_RDMA3_FIFO_CTRL, 0x00800000);

}

static void wf_lut_enable(struct cmdq_pkt *pkt, struct wf_lut_con_config *wf_lut_config)
{
	pp_write(pkt, WF_LUT_INTEN, wf_lut_config->wf_lut_inten);
	/* for ctp same */
	pp_write_mask(pkt, WF_LUT_EN, wf_lut_config->wf_lut_en,BIT_MASK(0));
	//pp_write(pkt, WF_LUT_EN, wf_lut_config->wf_lut_en);
}

static void Wf_Lut_Wdma_addr(struct cmdq_pkt *pkt,uint32_t addr)
{
	pp_write(pkt, WDMA_DST_ADDR0, addr);  //WDMA_DST_ADDR0
}

static void Wf_Lut_Wdma_Config(struct cmdq_pkt *pkt,uint32_t width, uint32_t height)
{
	pp_write_mask(pkt, DISP_WDMA0_SEL_IN, 0x1, BIT_MASK(0));
	pp_write(pkt, WDMA_INTEN, 0x00000001);
	pp_write(pkt, WDMA_CFG, 0x02020030); 
	pp_write(pkt, WDMA_DST_W_IN_BYTE, 4*width);
	pp_write(pkt, WDMA_DST_UV_PITCH, width);
	pp_write(pkt, WDMA_SRC_SIZE, height<<16|width);
	pp_write(pkt, WDMA_CLIP_SIZE, height<<16|width);
	pp_write(pkt, WDMA_CLIP_COORD, 0x00000000);

	Wf_Lut_Wdma_addr(pkt, g_wdma_addr);

	/* open module CG */
	pp_write(pkt, WDMA_EN, 0xC0000001);
	//pp_write_mask(pkt, WDMA_EN, 0x1, BIT_MASK(0));
}

void wf_lut_config_context(struct cmdq_pkt *pkt,struct wf_lut_con_config *wf_lut_config)
{
	wf_lut_waveform_table_init(wf_lut_config);

	tcon_setting(pkt);

	wf_lut_config_mmsys(pkt,wf_lut_config);

	wf_lut_config_waveform(pkt,wf_lut_config->waveform_table_current);

	//wf_lut_config_lut_enable(pkt);

	wf_lut_config_link_mode(pkt,wf_lut_config);

	wf_lut_config_base_addr(pkt,wf_lut_config);

	/*lut enable info from pipeline in direct link mode, not need config*/

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
		wf_lut_rdma_config_rdma(pkt,wf_lut_config->width/4,wf_lut_config->height,2);

		/*dpi config*/
		wf_lut_config_dpi_context(pkt,wf_lut_config->width/4,wf_lut_config->height);
	} else {
		if (wf_lut_config->wf_lut_mout == WF_LUT_MOUT_WDMA) {
			Wf_Lut_Wdma_Config(pkt,wf_lut_config->width/8,wf_lut_config->height);
		}
		
		/*rdma*/
		wf_lut_rdma_config_rdma(pkt,wf_lut_config->width/8,wf_lut_config->height,2);
		
		/*dpi config*/
		wf_lut_config_dpi_context(pkt,wf_lut_config->width/8,wf_lut_config->height);
	}

}


void wf_lut_pipeline_8bit_setting(unsigned int base_addr, unsigned int base_addr1)
{
	g_wf_lut_config.base_addr = base_addr;
	g_wf_lut_config.base_addr1 = base_addr1;
	g_wf_lut_config.gray_mode = GRAY_MODE_32_GRAY_LEVEL;
	g_wf_lut_config.width = platform->PANEL_WIDTH;
	g_wf_lut_config.height = platform->PANEL_HEIGHT;
	g_wf_lut_config.rdma_enable_mask = 0xf;
	g_wf_lut_config.DECFMT = 0x0;
	g_wf_lut_config.checksum_en = 0x01;
	g_wf_lut_config.H_FLIP_EN = 0x00;
	g_wf_lut_config.V_FLIP_EN = 0x00;
	g_wf_lut_config.wf_lut_en = 0x01;
	g_wf_lut_config.wf_lut_inten = 0x02;
	g_wf_lut_config.rg_default_val = 0x00;
	g_wf_lut_config.rg_partial_up_en = 0x00;
	g_wf_lut_config.rg_partial_up_val = 0x00;
	g_wf_lut_config.layer_greq_num = 0x10;
	g_wf_lut_config.checksum_sel = 0x00;
	g_wf_lut_config.layer_smi_id_en = 0x01;
	g_wf_lut_config.rg_8b_out = 0x01;
	g_wf_lut_config.byte_swap = 0x02;
	g_wf_lut_config.direct_link = 0x01;
	g_wf_lut_config.rg_lut_end_sel = 0x01;
	g_wf_lut_config.dpi_enable_mode = 0x01;

	/*bit 0 output to dpi,bit1 output to wdma */
	g_wf_lut_config.wf_lut_mout = WF_LUT_MOUT_WDMA;
	g_wf_lut_config.temperature_index = g_current_temperature;
	g_wf_lut_config.waveform_table_current =
	    (struct wf_lut_waveform *)&g_waveform_table[g_wf_lut_config.temperature_index][0];

}


void wf_lut_pipeline_16bit_setting(unsigned int base_addr, unsigned int base_addr1)
{
	g_wf_lut_config.base_addr = base_addr;
	g_wf_lut_config.base_addr1 = base_addr1;
	g_wf_lut_config.gray_mode = GRAY_MODE_32_GRAY_LEVEL;
	g_wf_lut_config.width = platform->PANEL_WIDTH;
	g_wf_lut_config.height = platform->PANEL_HEIGHT;
	g_wf_lut_config.rdma_enable_mask = 0xf;
	g_wf_lut_config.DECFMT = 0x0;
	g_wf_lut_config.checksum_en = 0x01;
	g_wf_lut_config.H_FLIP_EN = 0x00;
	g_wf_lut_config.V_FLIP_EN = 0x00;
	g_wf_lut_config.wf_lut_en = 0x01;
	g_wf_lut_config.wf_lut_inten = 0x02;
	g_wf_lut_config.rg_default_val = 0x00;
	g_wf_lut_config.rg_partial_up_en = 0x00;
	g_wf_lut_config.rg_partial_up_val = 0x00;
	g_wf_lut_config.layer_greq_num = 0x10;
	g_wf_lut_config.checksum_sel = 0x00;
	g_wf_lut_config.layer_smi_id_en = 0x01;
	g_wf_lut_config.rg_8b_out = 0x00;
	g_wf_lut_config.byte_swap = 0x02;
	g_wf_lut_config.direct_link = 0x01;
	g_wf_lut_config.rg_lut_end_sel = 0x01;
	g_wf_lut_config.dpi_enable_mode = 0x01;

	/*bit 0 output to dpi,bit1 output to wdma */
	g_wf_lut_config.wf_lut_mout = WF_LUT_MOUT_WDMA;
	g_wf_lut_config.temperature_index = g_current_temperature;
	g_wf_lut_config.waveform_table_current =
	    (struct wf_lut_waveform *)&g_waveform_table[g_wf_lut_config.temperature_index][0];

}

void wf_lut_for_pipeline_interface(unsigned int base_addr, unsigned int base_addr1,
	unsigned int wdma_addr, unsigned int waveform_addr,
	unsigned int partial_update,
	enum WF_LUT_MOUT_ENUM mout, unsigned int rg_8b_out)
{
	g_wdma_addr = wdma_addr;

	g_pipeline_addr = waveform_addr;

	TCON_LOG("wf_lut_for_pipeline_interface==>");


	if (rg_8b_out) {
		wf_lut_pipeline_8bit_setting(base_addr,base_addr1);

		/*partial update:1;full_update:0*/
		g_wf_lut_config.rg_partial_up_en = partial_update;

		/* if to dpi, this need to set 0x01, if to wdma, this need to set 0x02 */
		g_wf_lut_config.wf_lut_mout = mout;
		
		/*special config should below common setting, because will memset in common setting*/
		wf_lut_config_context(NULL,&g_wf_lut_config);
	} else {
		wf_lut_pipeline_16bit_setting(base_addr,base_addr1);

		/* partial update:1;full_update:0 */
		g_wf_lut_config.rg_partial_up_en = partial_update;

		/* if to dpi, this need to set 0x01, if to wdma, this need to set 0x02 */
		g_wf_lut_config.wf_lut_mout = mout;
		
		/*special config should below common setting, because will memset in common setting*/
		wf_lut_config_context(NULL,&g_wf_lut_config);
	}
}


void wf_lut_wait_for_framedone_v1(void)
{
#define MAX_WAIT_COUNT 1000
	int i = 0;

	TCON_LOG("begin to wait 0x%x 0x%x", pp_read(PIPELINT_LUT_STATUS0), pp_read(PIPELINT_LUT_STATUS1));	
	while (i++ < MAX_WAIT_COUNT) {
		if (pp_read(PIPELINT_LUT_STATUS0) == 0x00 &&
			pp_read(PIPELINT_LUT_STATUS1) == 0x00) {
			break;
		}
		mdelay(10);
	}
	if (i > MAX_WAIT_COUNT)
		TCON_ERR("wait framedone timeout: 0x%08x 0x%08x",
			pp_read(PIPELINT_LUT_STATUS0), pp_read(PIPELINT_LUT_STATUS1));
	else
		TCON_LOG("wait framedone OK: 0x%08x 0x%08x",
			pp_read(PIPELINT_LUT_STATUS0), pp_read(PIPELINT_LUT_STATUS1));
	mdelay(12);

	tcon_disable(NULL);

	/* clear irq  status */
	/* wb wdma */
	pp_write(NULL, PP_WDMA_INTSTA, 0x0);
	/* DPI */
	pp_write(NULL, WF_LUT_DPI_INTSTA, 0x0);

	/* LUT irq */
	pp_write_mask(NULL, WF_LUT_CON, 0x01 << 15, BIT_MASK(15));
	pp_write_mask(NULL, WF_LUT_CON, 0x00 << 15, BIT_MASK(15));
}

void wf_lut_waveform_day_mode_slot_v1(struct cmdq_pkt *pkt)
{
	wf_lut_waveform_replace(pkt, 0, 0);
	wf_lut_waveform_replace(pkt, 1, 1);
	wf_lut_waveform_replace(pkt, 2, 2);
	wf_lut_waveform_replace(pkt, 3, 10);
	wf_lut_waveform_replace(pkt, 4, 3);
	wf_lut_waveform_replace(pkt, 5, 4);
	wf_lut_waveform_replace(pkt, 6, 6);
	wf_lut_waveform_replace(pkt, 7, 2);
}

int wf_lut_map_waveform_with_index_v1(
	enum WAVEFORM_MODE_ENUM wf_mode,
	int night_mode)
{
	if (wf_mode == WAVEFORM_MODE_AUTO)
		return 0xF;

	if (!night_mode)	/* day mode */
		switch (wf_mode) {
		case WAVEFORM_MODE_INIT:
			return 0;
		case WAVEFORM_MODE_DU:
			return 1;
		case WAVEFORM_MODE_GC16:
			return 2;
		case WAVEFORM_MODE_GC16_PARTIAL:
			return 3;
		case WAVEFORM_MODE_GL16:
			return 4;
		case WAVEFORM_MODE_GLR16:
			return 5;
		case WAVEFORM_MODE_A2:
			return 6;
		default:
			TCON_ERR("invalid waveform mode:%d for day mode",
				wf_mode);
			return 0;	/* force white screen */
		}
	else	/* night mode */
		switch (wf_mode) {
		case WAVEFORM_MODE_INIT:
			return 0;
		case WAVEFORM_MODE_DU:
			return 1;
		case WAVEFORM_MODE_GCK16:
			return 2;
		case WAVEFORM_MODE_GCK16_PARTIAL:
			return 3;
		case WAVEFORM_MODE_GLKW16:
			return 4;
		//case WAVEFORM_MODE_GLKW16:
		//	return 5;
		case WAVEFORM_MODE_A2:
			return 6;
		default:
			TCON_ERR("invalid waveform mode:%d for night mode",
				wf_mode);
			return 0;	/* force white screen */
		}

	return 0;
}



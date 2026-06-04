#include "hwtcon_hal.h"
#include "hwtcon_reg.h"
#include "wf_lut_rdma_config.h"
#include "hwtcon.h"
#include "rdma_config.h"

void wf_lut_rdma_config_rdma(struct cmdq_pkt *pkt, unsigned int width,
			     unsigned int height,
			     unsigned int inputFormat)
{
	struct rdma_global_config global_config = { 0 };

	global_config.ENGINE_EN = true;
	global_config.MODE_SEL = false;
	global_config.SOFT_RESET = false;
	pp_write(pkt, WF_LUT_RDMA_GLOBAL_CON,
		 global_config.SOFT_RESET << 4 |
		 global_config.MODE_SEL << 1 | global_config.ENGINE_EN << 0);
	//pp_write_v2(PP_IMG_RDMA_GLOBAL_CON, 0x0103);      why need to set bit7

	/* OUTPUT_FRAME_WIDTH */
	pp_write_mask(pkt, WF_LUT_RDMA_SIZE_CON_0, width, BIT_MASKS(12, 0));
	/* OUTPUT_FRAME_HEIGHT */
	pp_write_mask(pkt, WF_LUT_RDMA_SIZE_CON_1, height, BIT_MASKS(19, 0));
	/* INPUT FORMAT */
	pp_write_mask(pkt, WF_LUT_RDMA_MEM_CON,
		      inputFormat << 4, BIT_MASKS(7, 4));

	/* MEM_MODE_SRC_PITCH */
	pp_write_mask(pkt, WF_LUT_RDMA_MEM_SRC_PITCH,
		      4 * width, BIT_MASKS(15, 0));
	pp_write(pkt, WF_LUT_RDMA_MEM_GMC_SETTING_0, 0x20202040);
	pp_write(pkt, WF_LUT_RDMA_MEM_GMC_SETTING_1, 0x00000020);
	pp_write(pkt, WF_LUT_RDMA_FIFO_CON, 0x01000010);
	pp_write(pkt, WF_LUT_RDMA_INT_ENABLE, 0x00000004);
}

void wf_lut_clear_disp_rdma_irq_status(struct cmdq_pkt *pkt)
{
	pp_write_mask(pkt, WF_LUT_RDMA_INT_STATUS, 0x00 << 2, BIT_MASK(2));
}


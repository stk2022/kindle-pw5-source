#include "rdma_config.h"
#include "hwtcon_reg.h"
#include "cmdq.h"


void config_image_rdma(struct cmdq_pkt *pkt)
{
	struct rdma_global_config global_config = {0};

	global_config.ENGINE_EN = true;
	global_config.MODE_SEL = true;
	global_config.SOFT_RESET = false;
	pp_write(pkt, PP_IMG_RDMA_GLOBAL_CON,
		global_config.SOFT_RESET << 4 |
		global_config.MODE_SEL << 1 |
		global_config.ENGINE_EN << 0);
	//pp_write(PP_IMG_RDMA_GLOBAL_CON, 0x0103);	 why need to set bit7

	pp_write(pkt, PP_IMG_RDMA_SIZE_CON_0, 0x02000029);	/* OUTPUT_FRAME_WIDTH */
	pp_write(pkt, PP_IMG_RDMA_SIZE_CON_1, 0x02000029);	/* OUTPUT_FRAME_HEIGHT */
	pp_write(pkt, PP_IMG_RDMA_MEM_SRC_PITCH, 0x00000029);	/* MEM_MODE_SRC_PITCH */
	pp_write(pkt, PP_IMG_RDMA_FIFO_CON, 0x01000010);
	pp_write(pkt, PP_IMG_RDMA_FIFO_CON, 0x01000001);
}

void config_wb_rdma(struct cmdq_pkt *pkt)
{
	struct rdma_global_config global_config = {0};

	global_config.ENGINE_EN = true;
	global_config.MODE_SEL = true;
	global_config.SOFT_RESET = false;
	pp_write(pkt, PP_WB_RDMA_GLOBAL_CON,
		global_config.SOFT_RESET << 4 |
		global_config.MODE_SEL << 1 |
		global_config.ENGINE_EN << 0);
	//pp_write(PP_WB_RDMA_GLOBAL_CON, 0x00000103);

	pp_write(pkt, PP_WB_RDMA_SIZE_CON_0, 0x02000029);
	pp_write(pkt, PP_WB_RDMA_SIZE_CON_1, 0x02000029);
	pp_write(pkt, PP_WB_RDMA_MEM_SRC_PITCH, 0x00000029);
}


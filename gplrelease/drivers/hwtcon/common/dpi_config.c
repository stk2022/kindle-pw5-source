#include "cmdq.h"
#include "hwtcon_def.h"
#include "hwtcon_reg.h"
#include "dpi_config.h"
#include "panel_setting.h"

void wf_lut_dpi_checksum_config(struct cmdq_pkt *pkt,unsigned int enable)
{
	pp_write_mask(pkt, WF_LUT_DPI_CHECKSUM, enable<<31, BIT_MASK(31));
}

void wf_lut_config_dpi(struct cmdq_pkt *pkt,struct dpi_timing_para* dpi_para)
{
	unsigned int dpi_size=(dpi_para->height<<16)|dpi_para->width;

	pp_write(pkt, WF_LUT_DPI_INTEN, 0x01);	/* IRQ Enable */

	pp_write_mask(pkt, WF_LUT_DPI_OUTPUT_SETTING, dpi_para->CK_POL<<15, BIT_MASK(15));

	pp_write(pkt, WF_LUT_DPI_SIZE, dpi_size);	/* OUTPUT_FRAME_WIDTH */
	pp_write_mask(pkt, WF_LUT_DPI_TGEN_HWIDTH, dpi_para->HSA, BIT_MASKS(11,0));
	pp_write(pkt, WF_LUT_DPI_TGEN_HPORCH, (dpi_para->HFP<<16)|dpi_para->HBP);
	pp_write_mask(pkt, WF_LUT_DPI_TGEN_VWIDTH, dpi_para->VSA, BIT_MASKS(11,0));
	pp_write(pkt, WF_LUT_DPI_TGEN_VPORCH, (dpi_para->VFP<<16)|dpi_para->VBP);
	pp_write(pkt, WF_LUT_DPI_MUTEX_VSYNC_SETTING, 0x00000101);
}

void wf_lut_dpi_enable(struct cmdq_pkt *pkt)
{
	/* dpi enable mode:
	 * 0: DPI use sw config  enable
	 */
	pp_write_mask(pkt, PAPER_TCTOP_DPI_CFG, 0x1<<1, BIT_MASK(1));

	pp_write(pkt, PAPER_TCTOP_SOF_CTL, 0x00200240); //?no need?
	pp_write(pkt, WF_LUT_DPI_EN, 0x01);
}

void wf_lut_dpi_disable(struct cmdq_pkt *pkt)
{
	pp_write(pkt, WF_LUT_DPI_EN, 0x00);
}

void wf_lut_config_dpi_context(struct cmdq_pkt *pkt,
	unsigned int width, unsigned int height)
{
	struct dpi_timing_para dpi_para = {0x00};

	dpi_para.height = height;
	dpi_para.width = width;

	dpi_para.HSA = platform->DPI_HSA;
	dpi_para.HFP = platform->DPI_HFP;
	dpi_para.HBP = platform->DPI_HBP;
	dpi_para.VSA = platform->DPI_VSA;
	dpi_para.VFP = platform->DPI_VFP;
	dpi_para.VBP = platform->DPI_VBP;
	dpi_para.CK_POL = platform->DPI_CK_POL;	

	wf_lut_dpi_checksum_config(pkt,0x01);

	wf_lut_config_dpi(pkt,&dpi_para);
}


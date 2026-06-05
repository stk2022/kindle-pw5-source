#ifndef __DPI_CONFIG_H__
#define __DPI_CONFIG_H__
#include "cmdq.h"

struct dpi_timing_para {
	unsigned int width;
	unsigned int height;
	unsigned int HSA;
	unsigned int HFP;
	unsigned int HBP;
	unsigned int VSA;
	unsigned int VFP;
	unsigned int VBP;
	unsigned int CK_POL;
};

void wf_lut_dpi_enable(struct cmdq_pkt *pkt);
void wf_lut_dpi_disable(struct cmdq_pkt *pkt);
void wf_lut_config_dpi_context(struct cmdq_pkt *pkt,
	unsigned int width, unsigned int height);


#endif /* endof __DPI_CONFIG_H__ */

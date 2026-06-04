#ifndef __WDMA_CONFIG_H__
#define __WDMA_CONFIG_H__
#include "hwtcon_def.h"
#include "cmdq.h"


void config_wb_wdma(struct cmdq_pkt *pkt, int panel_width, int panel_height, u32 addr);
#endif /* endof __WDMA_CONFIG_H__ */

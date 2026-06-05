#ifndef __WF_LUT_CONFIG_H__
#define __WF_LUT_CONFIG_H__
#include "cmdq.h"
#include "hwtcon_def.h"
#include "hwtcon_def.h"
#include "wf_lut_common_config.h"

int wf_lut_map_waveform_with_index_v1(
	enum WAVEFORM_MODE_ENUM wf_mode,
	int night_mode);
void wf_lut_waveform_day_mode_slot_v1(struct cmdq_pkt *pkt);
void wf_lut_wait_for_framedone_v1(void);
void wf_lut_for_pipeline_interface(unsigned int base_addr, unsigned int base_addr1,
	unsigned int wdma_addr, unsigned int waveform_addr,
	unsigned int partial_update,
	enum WF_LUT_MOUT_ENUM mout, unsigned int rg_8b_out);




#endif /* endof __WF_LUT_CONFIG_H__ */

#ifndef __HWTCON_WF_LUT_CONFIG_H__
#define __HWTCON_WF_LUT_CONFIG_H__

#include "hwtcon_hal.h"
#include "cmdq.h"
#include "hwtcon_def.h"
#include "wf_lut_common_config.h"

void TS_WF_LUT_set_lut_info(struct cmdq_pkt *pkt,
	int x, int y, int w, int h,
	int lut_id, int mode);
void wf_lut_config_context_init_for_pipeline(void);
void TS_WF_LUT_disable_wf_lut(void);
int wf_lut_wait_end_all_irq(void);
void wf_lut_waveform_day_mode_slot_v2(struct cmdq_pkt *pkt);
void wf_lut_waveform_night_mode_slot(struct cmdq_pkt *pkt);


#endif /* endof __HWTCON_WF_LUT_CONFIG_H__ */

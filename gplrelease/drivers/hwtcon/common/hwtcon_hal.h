#ifndef __HWTCON_HAL_H__
#define __HWTCON_HAL_H__
#include "cmdq.h"

enum HW_VERSION_ENUM {
	HW_VERSION_MT8110_1 = 0xCA00,
	HW_VERSION_MT8110_2 = 0xCA01,
	HW_VERSION_MT8113 = 0xCA02,
};

u32 pp_read(u32 va);
void pp_write(struct cmdq_pkt *pkt, u32 va, u32 value);
void pp_write_mask(struct cmdq_pkt *pkt, u32 va, u32 value, u32 mask);
enum HW_VERSION_ENUM hwtcon_get_hw_ver(void);
void rdma_config_smi_setting(struct cmdq_pkt *pkt);


#endif /* endof __HWTCON_HAL_H__ */

#ifndef __TCON_CONFIG_H__
#define __TCON_CONFIG_H__

#include "cmdq.h"

struct tcon_timing_para {
	unsigned int VS;
	unsigned int VE;
	unsigned int HS;
	unsigned int HE;
	unsigned int TCOPR;
	unsigned int INV;
	unsigned int VACTSEL;
	unsigned int HSPLCNT;
 };

void tcon_setting(struct cmdq_pkt *pkt);
void tcon_disable(struct cmdq_pkt *pkt);

void tcon_config_global_register(struct cmdq_pkt *pkt);

#endif /* endof __TCON_CONFIG_H__ */


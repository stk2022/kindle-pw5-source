#include "hwtcon_hal.h"

u32 pp_read(u32 va)
{
	return *((volatile u32 *)va);
}

void pp_write(struct cmdq_pkt *pkt, u32 va, u32 value)
{
	if (!pkt) {
		TCON_LOG("reg write: addr:0x%x value:0x%x", va, value);
		*((volatile u32 *)va) = value;
	} else {
		/* use gce */
		cmdq_pkt_assign_command(pkt, GCE_SPR0, va);
		cmdq_pkt_store_value(pkt, GCE_SPR0, value, 0xFFFFFFFF);
	}
}

void pp_write_mask(struct cmdq_pkt *pkt, u32 va, u32 value, u32 mask)
{
	if (!pkt) {
		/* only update mask bit = 1, for 0 case, do not update. */
		u32 read_back = pp_read(va);
		TCON_LOG("reg_write_mask: addr:0x%x value:0x%x mask:0x%x readback:0x%x", va, value, mask, read_back);
		pp_write(pkt, va, (read_back & ~mask) |(value & mask));
	} else {
		/* use gce */
		cmdq_pkt_assign_command(pkt, GCE_SPR0, va);
		cmdq_pkt_store_value(pkt, GCE_SPR0, value, mask);
	}
}

enum HW_VERSION_ENUM hwtcon_get_hw_ver(void)
{
	u32 hw_ver_reg = 0x08000008;

	return pp_read(hw_ver_reg);
}

void rdma_config_smi_setting(struct cmdq_pkt *pkt)
{
	u32 i = 0;

	/* smi common */
	pp_write(pkt, 0x140021a0, 0x1);
	pp_write(pkt, 0x140021c0, 0x0);
	pp_write(pkt, 0x140021a0, 0x0);
	pp_write(pkt, 0x140021a4, 0x1);
	pp_write(pkt, 0x14002220, 0x4444);

	/* SMI LARB0 */
	pp_write(pkt, 0x14003400, 0x1);
	pp_write(pkt, 0x14003400, 0x0);
	pp_write(pkt, 0x14003404, 0x1);

	for (i = 0; i < 10; i++) {
		pp_write(pkt, 0x14003380 + i * 4, 0);
		pp_write(pkt, 0x14003f80 + i * 4, 0);
	}

	/* SMI LARB1 */
	pp_write(pkt, 0x15002400, 0x1);
	pp_write(pkt, 0x15002400, 0x0);
	pp_write(pkt, 0x15002404, 0x1);
	
	/* memset 0x15002f80 ~ 0x15002fe0 to 0 */
	for (i = 0; i < 18; i++) {
		pp_write(pkt, 0x15002380 + i * 4, 0);
		pp_write(pkt, 0x15002f80 + i * 4, 0);
	}
}



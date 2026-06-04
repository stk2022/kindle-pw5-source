#ifndef __WF_LUT_COMMON_CONFIG_H__
#define __WF_LUT_COMMON_CONFIG_H__

#include "hwtcon_def.h"

#define TEMPERATURE_NUM		32
#define WAVEFORM_MODE_NUM	8
#define WAVEFORM_MODE_TOTAL_NUM			12

#define WAVEFORM_ADDR_OFFSET_TO_BEGIN	0xC0
#define WAVEFORM_LEN_OFFSET_TO_BEGIN	0x8C0
#define WAVEFORM_ADDR_OFFSET_PER_TEMP	0x40
#define WAVEFORM_LEN_OFFSET_PER_TEMP	0x40
#define WAVEFORM_TS_TO_BEGIN			0x80
#define WAVEFORM_TS_NUM_TO_BEGIN		0xA2

enum WF_LUT_MOUT_ENUM {
	WF_LUT_MOUT_DPI = 0x01,
	WF_LUT_MOUT_WDMA = 0x02,
};

enum WAVEFORM_MODE_ENUM {
	WAVEFORM_MODE_INIT = 0,
	WAVEFORM_MODE_DU = 1,
	WAVEFORM_MODE_GC16 = 2,
	WAVEFORM_MODE_GC16_FAST = 2,
	WAVEFORM_MODE_GL16 = 3,
	WAVEFORM_MODE_GL16_FAST = 3,
	WAVEFORM_MODE_GL4 = 3,
	WAVEFORM_MODE_GL16_INV = 3,
	WAVEFORM_MODE_GLR16 = 4,
	WAVEFORM_MODE_REAGL = 4,
	WAVEFORM_MODE_GLD16 = 5,
	WAVEFORM_MODE_REAGLD = 5,
	WAVEFORM_MODE_A2 = 6,
	WAVEFORM_MODE_DU4 = 7,
	WAVEFORM_MODE_LAST = 7,
	WAVEFORM_MODE_GCK16 = 8,
	WAVEFORM_MODE_GLKW16 = 9,
	WAVEFORM_MODE_GC16_PARTIAL = 10,
	WAVEFORM_MODE_GCK16_PARTIAL = 11,
	WAVEFORM_MODE_AUTO = 257,
};

enum WF_MODE_ENUM {
	WF_MODE_0 = 0,
	WF_MODE_1 = 1,
	WF_MODE_2 = 2,
	WF_MODE_3 = 3,
	WF_MODE_4 = 4,
	WF_MODE_5 = 5,
	WF_MODE_6 = 6,
	WF_MODE_7 = 7,
};

struct wf_lut_con_config {
	unsigned int gray_mode;
	unsigned int width;
	unsigned int height;
	unsigned int rdma_enable_mask;
	unsigned int DECFMT;    //decoder format 1T1pixel or 1T2pixel
	unsigned int layer_greq_num;
	unsigned int checksum_sel;
	unsigned int rg_de_sel;
	unsigned int rg_lut_end_sel;
	unsigned int layer_smi_id_en;
	unsigned int checksum_en;
	unsigned int H_FLIP_EN;
	unsigned int V_FLIP_EN;
	unsigned int wf_lut_en;
	unsigned int wf_lut_inten;
	unsigned int base_addr;
	unsigned int base_addr1;
	unsigned int rg_8b_out;
	unsigned int rg_partial_up_en;
	unsigned int rg_partial_up_val;
	unsigned int rg_default_val;
	enum WF_LUT_MOUT_ENUM wf_lut_mout;
	unsigned int byte_swap;
	struct wf_lut_waveform *waveform_table_current;
	unsigned int temperature_index;
	struct wf_lut_wb_rdma wb_rdma[4];
	unsigned int direct_link;
	unsigned int dpi_enable_mode;
	unsigned int checksum_mode;	
};

#endif /* __WF_LUT_COMMON_CONFIG_H__ */

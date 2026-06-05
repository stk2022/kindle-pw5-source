#include "panel_setting.h"

const struct platform_info_struct panel_1264_1680_info = {
	.clock_setting = 0x83147E07,
	.PANEL_WIDTH = 1264,
	.PANEL_HEIGHT = 1680,
	.PANEL_8_BIT = 0,

	/* sdce */
	.TIME0_HS = 57,
	.TIME0_HE = 67,
	.TIME0_VS = 6,
	.TIME0_VE = 1686,
	.TIME0_INV = 1,

	/* sdle */
	.TIME1_HS = 7,
	.TIME1_HE = 43,
	.TIME1_VS = 6,
	.TIME1_VE = 1686,
	.TIME1_INV = 0,

	/* time 2 sdoe all high */
	.TIME2_HS = 0,
	.TIME2_HE = 0,
	.TIME2_VS = 0,
	.TIME2_VE = 0,
	.TIME2_INV = 0,

	/* gdck */
	.TIME3_HS = 49,
	.TIME3_HE = 417,
	.TIME3_VS = 1,
	.TIME3_VE = 1690,
	.TIME3_INV = 0,

	/* time 4 gdoe all high */
	/* gdsp */
	.TIME5_HS = 256,
	.TIME5_HE = 272,
	.TIME5_VS = 2,
	.TIME5_VE = 3,
	.TIME5_HSPLCNT = 7,
	.TIME5_VACTSEL = 1,
	.TIME5_INV = 1,
	.TIME5_TCOPR = 4,

	.DPI_HSA = 18,
	.DPI_HFP = 48,
	.DPI_HBP = 7,
	.DPI_VSA = 2,
	.DPI_VFP = 11,
	.DPI_VBP = 4,
	.DPI_CK_POL = 1,
};

const struct platform_info_struct panel_1448_1072_info = {
	.clock_setting = 0x83189D89,
	.PANEL_WIDTH = 1448,
	.PANEL_HEIGHT = 1072,
	.PANEL_8_BIT = 1,

	/* sdce */
	.TIME0_HS = 51,
	.TIME0_HE = 64,
	.TIME0_VS = 7,
	.TIME0_VE = 1079,
	.TIME0_INV = 1,

	/* sdle */
	.TIME1_HS = 7,
	.TIME1_HE = 35,
	.TIME1_VS = 8,
	.TIME1_VE = 1080,
	.TIME1_INV = 0,

	/* time 2 sdoe all high */
	.TIME2_HS = 0,
	.TIME2_HE = 0,
	.TIME2_VS = 0,
	.TIME2_VE = 0,
	.TIME2_INV = 0,

	/* gdck */
	.TIME3_HS = 235,
	.TIME3_HE = 797,
	.TIME3_VS = 1,
	.TIME3_VE = 1081,
	.TIME3_INV = 0,

	/* time 4 gdoe all high */
	/* gdsp */
	.TIME5_HS = 512,
	.TIME5_HE = 592,
	.TIME5_VS = 2,
	.TIME5_VE = 3,
	.TIME5_HSPLCNT = 7,
	.TIME5_VACTSEL = 1,
	.TIME5_INV = 1,
	.TIME5_TCOPR = 4,

	.DPI_HSA = 0x0E,
	.DPI_HFP = 0x33,
	.DPI_HBP = 0x08,
	.DPI_VSA = 0x02,
	.DPI_VFP = 0x04,
	.DPI_VBP = 0x04,
	.DPI_CK_POL = 0x00,
};

const struct platform_info_struct panel_1648_1236_info = {
	.clock_setting = 0x83127627,
	.PANEL_WIDTH = 1648,
	.PANEL_HEIGHT = 1236,
	.PANEL_8_BIT = 0,

	/* sdce */
	.TIME0_HS = 41,
	.TIME0_HE = 51,
	.TIME0_VS = 6,
	.TIME0_VE = 1242,
	.TIME0_INV = 1,

	/* sdle */
	.TIME1_HS = 7,
	.TIME1_HE = 27,
	.TIME1_VS = 6,
	.TIME1_VE = 1242,
	.TIME1_INV = 0,

	/* time 2 sdoe all high */
	.TIME2_HS = 7,
	.TIME2_HE = 7,
	.TIME2_VS = 5,
	.TIME2_VE = 1244,
	.TIME2_INV = 0,
	.TIME2_HSPLCNT = 7,
	.TIME2_VACTSEL = 1,
	.TIME2_TCOPR = 4,

	/* gdck */
	.TIME3_HS = 57,
	.TIME3_HE = 497,
	.TIME3_VS = 1,
	.TIME3_VE = 1246,
	.TIME3_INV = 0,

	/* time 4 gdoe all high */


	/* gdsp */
	.TIME5_HS = 256,
	.TIME5_HE = 280,
	.TIME5_VS = 2,
	.TIME5_VE = 3,
	.TIME5_HSPLCNT = 7,
	.TIME5_VACTSEL = 1,
	.TIME5_INV = 1,
	.TIME5_TCOPR = 4,

	.DPI_HSA = 10,
	.DPI_HFP = 60,
	.DPI_HBP = 7,
	.DPI_VSA = 2,
	.DPI_VFP = 5,
	.DPI_VBP = 4,
	.DPI_CK_POL = 1,
};


/*****************************************
 *  Copyright (C) 2009-2019
 *  ITE Tech. Inc. All Rights Reserved
 *  Proprietary and Confidential
 *****************************************
 *   @file   <IT662X_typedef.h>
 *   @author Hojim.Tseng@ite.com.tw
 *   @date   2019/01/16
 *   @fileversion: ITE_eARC_RX_0.70
 */
#ifndef _IT662X_TYPEDEF_h_
#define _IT662X_TYPEDEF_h_

#ifdef _MCU_8051_
typedef bit iTE_u1;
#define _CODE code
#define _CODE_3K code
#else
#ifdef WIN32
typedef int iTE_u1;
#define _CODE
#define _CODE_3K
#else
typedef unsigned char iTE_u1;
/*	#define _CODE*/
#define _CODE __attribute__((section("._OEM_BU1_RODATA ")))
#define _CODE_3K __attribute__((section("._3K_RODATA ")))
#endif /* _MCU_8051_*/
#endif

/*#include <stdio.h>*/
#include <linux/kernel.h>
#define iTE_FALSE 0
#define iTE_TRUE 1
typedef char iTE_s8, *iTE_ps8;
typedef unsigned char iTE_u8, *iTE_pu8;
typedef short iTE_s16, *iTE_ps16;
typedef unsigned short iTE_u16, *iTE_pu16;
typedef unsigned int iTE_u32, *iTE_pu32;
typedef int iTE_s32, *iTE_ps32;

#define iTE_Msg(x) do {pr_debug("\r[iTE]"); pr_debug x; } while (0)
#define iTE_MsgA(x)   do {pr_debug x; ; } while (0)
#define iTE_Cec_Msg(x)  do {iTE_MsgA(("\r[CEC]")); iTE_MsgA(x); } while (0)
#define iTE_CecDbg_Msg(x) do {iTE_MsgA(("\r[CEC]")); iTE_MsgA(x); } while (0)
#define iTE_MsgE(x) do {iTE_Msg(("\r\n****ERROR****:\r\n")); iTE_Msg(x); } while (0)
#define STA_CHANGE(a, b, c) do {(a) = ((a) & (~(b))) | ((c) & (b)); ; } while (0)

#define AUD32K (0x03)
#define AUD44K (0x00)
#define AUD48K (0x02)
#define AUD64K (0x0B)
#define AUD88K (0x08)
#define AUD96K (0x0A)
#define AUD128K (0x2B)
#define AUD176K (0x0C)
#define AUD192K (0x0E)
#define AUD256K (0x1B)
#define AUD352K (0x0D)
#define AUD384K (0x05)
#define AUD512K (0x3B)
#define AUD705K (0x2D)
#define AUD768K (0x09)
#define AUD1024K (0x35)
#define AUD1411K (0x1D)
#define AUD1536K (0x15)

#define I2S (0)
#define SPDIF (1)

#define LPCM (0)
#define NLPCM (1)
#define HBR (2)
#define DSD (3)
#define CEC_ADR (0xCA)
#endif

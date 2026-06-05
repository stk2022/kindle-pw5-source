/*****************************************
 *  Copyright (C) 2009-2019
 *  ITE Tech. Inc. All Rights Reserved
 *  Proprietary and Confidential
 *****************************************
 *   @file   <IT662X_Cec.h>
 *   @author Hojim.Tseng@ite.com.tw
 *   @date   2019/01/16
 *   @fileversion: ITE_eARC_RX_0.70
 */
#ifndef _IT662X_CEC_H_
#define _IT662X_CEC_H_

#include "IT662X_eARC.h"
#if (IS_IT663XX == IT66321)
#include "../IT6634_Customer/IT6634_Config.h"
#else
#include "IT662X_typedef.h"
#endif

extern void (*Cec_TxDoneCb)(iTE_u8 u8TxSta1, iTE_u8 u8TxSta2, iTE_u8 u8TxHeader,
			    iTE_u8 u8TxOpCode);
extern void (*Cec_RxDoneCb)(iTE_u8 *u8CecCmd, iTE_u8 u8Size);
void Cec_Init(iTE_u8 u8TimerUnit);
void Cec_Irq(void);
void Cec_CmdFire(iTE_u8 *u8CecCmd, iTE_u8 u8Len);
void Cec_LaSet(iTE_u8 u8La);
void Cec_Fire(void);

#define CEC_RX_SELF_DIS (1)
#define CEC_RST (0)
#define CEC_NACK_EN (0)
#define CEC_CAL_CNT (1)
#define CEC_RE_FIRE_MAX (3)

#if (IS_IT663XX != IT66321)
iTE_u8 iTE_I2C_ReadBurst(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8Count,
			 iTE_u8 *pu8Data);
iTE_u8 iTE_I2C_SetByte(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8InvMask,
		       iTE_u8 data);
iTE_u8 iTE_I2C_WriteByte(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 data);
iTE_u8 iTE_I2C_ReadByte(iTE_u8 Addr, iTE_u8 u8Offset);
iTE_u8 iTE_I2C_WriteBurst(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8Count,
			  iTE_u8 *pu8Data);

#define Cec_Wb(u8Offset, u8Count, pu8Data)                                     \
	iTE_I2C_WriteBurst(CEC_ADR, u8Offset, u8Count, pu8Data)
#define Cec_Rb(u8Offset, u8Count, pu8Data)                                     \
	iTE_I2C_ReadBurst(CEC_ADR, u8Offset, u8Count, pu8Data)
#define Cec_Set(u8Offset, u8InvMask, u8Data)                                   \
	iTE_I2C_SetByte(CEC_ADR, u8Offset, u8InvMask, u8Data)
#define Cec_W(u8Offset, u8Data) iTE_I2C_WriteByte(CEC_ADR, u8Offset, u8Data)
#define Cec_R(u8Offset) iTE_I2C_ReadByte(CEC_ADR, u8Offset)
#else
/*Define your I2C driver's API*/
void I2C_API1(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Count, iTE_pu8 pu8Data);
void I2C_API2(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Count, iTE_pu8 pu8Data);
void I2C_API3(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8InvMask, iTE_u8 u8Data);
void I2C_API4(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Data);
iTE_u8 I2C_API5(iTE_u8 addr, iTE_u8 u8Offset);

#define Cec_Wb(u8Offset, u8Count, pu8Data)                                     \
	I2C_API1(CEC_ADR, u8Offset, u8Count, pu8Data)
#define Cec_Rb(u8Offset, u8Count, pu8Data)                                     \
	I2C_API2(CEC_ADR, u8Offset, u8Count, pu8Data)
#define Cec_Set(u8Offset, u8InvMask, u8Data)                                   \
	I2C_API3(CEC_ADR, u8Offset, u8InvMask, u8Data)
#define Cec_W(u8Offset, u8Data) I2C_API4(CEC_ADR, u8Offset, u8Data)
#define Cec_R(u8Offset) I2C_API5(CEC_ADR, u8Offset)
#endif
#endif

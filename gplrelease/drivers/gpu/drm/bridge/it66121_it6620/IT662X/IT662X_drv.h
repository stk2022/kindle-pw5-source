/*****************************************
 *  Copyright (C) 2009-2019
 *  ITE Tech. Inc. All Rights Reserved
 *  Proprietary and Confidential
 *****************************************
 *   @file   <IT662X_drv.h>
 *   @author Hojim.Tseng@ite.com.tw
 *   @date   2019/01/16
 *   @fileversion: ITE_eARC_RX_0.70
 */
#ifndef _eARC_DRV_H_
#define _eARC_DRV_H_

void IT662x_eARC_ini(void);
void IT662x_eARC_Main(void);

#define iTE_u8 unsigned char
iTE_u8 iTE_I2C_ReadByte(iTE_u8 Addr, iTE_u8 u8Offset);
iTE_u8 iTE_I2C_WriteByte(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 data);
iTE_u8 iTE_I2C_SetByte(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8InvMask,
		       iTE_u8 data);
iTE_u8 iTE_I2C_ReadBurst(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8Count,
			 iTE_u8 *pu8Data);
iTE_u8 iTE_I2C_WriteBurst(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8Count,
			  iTE_u8 *pu8Data);
void test_msg(int cmd);


void IT662x_eARC_RX_Bank(iTE_u8 bankno);
iTE_u8 IT662x_eARC_RX_Ini(void);
void iTE_I2S5_GPIO_Enable(void);
void iTE_I2S4_GPIO_Reset(void);
void CecSys_TxHandler(void);

#endif

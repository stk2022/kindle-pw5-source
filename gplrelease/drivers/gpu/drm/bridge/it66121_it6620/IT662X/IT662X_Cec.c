/*
 * Copyright (C) 2009-2019
 * ITE Tech. Inc. All Rights Reserved
 * Proprietary and Confidential
 * @file   <IT662X_Cec.c>
 * @author Hojim.Tseng@ite.com.tw
 * @date   2019/01/16
 * @fileversion: ITE_eARC_RX_0.70
 */

#include "IT662X_Cec.h"
#if ((En_eARCRX == iTE_TRUE) || (En_eARCTX == iTE_TRUE))

iTE_ps8 _CODE sCecTxStatus[4] = {"ACK", "NACK", "RETRY", "FAIL"};

/*iTE_u8	u8CecRxHeader[3];*/
iTE_u8 u8CecCmd[20];

/*********************************************/
void (*Cec_TxDoneCb)(iTE_u8 u8TxSta1, iTE_u8 u8TxSta2, iTE_u8 u8TxHeader,
		     iTE_u8 u8TxOpCode);
void (*Cec_RxDoneCb)(iTE_u8 *u8CecCmd, iTE_u8 u8Size);

#if 0
/********************************************/
void Cec_TxDoneCb(iTE_u8 u8TxSta1, iTE_u8 u8TxSta2)
{
	iTE_Cec_Msg((""));
}

/********************************************/
void Cec_RxDoneCb(iTE_u8 *u8CecCmd, iTE_u8 u8Size)
{
	iTE_pu8	pu8Ptr = u8CecCmd;

	iTE_Cec_Msg(("\\\\\\\\\\\\n"));
	while (u8Size--)
		iTE_Cec_Msg(("%02X\n", *pu8Ptr++));

	iTE_Cec_Msg(("\\\\\\\\\\\\n"));
}
/********************************************/
void Cec_FireStatus(iTE_u1 bTxDone)
{
	iTE_u8	u8Initiator, u8Follower;
	iTE_u8	u8Sta;

	u8Sta = Cec_R(0x54);

	iTE_Cec_Msg(("OpCode[%02X] %s, %02X\n", Cec_R(0x11),
		sCecTxStatus[(Cec_R(0x44) & 0x0C) >> 2], u8Sta));

	u8Initiator = Cec_R(0x10);
	u8Follower = u8Initiator & 0x0F;
	u8Initiator >>= 4;

	if (u8Initiator == u8Follower) {
		u8Sta &= 0x03;
		if (bTxDone) {
			if (u8Sta == 0x01) {	// ACK
				return iTE_FALSE;
			}
		} else{
			if (u8Sta != 0x02) {		// NACK
				return iTE_FALSE;
			}
		}
	}
	return iTE_TRUE;
}
#endif
/********************************************/
void Cec_RxFifoReset(void)
{
	Cec_W(0x52, 0x60);
	Cec_W(0x52, 0xE0);
	Cec_W(0x52, 0x80);
	/*	u8CecRxHeader[0] = 0;*/
}
/********************************************/
void Cec_RxCmdPush(iTE_u8 *u8Header)
{
	iTE_u8 u8Size;
	iTE_pu8 pu8Ptr;

	u8Size = u8Header[2] & 0x1F;
	iTE_Cec_Msg(("Cec_RxCmdPush %d\n", u8Size));
	if (u8Size < 2) {
		iTE_Cec_Msg(("Rx Cmd Fail Header[%02X] OP[%02X] %X\n",
			     u8Header[0], u8Header[1], u8Header[2]));
	} else {
		pu8Ptr = &u8Header[2];
		u8Size -= 2;

		if (u8Size)
			Cec_Rb(0x50, u8Size, pu8Ptr);

		if (Cec_RxDoneCb) {

			iTE_pu8 pu8Buf = u8Header;
			iTE_u8 u8Cnt = u8Size + 2;

			while (u8Cnt--)
				iTE_MsgA(("%02X ", *pu8Buf++));

			iTE_MsgA(("\n\n"));

			Cec_RxDoneCb(u8Header, u8Size + 2);
		} else {
			iTE_MsgE(("[Cec] CecSys_Init Fail\n"));
		}
	}
}
/********************************************/
void Cec_RxCmdGet(void)
{
	iTE_u8 u8Reg51[3] = {0};

	Cec_Rb(0x51, 0x03, u8Reg51);
	u8Reg51[1] &= 0x0F;
	do {
		if (u8Reg51[0] & 0xCC) {
			iTE_MsgE(("[Cec] CecRx_FifoReset\n"));
			Cec_RxFifoReset();
			return; /*for check server*/
		} else {
			iTE_CecDbg_Msg(("Cec_RxCmdGet %02X, %02X, %02X\n\n",
					u8Reg51[0], u8Reg51[1], u8Reg51[2]));
			while (u8Reg51[1]--) {
				Cec_Rb(0x4D, 0x03, u8CecCmd);
				Cec_RxCmdPush(u8CecCmd);
			}
			Cec_Rb(0x51, 0x03, u8Reg51);
			u8Reg51[1] &= 0x0F;
		}
	} while (u8Reg51[1]);
}
/********************************************/
void Cec_Init(iTE_u8 u8TimerUnit)
{
	iTE_Cec_Msg(("Cec_Init %X\n", u8TimerUnit));

	Cec_W(0x08, 0x4C);
	Cec_W(0x08, 0x48);
	Cec_W(0x22, 0x0F); // default LogAdr
	Cec_W(0x09, 0x40 | (CEC_RX_SELF_DIS << 5));
	Cec_W(0x0B, 0x14);
/*CEC bit-timing decided by mDelay1ms(99) in IT662x_eARC_RX_CalRclk*/
	/*Cec_W(0x0C, u8TimerUnit);*/
	Cec_W(0x0C, 0x40); /*to match CEC bit-timing spec*/
	Cec_Set(0x08, 0x04, CEC_RST << 2);
	Cec_Set(0x09, 0x02, 0x00);
	Cec_W(0x06, 0x00);
	Cec_Set(0x08, 0x01, 0x01);
	Cec_RxFifoReset();
	/*	Cec_W(0x0A, 0x03); */
	Cec_W(0x0A, 0x23);
	Cec_Set(0x0A, 0x40, 0x40);
}
/********************************************/
void Cec_Irq(void)
{
	iTE_u8 u8CecSta = Cec_R(0x4C);

	/*	iTE_Cec_Msg(("Cec_Irq %02X\n", u8CecSta));*/
	Cec_W(0x4C, u8CecSta);
	if (u8CecSta & 0x28) { /* Cec Initiator int*/
		iTE_u8 u8TxSta1 = (Cec_R(0x44) & 0x0C) >> 2;
		iTE_u8 u8TxSta2 = Cec_R(0x54) & 0x03;

		iTE_CecDbg_Msg(("TxOpCode[%02X] %s, %d\n", Cec_R(0x11),
				sCecTxStatus[u8TxSta1], u8TxSta2));
		if (Cec_TxDoneCb) {
			Cec_TxDoneCb(u8TxSta1, u8TxSta2, Cec_R(0x10),
				     Cec_R(0x11));
		} else {
			iTE_MsgE(("[Cec] CecSys_Init Fail\n"));
		}
	}
	if (u8CecSta & 0xD4) { /* Cec receiver int*/
		if (u8CecSta & 0x04) {
			iTE_MsgE(("[Cec] Rx Fail\n"));
			Cec_RxCmdGet();
		}
		if (u8CecSta & 0xC0) {
			iTE_u8 u8Reg51[3];

			iTE_MsgE(("[Cec] Rx FIFO overflow %X\n", u8CecSta));
			Cec_Rb(0x51, 0x03, u8Reg51);
			iTE_Cec_Msg(("%02X, %02X, %02X\n", u8Reg51[0],
				     u8Reg51[1], u8Reg51[2]));
			Cec_RxFifoReset();
		}
		if (u8CecSta & 0x10) {
			iTE_CecDbg_Msg(("Rx Done\n"));
			Cec_RxCmdGet();
		}
	}
}
/********************************************/
void Cec_CmdFire(iTE_u8 *u8CecCmd, iTE_u8 u8Len)
{
	if (u8Len && (u8Len < 19)) {

		iTE_pu8 pu8Ptr = u8CecCmd;

		Cec_Wb(0x10, u8Len, u8CecCmd);
		Cec_W(0x23, u8Len);
		if (u8Len == 1)
			Cec_W(0x11, 0xFF);

		while (u8Len--)
			iTE_MsgA(("%02x ", *pu8Ptr++));

		iTE_Cec_Msg(("\n\n"));

		Cec_Fire();
	}
}
/********************************************/
void Cec_LaSet(iTE_u8 u8La)
{
	Cec_W(0x22, u8La);
}
/********************************************/
void Cec_Fire(void)
{
	iTE_u8 u8Temp;

	u8Temp = Cec_R(0x08) | 0x88;
	Cec_W(0x08, u8Temp & 0x7F);
	Cec_W(0x08, u8Temp);
}
/********************************************/
#if (IS_IT663XX != IT66321)
void I2C_API1(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Count, iTE_pu8 pu8Data)
{
}
void I2C_API2(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Count, iTE_pu8 pu8Data)
{
}
void I2C_API3(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8InvMask, iTE_u8 u8Data)
{
}
void I2C_API4(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Data)
{
}
iTE_u8 I2C_API5(iTE_u8 addr, iTE_u8 u8Offset)
{
	return 0;
}
#endif
#endif

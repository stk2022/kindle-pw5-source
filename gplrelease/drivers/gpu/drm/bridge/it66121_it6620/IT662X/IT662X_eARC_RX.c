/*****************************************
 *  Copyright (C) 2009-2019
 *  ITE Tech. Inc. All Rights Reserved
 *  Proprietary and Confidential
 *****************************************
 *   @file   <IT662X_eARC_RX.c>
 *   @author Hojim.Tseng@ite.com.tw
 *   @date   2019/01/16
 *   @fileversion: ITE_eARC_RX_0.70
 */

#include "IT662X_eARC.h"
#include "../inc/hdmi_drv.h"


#if (En_eARCRX == iTE_TRUE)

eARCRX_u8Data g_u8RX;

iTE_u8 rxcapfile[256];
iTE_u8 const DefaultAdoData[256] = {
	0x01,		  /* Capabilities Data Structure Version = 0x01*/
	0x01, 0x20,       /* BLOCK_ID=1, 38-byte*/
	0x3B,		  /* Tag=1 (Audio Data Block), Length=27*/
	0x0F, 0x7F, 0x07, /* LPCM : 8-ch, 32~192K*/
	0x15, 0x07, 0x50, /* AC-3 : 6-ch, 32~48K*/
	0x35, 0x06, 0x3C, /* AAC  : 6-ch, 44~48K*/
	0x3E, 0x1E, 0xC0, /* DTS  : 7-ch, 44~96K*/
	0x4D, 0x02, 0x00, /* DSD  : 6-ch, 44K*/
	0x57, 0x06, 0x00, /* HBR  : 8-ch, 44~48K (Dolby Digital)*/
	0x5F, 0x7E, 0x01, /* HBR  : 8-ch, 44~192K (DTS-HD)*/
	0x67, 0x7E, 0x00, /* HBR  : 8-ch, 44~192K (Dolby TrueHD) MAT*/
	0xFF, 0x7F, 0x6F, /* LPCM : 16-ch, 32~192K (3D Audio)*/

	0x83, /* Tag=4 (Speaker Allocation Data Block), 3-bye*/
	0x6F, 0x0F, 0x0C,

/*0xE6, 0x13,*/ /* Tag Code=7, Length=11, Extended Tag Code=0x13 */
/*(Room Configuration Data Block)*/
/*0x00, 0x6F, 0x0F, 0x0C,*/
#ifdef _QD980ATC_   /* temp for QD980 HFR5-2-36*/
	0x02, 0x0C, /* BLOCK_ID=2, 12-byte*/
	0xEB, 0x14, /* Tag Code=7, Length=11, Extended Tag Code=0x14*/
#else
	0x02, 0x0A,         /* BLOCK_ID=2, 10-byte*/
	/* should be marked for QD980 HFR5-2-26*/
#endif
	0x20, 0x00, 0x00, 0x00, 0x00, 0x21, 0x01, 0x01, 0x01, 0x01, 0x03,
	0x01, /* BLOCK_ID=3, 1-byte*/
	0x89, /* Supports_AI=1, ONE_BIT_AUDIO_LAYOUT=1 (12-ch),*/
		 /*MULTI_CH_LPCM_LAYOUT=1 (16-ch)*/
#ifndef _QD980ATC_
	0x00, 0x00, /* should be marked for QD980 HFR5-2-26*/
#endif
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, /*70*/
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
iTE_u8 TxAudEnc = iTE_FALSE; /* LPCM 2-ch do not support encryption*/
iTE_u8 RxErrDet = iTE_FALSE; /* must be FALSE when TxAudEnc=TRUE*/
iTE_u32 RXRCLK, RXACLK, RXBCLK;

/*I2C function for it6620 start*/
iTE_u8 iTE_I2C_ReadByte(iTE_u8 Addr, iTE_u8 u8Offset)
{
	iTE_u8 p_data;

	it6620_i2c_read_byte(Addr, u8Offset, &p_data);
	return p_data;
}

iTE_u8 iTE_I2C_WriteByte(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 data)
{
	iTE_u8 flag;

	flag = it6620_i2c_write_byte(Addr, u8Offset, data);

	return flag;
}

iTE_u8 iTE_I2C_SetByte(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8InvMask,
		       iTE_u8 data)
{
	iTE_u8 Temp;

	if (u8InvMask != 0xFF) {
		Temp = iTE_I2C_ReadByte(Addr, u8Offset);
		Temp &= (~u8InvMask);
		Temp |= data & u8InvMask;
	} else {
		Temp = data;
	}
	return iTE_I2C_WriteByte(Addr, u8Offset, Temp);
}

iTE_u8 iTE_I2C_ReadBurst(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8Count,
			 iTE_u8 *pu8Data)
{
	int i;

	for (i = 0; i < u8Count; i++)
		pu8Data[i] = iTE_I2C_ReadByte(Addr, u8Offset + i);


	return 0;
}

iTE_u8 iTE_I2C_WriteBurst(iTE_u8 Addr, iTE_u8 u8Offset, iTE_u8 u8Count,
			  iTE_u8 *pu8Data)
{
	int i;

	for (i = 0; i < u8Count; i++)
		iTE_I2C_WriteByte(Addr, (u8Offset+i), pu8Data[i]);

	return 0;
}

/*I2C function for it6620 end*/

/*I2S4 --GPIO--OUT--0 - 1  for it66121 reset*/
void iTE_I2S4_GPIO_Reset(void)
{
	eARC_Rxset(0xea, 0x03, 0x03);
	msleep(20);
	eARC_Rxset(0xea, 0x07, 0x07);
	msleep(20);
}

/*I2S5 --GPIO--OUT--1  for HDMI 5V Power*/
void iTE_I2S5_GPIO_Enable(void)
{
	eARC_Rxset(0xea, 0x77, 0x77);
}

#ifdef IT662X_CEC_CodeBase
/********************************************/
/*Function: IT662X_eARC_RX_CEC_Init*/
/*Note: CEC initialization for eARC RX*/
/*others:*/
/********************************************/
void IT662x_eARC_RX_CEC_Init(void)
{
	iTE_u8 u8CecTimerUnit;

	u8CecTimerUnit = RXRCLK / 160;
/*iTE_Cec_Msg(("eARCRX CEC init , u8CecTimerUnit= %02X*/
/* \r\n",u8CecTimerUnit));*/
	eARC_Rxwr(0x05, 0x00);
	eARC_Rxwr(0xFC, CEC_ADR | 0x01);
	Cec_Init(u8CecTimerUnit);
}
/********************************************/
/*Function: eARC_RX_CEC*/
/*Note: CEC Irq service and command handler for eARC RX*/
/*others:*/
/********************************************/
void eARC_RX_CEC(void)
{
#if 1
	if (notify_hpd_flag == 1) {
		notify_hpd_flag = 0;
		CecSys_Init(Cec_Pab, Cec_Pcd, 0);
		iTE_Cec_Msg(("eARC_RX_CEC Pab:0x%x,Pcd:0x%x\n",
			Cec_Pab, Cec_Pcd));
	}

	if (Cec_R(0x48) & 0x20)
		Cec_Irq();

	CecSys_TxHandler();
	CecSys_RxHandler();
#endif
}
#endif
/********************************************/
/*Function: IT662x_CEC_ARC_RX_ONOFF*/
/*Note: ARC function enable for eARC RX*/
/*others:*/
/********************************************/
void IT662x_CEC_ARC_RX_ONOFF(iTE_u8 u8sts)
{
#if 1
	iTE_u8 Reg3A;

	if (u8sts == iTE_TRUE) {
		eARC_Rxset(0x30, 0x08, 0x08);
		eARC_Rxset(0x38, 0x08, 0x08);
		Reg3A = eARC_Rxrd(0x3A);
		if (Reg3A == 0x20)
			iTE_eARCRx_Msg(("ARC mode open \r\n"));
		else
			iTE_eARCRx_Msg((" ARC mode open fail \r\n"));

	} else {
		eARC_Rxset(0x30, 0x08, 0x00);
		eARC_Rxset(0x38, 0x08, 0x00);
		iTE_eARCRx_Msg(("ARC mode close \r\n"));
	}
#endif
}
/********************************************/
/*Function: IT662x_CEC_ARC_State*/
/*Note: Report ARC function enable status for eARC TX*/
/*others:*/
/********************************************/
iTE_u8 IT662x_CEC_ARC_State(void)
{
	iTE_u8 Reg3A;

	Reg3A = eARC_Rxrd(0x3A);
	if (Reg3A == 0x20)
		return iTE_TRUE;
	else
		return iTE_FALSE;

}
/********************************************/
/*Function: IT662x_eARC_RX_SetLat*/
/*Note: Audio Latency function for eARC RX*/
/*others:  lat => Xms delay*/
/********************************************/
void IT662x_eARC_RX_SetLat(void)
{
	eARC_Rxwr(0x54, eARC_Rxrd(0x55)); /* set ERX_LATENCY=earcrxrd(0x55)*/
	eARC_Rxset(0x52, 0x10, 0x10);     /* STAT_CHNG=1*/
}
/********************************************/
/*Function: eARC_Main*/
/*Note:*/
/*others:*/
/********************************************/
void IT662x_eARC_Main(void)
{
#if (En_eARCRX == iTE_TRUE)
	IT662x_eARC_RX_Irq();
#ifdef IT662X_CEC_CodeBase
	eARC_RX_CEC();
#endif
#endif
}
/********************************************/
/*Function: eARC_ini*/
/*Note: Initial IT6620 and IT6621*/
/*others:*/
/********************************************/
void IT662x_eARC_ini(void)
{
#if (En_eARCRX == iTE_TRUE)
	if (IT662x_eARC_RX_Ini() == iTE_FALSE) {
		iTE_eARCRx_Msg(("RX_ini FAIL \r\n"));
		return;
	}
	eARC_Rxset(0x38, 0x01, 0x01); /* RegRxEnDDFSM=1*/

#endif
}
iTE_u8 IT662x_eARC_RX_Ini(void)
{
	iTE_u8 VenID[2], DevID[2];

	VenID[0] = 0;
	VenID[1] = 0;
	DevID[0] = 0;
	DevID[1] = 0;

	DevID[0] = eARC_Rxrd(0x02);
	DevID[1] = eARC_Rxrd(0x03);

	VenID[0] = eARC_Rxrd(0x00);
	VenID[1] = eARC_Rxrd(0x01);

	iTE_eARCRx_Msg(("\n w-y: iTE_eARCRx_Msg\r\n"));

	if (VenID[0] != 0x54 || VenID[1] != 0x49 || DevID[0] != 0x20 ||
	    DevID[1] != 0x66) {
		iTE_eARCRx_Msg((
		"\nError: Can not find IT6620 A1 eARC RX Device !!!\r\n"));
		return iTE_FALSE;
	}

	IT662x_eARC_RX_Rst();
	IT662x_eARC_RX_CEC_Init();
	eARC_RX_VarInit();
	return iTE_TRUE;
}
#if (En_eARCRX == iTE_TRUE)

void IT662x_eARC_RX_Rst(void)
{
	/* Enable GRCLK*/
	eARC_Rxset(0x28, 0x10, 0x00);
	/* RXAFE PLL Reset*/
	eARC_Rxset(0xb0, 0x20, 0x20); /* RegRx_RST*/
	/*idle(100);*/
	/* earcrx Reset*/
	eARC_Rxset(0x05, 0x80, 0x80); /* RCLK Reset*/

	eARC_Rxset(0x28, 0x03, eARC_RCLKFreqSel);

#ifdef _DACOUT_
/* Setting for audio DAC: RegRxI2SFmt[1]=1 => right-justified,*/
	 /* RegRxI2SFmt[2]=1 => No delay 1T data*/
	eARC_Rxset(0x92, 0x06, 0x06);
#endif

	if (TxAudEnc) {
		eARC_Rxset(0x50, 0x04, 0x04);
		RxErrDet = iTE_FALSE;
	} else
		eARC_Rxset(0x50, 0x04, 0x00);

	RXRCLK = 0;
	RXACLK = 0;
	RXBCLK = 0;
#if 1
	eARC_Rxset(0x70, 0x04, RxECCOpt << 2);
	eARC_Rxset(0x74, 0x62,
		   (RxUBitOpt << 6) + (RxCChOpt << 5) + (RxErrDet << 1));
	eARC_Rxset(0x91, 0x70,
		   (RxEnI2SC << 6) + (RxEnI2SHBR << 5) + (RxEnTDM << 4));
	eARC_Rxset(0x9a, 0xC0, (RxEnMuteB << 7) + (RxEnMuteF << 6));
	eARC_Rxset(0x9b, 0x77, (RxMuteStep << 4) + (RxMuteDSD << 2) +
				       (RxMuteNLPCM << 1) + RxMuteLPCM);
	eARC_Rxset(0x9e, 0x30, (RxEnMCLKInv << 5) + (RxEnSCKInv << 4));
	eARC_Rxset(0x50, 0xC0, (TurnOverSel << 6));
	eARC_Rxset(0x39, 0x04, RxEnMaxHBChk << 2);
	eARC_Rxset(0x2a, 0x03, (RxBCLKInv << 1) + RxACLKInv);
	eARC_Rxset(0x38, 0x0E,
		   (EnterARCNow << 3) + (RxEnARC << 2) + (RxEneARC << 1));
	eARC_Rxset(0x3B, 0x01, ForceARCMode);

	eARC_Rxset(0xc7, 0x10, EnUpdAvg << 4); /* default=0*/

	eARC_Rxwr(0x1f, 0x7f);
	eARC_Rxwr(0x20, 0x7e);
	eARC_Rxwr(0x21, 0x00);
	eARC_Rxwr(0x22, 0x00);
	eARC_Rxwr(0x23, 0x00);
	eARC_Rxwr(0x24, 0x00);
	eARC_Rxwr(0x25, 0x00);

	RXRCLK = 0;
	RXACLK = 0;
	RXBCLK = 0;

	IT662x_eARC_RX_CalRclk();
	IT662x_eARC_RX_SetRxCapDataStruct(DefaultAdoData);

	eARC_Rxset(0x50, 0x08, 0x08); /* RegRxAutoCapChg=1*/
	/*  earcrxset(0x50, 0xC0, 0x40);   // RegRxTurnOverSel*/
	eARC_Rxset(0x5F, 0x80, 0x80); /* RRxDbgFifoClr*/

	eARC_Rxset(0xb1, 0x17, (RX_ENEQ << 4) + RX_RLV);
	eARC_Rxset(0xb3, 0x48, (RX_CDRSEL << 6) + (RX_ENI2 << 3));
	eARC_Rxset(0xb4, 0x77, (RX_DRV_CSW << 4) + RX_DRV_CSR);
	eARC_Rxset(0xb5, 0x7E,
		   (RX_ENHYS << 4) + (RX_VCMSEL << 2) + (RX_RC_CKSEN << 1));
	eARC_Rxset(0x9e, 0x07,
		   0x07); /* RegRxEnI2S=1, RegRxEnSPDIF=1, RegRxEnMUTE=1*/

	eARC_Rxset(0x05, 0x0F, 0x00);

#ifdef _CMDCONLY_
	eARC_Rxset(0xc7, 0x03, 0x00);
	eARC_Rxset(0x05, 0x0F, 0x03);
#endif

#ifdef _DMACONLY_
	eARC_Rxwr(0xbf, 0x00);
	eARC_Rxset(0x05, 0x0F, 0x0C);
#endif
#endif
}
#endif
void eARC_RX_VarInit(void)
{
	g_u8RX.eARC_RX_event = 0;
}

#if (En_eARCRX == iTE_TRUE)

void IT662x_eARC_RX_Irq(void)
{
	iTE_u8 RX_IRQ[7], tmp, x;
	iTE_u8 Reg29, RegB2;

	x = 0;
	for (tmp = 0; tmp < 7; tmp++) {
		RX_IRQ[tmp] = eARC_Rxrd(0x13 + tmp);
		if (eARC_Rxrd(0x13 + tmp))
			x = 1;

	}
	show_rxdbg_fifo();

	if (x) {
		iTE_eARCRx_Msg((
			"******** eARC RX Interrupt Begin ********\r\n"));
	}
	/* Interrupt RxReg13*/
	if (RX_IRQ[0] & 0x01) {
		iTE_eARCRx_Msg(("eARC RX HPD on interrupt ...\r\n"));
		/*enrxdbgfifo();*/
	}
	if (RX_IRQ[0] & 0x02) {
		IT662x_CEC_ARC_RX_ONOFF(iTE_FALSE);
		eARC_Event_Change(g_u8RX.eARC_RX_event, eARC_ARC_Start,
				  eARC_Event_Null);
		iTE_eARCRx_Msg(("eARC RX HPD off interrupt ...\r\n"));
	}

	if (RX_IRQ[0] & 0x04) {
		iTE_eARCRx_Msg(("eARC RX EARC enable interrupt ...\r\n"));
		eARC_Rxwr(0x21, 0xff);
		eARC_Rxwr(0x22, 0xff);
		eARC_Rxwr(0x23, 0xff);
		eARC_Rxwr(0x24, 0x03);
		eARC_Rxwr(0x25, 0x3f);

		eARC_Rxwr(0x54, 0x00); /* set ERX_LATENCY=0ms*/
		eARC_Rxset(0x52, 0x10, 0x10);
/* STAT_CHNG=1   (add this command to pass SL-870 HFR5_2_37)*/

		disrxdbgfifo();
	}

	if (RX_IRQ[0] & 0x08) {
		iTE_eARCRx_Msg(("eARC RX HeartBeat lost interrupt !!!\r\n"));
		disrxdbgfifo();
	}

	if (RX_IRQ[0] & 0x10) {
		iTE_eARCRx_Msg(("eARC RX Discovery TimeOut interrupt !!!\r\n"));
		eARC_Event_Change(g_u8RX.eARC_RX_event, eARC_ARC_Start,
				  eARC_ARC_Start);
	}

	if (RX_IRQ[0] & 0x20) {
		iTE_eARCRx_Msg((
			"eARC RX No Audio change interrupt, RxNoAudFlag=%d \r\n",
			(eARC_Rxrd(0x07) & 0x20) >> 5));
	}

	/* Interrupt RxReg14*/
	if (RX_IRQ[1] & 0x01)
		iTE_eARCRx_Msg(("eARC RX command done interrupt ...\r\n"));


	if (RX_IRQ[1] & 0x02)
		iTE_eARCRx_Msg(("eARC RX command fail interrupt !!!\r\n"));


	if (RX_IRQ[1] & 0x04)
		iTE_eARCRx_Msg(("eARC RX command tiemout interrupt !!!\r\n"));


	if (RX_IRQ[1] & 0x08)
		iTE_eARCRx_Msg(("eARC RX command re-start interrupt !!!\r\n"));


	if (RX_IRQ[1] & 0x10) {
		iTE_eARCRx_Msg((
			"eARC RX ERX_LATENCY_REQ interrupt , ERX_LATENCY_REQ=0x%02X ms \r\n",
			eARC_Rxrd(0x55)));
		IT662x_eARC_RX_SetLat();
	}

	if (RX_IRQ[1] & 0x20) {
		iTE_eARCRx_Msg(
			("eARC RX CMDC bi-phase error interrupt !!!\r\n"));
	}

	/* Interrupt RxReg15*/
	if (RX_IRQ[2] & 0x01)
		iTE_eARCRx_Msg(("eARC RX DMAC Frame error interrupt !!!\r\n"));


	if (RX_IRQ[2] & 0x02) {
		iTE_eARCRx_Msg(
			("eARC RX DMAC Frame_B error interrupt !!!\r\n"));
	}

	if (RX_IRQ[2] & 0x04) {
		iTE_eARCRx_Msg(
			("eARC RX DMAC Frame_M error interrupt !!!\r\n"));
	}

	if (RX_IRQ[2] & 0x08) {
		iTE_eARCRx_Msg(
			("eARC RX DMAC Frame_W error interrupt !!!\r\n"));
	}

	if (RX_IRQ[2] & 0x10) {
		iTE_eARCRx_Msg(
			("eARC RX DMAC preamble error interrupt !!!\r\n"));
	}

	if (RX_IRQ[2] & 0x20) {
		iTE_eARCRx_Msg(
			("eARC RX DMAC bi-phase error interrupt !!!\r\n"));
	}

	if (RX_IRQ[2] & 0x40)
		iTE_eARCRx_Msg(("eARC RX DMAC parity error interrupt !!!\r\n"));


	if (RX_IRQ[2] & 0x80)
		iTE_eARCRx_Msg(("eARC RX DMAC ECC error interrupt !!!\r\n"));


	/* Interrupt RxReg16*/
	if (RX_IRQ[3] & 0x01)
		iTE_eARCRx_Msg(("eARC RX DMAC ECC fix interrupt !!!\r\n"));


	if (RX_IRQ[3] & 0x02)
		iTE_eARCRx_Msg(("eARC RX DMAC V-Bit change interrupt ...\r\n"));


	if (RX_IRQ[3] & 0x04) {
		iTE_eARCRx_Msg(("eARC RX DMAC ChStRdy interrupt ...\r\n"));
		IT662x_eARC_RX_ShowAdoInfo();
		IT662x_eARC_RX_ShowAdoChSts();

		if (EnAIFTest)
			IT662x_eARC_TX_SetAdoInfoFrame();

		if (EnPkt1Test)
			setup_txpkt1();

		if (EnPkt2Test)
			setup_txpkt2();

		if (EnPkt3Test)
			setup_txpkt3();

		/*iTE_u8 Reg29, RegB2;*/
		Reg29 = eARC_Rxrd(0x29) & 0x70;
		RegB2 = eARC_Rxrd(0xb2) & 0x70;
		iTE_eARCRx_Msg(("AOCLKSel=%d, RX_FSEL=%d \r\n", (Reg29 >> 4),
				(RegB2 >> 4)));
		eARC_Rxset(
			0x9E, 0x07,
			0x07); /* RegRxEnI2S=1, RegRxEnSPDIF=1, RegRxEnMUTE=1*/
	}

	if (RX_IRQ[3] & 0x08) {
		iTE_eARCRx_Msg(("eARC RX DMAC ChStChg interrupt ...\r\n"));
		IT662x_eARC_RX_ShowAdoChSts();
	}

	if (RX_IRQ[3] & 0x10) {
		iTE_eARCRx_Msg(("eARC RX DMAC AIF change interrupt ...\r\n"));
		IT662x_eARC_RX_ShowAdoChSts();

		if (EnAIFTest) {
			IT662x_eARC_RX_CheAdoInfoFrame();
			IT662x_eARC_TX_SetAdoInfoFrame();
		}
	}

	if (RX_IRQ[3] & 0x20) {
		iTE_eARCRx_Msg(
			("eARC RX DMAC MUTE change interrupt, RXMUTE=%d \r\n",
			 (eARC_Rxrd(0x07) & 0x80) >> 7));
	}

	if (RX_IRQ[3] & 0x40) {
		iTE_eARCRx_Msg(
			("eARC RX DMAC audio FIFO error interrupt !!!\r\n"));
	}

	if (RX_IRQ[3] & 0x80) {
		iTE_eARCRx_Msg((
	"eARC RX DMAC output audio encoding error interrupt\r\n"));
	}

	/* Interrupt RxReg17*/
	if (RX_IRQ[4] & 0x01) {
		iTE_eARCRx_Msg((
			"eARC RX DMAC U-Bit packet 1 ready interrupt\r\n"));
		if (EnPkt1Test) {
			check_rxpkt1();
			setup_txpkt1();
		}
	}

	if (RX_IRQ[4] & 0x02) {
		iTE_eARCRx_Msg((
			"eARC RX DMAC U-Bit packet 1 change interrupt\r\n"));
		if (EnPkt1Test) {
			check_rxpkt1();
			setup_txpkt1();
		}
	}

	if (RX_IRQ[4] & 0x04) {
		iTE_eARCRx_Msg((
			"eARC RX DMAC U-Bit packet 2 ready interrupt\r\n"));
		if (EnPkt2Test) {
			check_rxpkt2();
			setup_txpkt2();
		}
	}

	if (RX_IRQ[4] & 0x08) {
		iTE_eARCRx_Msg((
			"eARC RX DMAC U-Bit packet 2 change interrupt\r\n"));
		if (EnPkt2Test) {
			check_rxpkt2();
			setup_txpkt2();
		}
	}

	if (RX_IRQ[4] & 0x10) {
		iTE_eARCRx_Msg((
			"eARC RX DMAC U-Bit packet 3 ready interrupt\r\n"));
		if (EnPkt3Test) {
			check_rxpkt3();
			setup_txpkt3();
		}
	}

	if (RX_IRQ[4] & 0x20) {
		iTE_eARCRx_Msg((
			"eARC RX DMAC U-Bit packet 3 change interrupt\r\n"));
		if (EnPkt3Test) {
			check_rxpkt3();
			setup_txpkt3();
		}
	}

	if (RX_IRQ[4] & 0x40) {
		iTE_eARCRx_Msg((
		"eARC RX DMAC U-Bit message start detect interrupt\r\n"));
	}

	if (RX_IRQ[4] & 0x80) {
		iTE_eARCRx_Msg((
		"eARC RX DMAC U-Bit message start error interrupt !!!\r\n"));
	}

	/* Interrupt RxReg18*/
	if (RX_IRQ[5] & 0x01) {
		iTE_eARCRx_Msg((
		"eARC RX DMAC U-Bit message IU number error interrupt\r\n"));
	}

	if (RX_IRQ[5] & 0x02) {
		iTE_eARCRx_Msg((
		"eARC RX DMAC U-Bit message IU counter error interrupt\r\n"));
	}

	/* Interrupt RxReg19*/
	if (RX_IRQ[6] & 0x01) {
		iTE_eARCRx_Msg(
			("eARC RX detect ACLK stable interrupt ...\r\n"));
		IT662x_eARC_RX_ShowAclk();
	}

	if (RX_IRQ[6] & 0x02)
		iTE_eARCRx_Msg(("eARC RX detect ACLK valid interrupt ...\r\n"));


	if (RX_IRQ[6] & 0x08)
		iTE_eARCRx_Msg(("eARC RX detect BCLK valid interrupt ...\r\n"));


	if (RX_IRQ[6] & 0x10)
		iTE_eARCRx_Msg(("eARC RX detect no BCLK interrupt ...\r\n"));


	if (RX_IRQ[6] & 0x04) {
		iTE_eARCRx_Msg(
			("eARC RX detect BCLK stable interrupt ...\r\n"));
		IT662x_eARC_RX_ShowBclk();
		IT662x_eARC_RX_ForcePDIV();
	}
	if (RX_IRQ[6] & 0x20)
		iTE_eARCRx_Msg(("eARC RX detect no DMAC interrupt ...\r\n"));

	for (tmp = 0; tmp < 7; tmp++)
		eARC_Rxwr(0x13 + tmp, RX_IRQ[tmp]);


	if (x) {
		iTE_eARCRx_Msg((
			"****** eARC RX Interrupt End **********\r\n"));
	}
}
#endif

void IT662x_eARC_RX_ShowAdoInfo(void)
{
	iTE_u8 rxaudfmt, rxlayout, rxchstfs, rxaudsrc, rxaudlen;
	iTE_u8 rxaudfs = AUD32K; /*for build error*/

	rxaudfmt = eARC_Rxrd(0x75) & 0x1F;
	iTE_eARCRx_Msg(("eARC RX AudFmt = "));
	if (rxaudfmt == UN2CHLPCM)
		iTE_eARCRx_Msg(("UN2CHLPCM\r\n"));
	else if (rxaudfmt == UNMCHLPCM)
		iTE_eARCRx_Msg(("UNMCHLPCM\r\n"));
	else if (rxaudfmt == UNXCHDSD)
		iTE_eARCRx_Msg(("UNXCHDSD\r\n"));
	else if (rxaudfmt == UN2CHNLPCM)
		iTE_eARCRx_Msg(("UN2CHNLPCM\r\n"));
	else if (rxaudfmt == EN2CHNLPCM)
		iTE_eARCRx_Msg(("EN2CHNLPCM\r\n"));
	else if (rxaudfmt == ENMCHNLPCM)
		iTE_eARCRx_Msg(("ENMCHNLPCM\r\n"));
	else if (rxaudfmt == ENXCHDSD)
		iTE_eARCRx_Msg(("ENXCHDSD\r\n"));
	else
		iTE_eARCRx_Msg(("ERROR !!! Reg75[4:0]=0x%02X\r\n", rxaudfmt));


	rxlayout = eARC_Rxrd(0x76) & 0x0F;
	iTE_eARCRx_Msg(("eARC RX Layout = "));
	if ((rxaudfmt == UN2CHLPCM || rxaudfmt == UNMCHLPCM) &&
	    (rxlayout == LPCM2CH)) {
		iTE_eARCRx_Msg(("LPCM2CH\r\n"));
	} else if ((rxaudfmt == UNMCHLPCM || rxaudfmt == ENMCHNLPCM) &&
		   (rxlayout == LPCM8CH)) {
		iTE_eARCRx_Msg(("LPCM8CH\r\n"));
	} else if ((rxaudfmt == UNMCHLPCM || rxaudfmt == ENMCHNLPCM) &&
		   (rxlayout == LPCM16CH)) {
		iTE_eARCRx_Msg(("LPCM16CH\r\n"));
	} else if ((rxaudfmt == UNMCHLPCM || rxaudfmt == ENMCHNLPCM) &&
		   (rxlayout == LPCM32CH)) {
		iTE_eARCRx_Msg(("LPCM32CH\r\n"));
	} else if ((rxaudfmt == UN2CHNLPCM || rxaudfmt == EN2CHNLPCM) &&
		   (rxlayout == NLPCM2CH)) {
		iTE_eARCRx_Msg(("NLPCM2CH\r\n"));
	} else if ((rxaudfmt == UN2CHNLPCM || rxaudfmt == EN2CHNLPCM) &&
		   (rxlayout == NLPCM8CH)) {
		iTE_eARCRx_Msg(("NLPCM8CH\r\n"));
	} else if ((rxaudfmt == UNXCHDSD || rxaudfmt == ENXCHDSD) &&
		   (rxlayout == DSD6CH)) {
		iTE_eARCRx_Msg(("DSD6CH\r\n"));
	} else if ((rxaudfmt == UNXCHDSD || rxaudfmt == ENXCHDSD) &&
		   (rxlayout == DSD12CH)) {
		iTE_eARCRx_Msg(("DSD12CH\r\n"));
	} else {
		iTE_eARCRx_Msg(("ERROR !!! Reg76[3:0]=0x%01X\r\n", rxlayout));
	}

	rxchstfs = eARC_Rxrd(0x77) & 0x3F;
	iTE_eARCRx_Msg(("eARC RX ChStFS = "));
	switch (rxchstfs) {
	case AUD32K:
		iTE_eARCRx_Msg(("32K\r\n"));
		break;
	case AUD44K:
		iTE_eARCRx_Msg(("44.1K\r\n"));
		break;
	case AUD48K:
		iTE_eARCRx_Msg(("48K\r\n"));
		break;
	case AUD64K:
		iTE_eARCRx_Msg(("64K\r\n"));
		break;
	case AUD88K:
		iTE_eARCRx_Msg(("88.2K\r\n"));
		break;
	case AUD96K:
		iTE_eARCRx_Msg(("96K\r\n"));
		break;
	case AUD128K:
		iTE_eARCRx_Msg(("128K\r\n"));
		break;
	case AUD176K:
		iTE_eARCRx_Msg(("176.4K\r\n"));
		break;
	case AUD192K:
		iTE_eARCRx_Msg(("192K\r\n"));
		break;
	case AUD256K:
		iTE_eARCRx_Msg(("256K\r\n"));
		break;
	case AUD352K:
		iTE_eARCRx_Msg(("352.8K\r\n"));
		break;
	case AUD384K:
		iTE_eARCRx_Msg(("384K\r\n"));
		break;
	case AUD512K:
		iTE_eARCRx_Msg(("512K\r\n"));
		break;
	case AUD705K:
		iTE_eARCRx_Msg(("705.6K\r\n"));
		break;
	case AUD768K:
		iTE_eARCRx_Msg(("768K\r\n"));
		break;
	case AUD1024K:
		iTE_eARCRx_Msg(("1024K\r\n"));
		break;
	case AUD1411K:
		iTE_eARCRx_Msg(("1411.2K\r\n"));
		break;
	case AUD1536K:
		iTE_eARCRx_Msg(("1536K\r\n"));
		break;
	default:
		iTE_eARCRx_Msg(("Error !!!\r\n"));
		break;
	}

	rxaudsrc = eARC_Rxrd(0x91) & 0x03;
	if (rxaudsrc == I2S)
		iTE_eARCRx_Msg(("\nRxAudSrc=I2S, "));
	else if (rxaudsrc == SPDIF)
		iTE_eARCRx_Msg(("\nRxAudSrc=SPDIF, "));
	else if (rxaudsrc == TDM)
		iTE_eARCRx_Msg(("\nRxAudSrc=TDM, "));
	else
		iTE_eARCRx_Msg(("\nRxAudSrc=DSD, "));


	iTE_eARCRx_Msg(("RxAudEn=0x%02X\r\n", eARC_Rxrd(0x90)));

	if (rxlayout == LPCM2CH || rxlayout == NLPCM2CH)
		rxaudfs = rxchstfs;
	else if (rxlayout == LPCM8CH || rxlayout == NLPCM8CH) {
		if (rxchstfs == AUD128K)
			rxaudfs = AUD32K;
		else if (rxchstfs == AUD176K)
			rxaudfs = AUD44K;
		else if (rxchstfs == AUD192K)
			rxaudfs = AUD48K;
		else if (rxchstfs == AUD256K)
			rxaudfs = AUD64K;
		else if (rxchstfs == AUD352K)
			rxaudfs = AUD88K;
		else if (rxchstfs == AUD384K)
			rxaudfs = AUD96K;
		else if (rxchstfs == AUD512K)
			rxaudfs = AUD128K;
		else if (rxchstfs == AUD705K)
			rxaudfs = AUD176K;
		else if (rxchstfs == AUD768K)
			rxaudfs = AUD192K;
	} else if (rxlayout == LPCM16CH || rxlayout == DSD6CH) {
		if (rxchstfs == AUD256K)
			rxaudfs = AUD32K;
		else if (rxchstfs == AUD352K)
			rxaudfs = AUD44K;
		else if (rxchstfs == AUD384K)
			rxaudfs = AUD48K;
		else if (rxchstfs == AUD512K)
			rxaudfs = AUD64K;
		else if (rxchstfs == AUD705K)
			rxaudfs = AUD88K;
		else if (rxchstfs == AUD768K)
			rxaudfs = AUD96K;
	} else if (rxlayout == DSD12CH) {
		if (rxchstfs == AUD512K)
			rxaudfs = AUD32K;
		else if (rxchstfs == AUD705K)
			rxaudfs = AUD44K;
		else if (rxchstfs == AUD768K)
			rxaudfs = AUD48K;
	}
	switch (rxaudfs) {
	case AUD32K:
		iTE_eARCRx_Msg(("RxAudFS=32K\r\n"));
		break;
	case AUD44K:
		iTE_eARCRx_Msg(("RxAudFS=44.1K\r\n"));
		break;
	case AUD48K:
		iTE_eARCRx_Msg(("RxAudFS=48K\r\n"));
		break;
	case AUD64K:
		iTE_eARCRx_Msg(("RxAudFS=64K\r\n"));
		break;
	case AUD88K:
		iTE_eARCRx_Msg(("RxAudFS=88.2K\r\n"));
		break;
	case AUD96K:
		iTE_eARCRx_Msg(("RxAudFS=96K\r\n"));
		break;
	case AUD128K:
		iTE_eARCRx_Msg(("RxAudFS=128K\r\n"));
		break;
	case AUD176K:
		iTE_eARCRx_Msg(("RxAudFS=176.4K\r\n"));
		break;
	case AUD192K:
		iTE_eARCRx_Msg(("RxAudFS=192K\r\n"));
		break;
	case AUD256K:
		iTE_eARCRx_Msg(("RxAudFS=256K\r\n"));
		break;
	case AUD352K:
		iTE_eARCRx_Msg(("RxAudFS=352.8K\r\n"));
		break;
	case AUD384K:
		iTE_eARCRx_Msg(("RxAudFS=384K\r\n"));
		break;
	case AUD512K:
		iTE_eARCRx_Msg(("RxAudFS=512K\r\n"));
		break;
	case AUD705K:
		iTE_eARCRx_Msg(("RxAudFS=705.6K\r\n"));
		break;
	case AUD768K:
		iTE_eARCRx_Msg(("RxAudFS=768K\r\n"));
		break;
	case AUD1024K:
		iTE_eARCRx_Msg(("RxAudFS=1024K\r\n"));
		break;
	case AUD1411K:
		iTE_eARCRx_Msg(("RxAudFS=1411.2K\r\n"));
		break;
	case AUD1536K:
		iTE_eARCRx_Msg(("RxAudFS=1536K\r\n"));
		break;
	default:
		iTE_eARCRx_Msg(("RxAudFS Error!!!\r\n"));
	}

	IT662x_eARC_RX_Bank(1);
	rxaudlen = eARC_Rxrd(0x44) & 0x0F;
	iTE_eARCRx_Msg(("RX144 = %2x\r\n", eARC_Rxrd(0x44)));
	switch (rxaudlen) {
	case 0x0D:
		iTE_eARCRx_Msg(("RxAudLen=21-bit\r\n"));
		break;
	case 0x0B:
		iTE_eARCRx_Msg(("RxAudLen=24-bit\r\n"));
		break;
	case 0x09:
		iTE_eARCRx_Msg(("RxAudLen=23-bit\r\n"));
		break;
	case 0x05:
		iTE_eARCRx_Msg(("RxAudLen=22-bit\r\n"));
		break;
	case 0x03:
		iTE_eARCRx_Msg(("RxAudLen=20-bit\r\n"));
		break;
	case 0x01:
		iTE_eARCRx_Msg(("RxAudLen=Not Indicated\r\n"));
		break;
	case 0x0C:
		iTE_eARCRx_Msg(("RxAudLen=17-bit\r\n"));
		break;
	case 0x0A:
		iTE_eARCRx_Msg(("RxAudLen=20-bit\r\n"));
		break;
	case 0x08:
		iTE_eARCRx_Msg(("RxAudLen=19-bit\r\n"));
		break;
	case 0x04:
		iTE_eARCRx_Msg(("RxAudLen=18-bit\r\n"));
		break;
	case 0x02:
		iTE_eARCRx_Msg(("RxAudLen=16-bit\r\n"));
		break;
	case 0x00:
		iTE_eARCRx_Msg(("RxAudLen=Not Indicated\r\n"));
		break;
	default:
		iTE_eARCRx_Msg(("RxAudLen Error!!!\r\n"));
		break;
	}
	IT662x_eARC_RX_Bank(0);
}

void IT662x_eARC_RX_ShowAdoChSts(void)
{
	iTE_u8 i, rddata[16];

	IT662x_eARC_RX_Bank(1);
	eARC_Rxbrd(0x40, 0x10, &rddata[0]);
	IT662x_eARC_RX_Bank(0);

	iTE_eARCRx_Msg(("eARC RX Channel Status:\r\n"));
	for (i = 0; i < 0x10; i++)
		iTE_eARCRx_Msg(("Reg14%01X=0x%02X \r\n", i, rddata[i]));

}
#if (En_eARCRX == iTE_TRUE)
/********************************************/
/*Function: IT662x_eARC_RX_ForcePDIV*/
/*Note: If auto mode error , FW force to correct PLL's setting*/
/*others:*/
/********************************************/

void IT662x_eARC_RX_ForcePDIV(void)
{
	iTE_u8 RX_FSEL, tmp;

	RX_FSEL = (eARC_Rxrd(0xb2) & 0x70) >> 4;
	iTE_eARCRx_Msg(("RX_FSEL=%d \r\n", RX_FSEL));

	if ((RXBCLK > 50000) && (RX_FSEL != 0)) {
		eARC_Rxset(0xb2, 0xf0, 0x00);
/*iTE_eARCRx_Msg(("ERROR: Detect RX_FSEL=%d, Correct RX_FSEL=0*/
		 /* !!!\n", RX_FSEL ));*/
	} else if ((RXBCLK > 25000) && (RXBCLK <= 50000) && (RX_FSEL != 1)) {
		eARC_Rxset(0xb2, 0xf0, 0x10);
/*iTE_eARCRx_Msg(("ERROR: Detect RX_FSEL=%d, Correct RX_FSEL=1*/
		 /* !!!\n", RX_FSEL ));*/
	} else if ((RXBCLK > 12500) && (RXBCLK <= 25000) && (RX_FSEL != 2)) {
		eARC_Rxset(0xb2, 0xf0, 0x20);
/*iTE_eARCRx_Msg(("ERROR: Detect RX_FSEL=%d, Correct RX_FSEL=2*/
		 /* !!!\n", RX_FSEL ));*/
	} else if ((RXBCLK > 6250) && (RXBCLK <= 12500) && (RX_FSEL != 3)) {
		eARC_Rxset(0xb2, 0xf0, 0x30);
/*iTE_eARCRx_Msg(("ERROR: Detect RX_FSEL=%d, Correct RX_FSEL=3*/
		 /* !!!\n", RX_FSEL ));*/
	} else if ((RXBCLK <= 6250) && (RX_FSEL != 7)) {
		eARC_Rxset(0xb2, 0xf0, 0x70);
/*iTE_eARCRx_Msg(("ERROR: Detect RX_FSEL=%d, Correct RX_FSEL=7*/
		 /* !!!\n", RX_FSEL ));*/
	}
	tmp = (eARC_Rxrd(0xb2) & 0x70) >> 4;
	iTE_eARCRx_Msg(("ERROR: Detect RX_FSEL=%d, Correct RX_FSEL=%d !!!\n",
			RX_FSEL, tmp));
	eARC_Rxset(0xb0, 0x20, 0x20);
	/*mSleep(50);*/
	iTE_eARCRx_Msg(("Reset RX DLL/PLL after parameter changed ...\r\n"));
	eARC_Rxset(0xb0, 0x20, 0x00);
}

void IT662x_eARC_RX_CalRclk(void)
{
	iTE_u32 timer1usint, timer1usflt;
	iTE_u16 bclkbndnum, bclkvalidnum;
	iTE_u32 rddata;
	iTE_u32 sum, timer1us;

	IT662x_eARC_RX_Bank(1);

	sum = 0;
	eARC_Rxwr(0x10, 0x80);
	/*mDelay1ms is not precision, need get system clock*/
	mDelay1ms(99);
	eARC_Rxwr(0x10, 0x00);
	rddata = (iTE_u32)eARC_Rxrd(0x11);
	rddata += (iTE_u32)(eARC_Rxrd(0x12) << 8);
	rddata += (iTE_u32)(eARC_Rxrd(0x13) << 16);
	sum = rddata;
	RXRCLK = sum / 100;
	iTE_eARCRx_Msg(("RXRCLK=%uMHz\r\n", RXRCLK / 1000));
	/*printf("sum=%04x \r\n", sum);*/
	/* update 1us timer*/
	timer1us = sum / 100000;
	timer1usint = (iTE_u16)timer1us;
	timer1usflt = sum % 100000;
	timer1usflt <<= 8;
	timer1usflt /= 100000;
	eARC_Rxwr(0x14, timer1usint);
	eARC_Rxwr(0x15, timer1usflt);
	iTE_eARCRx_Msg(("0x14 = %02x\r\n", eARC_Rxrd(0x14)));
	iTE_eARCRx_Msg(("0x15 =%02x\r\n", eARC_Rxrd(0x15)));
	IT662x_eARC_RX_Bank(0);

	/* calculate sum when eARC_RCLKFreqSel=1 (10MHz)*/
	if (eARC_RCLKFreqSel == 0)
		sum /= 2;
	else if (eARC_RCLKFreqSel == 2)
		sum *= 2;
	else if (eARC_RCLKFreqSel == 3)
		sum *= 4;

	bclkbndnum = sum * 128 / 625000;
	bclkvalidnum = sum * 128 / 400000;
	eARC_Rxwr(0xc4, bclkbndnum);
	eARC_Rxwr(0xc5, bclkvalidnum & 0xFF);
	eARC_Rxwr(0xc6, (bclkvalidnum & 0x100) >> 8);
}
#endif

#if (En_eARCRX == iTE_TRUE)

void IT662x_eARC_RX_ShowAclk(void)
{
	iTE_u16 detaclkavg, detaclkpred2, detaclkpred4, detaclkvalid,
		detaclkstb;
	iTE_u8 rddata;
	iTE_u32 RCLK;

	if (eARC_RCLKFreqSel == 0)
		RCLK = RXRCLK / 2;
	else if (eARC_RCLKFreqSel == 1)
		RCLK = RXRCLK;
	else if (eARC_RCLKFreqSel == 2)
		RCLK = RXRCLK * 2;
	else if (eARC_RCLKFreqSel == 3)
		RCLK = RXRCLK * 4;


	detaclkavg = eARC_Rxrd(0xc8);
	rddata = eARC_Rxrd(0xc9);
	detaclkavg += ((rddata & 0x0F) << 8);
	detaclkpred2 = (rddata & 0x10) >> 4;
	detaclkpred4 = (rddata & 0x20) >> 5;
	detaclkvalid = (rddata & 0x40) >> 6;
	detaclkstb = (rddata & 0x80) >> 7;

	RXACLK = RCLK * 128 / detaclkavg;

	if (detaclkpred4)
		RXACLK *= 4;
	else if (detaclkpred2)
		RXACLK *= 2;

	iTE_eARCRx_Msg(
		("\nCount RXACLK=%uMHz, RxACLKValid=%d, RxACLKStb=%d\r\n",
		 RXACLK / 1000, detaclkvalid, detaclkstb));
}

void IT662x_eARC_RX_ShowBclk(void)
{
	iTE_u16 detbclkavg, detbclkpred2, detbclkpred4, detbclkvalid,
		detbclkstb;
	iTE_u8 rddata;
	iTE_u32 RCLK;

	if (eARC_RCLKFreqSel == 0)
		RCLK = RXRCLK / 2;
	else if (eARC_RCLKFreqSel == 1)
		RCLK = RXRCLK;
	else if (eARC_RCLKFreqSel == 2)
		RCLK = RXRCLK * 2;
	else if (eARC_RCLKFreqSel == 3)
		RCLK = RXRCLK * 4;


	detbclkavg = eARC_Rxrd(0xca);
	rddata = eARC_Rxrd(0xcb);
	detbclkavg += ((rddata & 0x07) << 8);
	detbclkpred2 = (rddata & 0x10) >> 4;
	detbclkpred4 = (rddata & 0x20) >> 5;
	detbclkvalid = (rddata & 0x40) >> 6;
	detbclkstb = (rddata & 0x80) >> 7;

	RXBCLK = RCLK * 128 / detbclkavg;

	if (detbclkpred4)
		RXBCLK *= 4;
	else if (detbclkpred2)
		RXBCLK *= 2;

	iTE_eARCRx_Msg(
		("\nCount RXBCLK=%uMHz, RxBCLKValid=%d, RxBCLKStb=%d\r\n",
		 RXBCLK / 1000, detbclkvalid, detbclkstb));
}
#endif
void enrxdbgfifo(void)
{
	eARC_Rxset(0x50, 0x01, 0x01);
}
void disrxdbgfifo(void)
{
	eARC_Rxset(0x50, 0x01, 0x00);
}
void show_rxdbg_fifo(void)
{
	iTE_u8 i, rddata, rxdbgfifostg, rxdbgfifoerr, rxdbgfifo[64];

	rddata = eARC_Rxrd(0x5F);
	rxdbgfifostg = rddata & 0x3F;
	rxdbgfifoerr = (rddata & 0x40) >> 6;

	if (rxdbgfifostg == 0)
		return;

	iTE_eARCRx_Msg((
		"@@@@@@@@@@ RX DbgFIFO @@@@@@@@@@@@@@@@@@\r\n"));

	do {
		if (rxdbgfifoerr) {
			iTE_eARCRx_Msg(("ERROR: RX Debug FIFO error !!!\r\n"));
			eARC_Rxset(0x5F, 0x80, 0x80);
			return;
		}
		iTE_eARCRx_Msg(("=> RxDbgFifoStg=%d\r\n", rxdbgfifostg));
		eARC_Rxbrd(0x5d, rxdbgfifostg * 2, &rxdbgfifo[0]);

		for (i = 0; i < rxdbgfifostg; i++)
			parse_dbg_fifo(rxdbgfifo[i * 2], rxdbgfifo[i * 2 + 1]);

		rddata = eARC_Rxrd(0x5F);
		rxdbgfifostg = rddata & 0x3F;
		rxdbgfifoerr = (rddata & 0x40) >> 6;
	} while (rxdbgfifostg != 0);
}

void parse_dbg_fifo(int data0, int data1)
{
	iTE_eARCRx_Msg(("DbgData=0x%02X%02X, ", data1, data0));
	if (data1 & 0x04) {
		iTE_eARCRx_Msg(("ERROR: ECC error !!!\r\n"));
		return;
	}

	switch (data1 & 0x03) {
	case 0:
		iTE_eARCRx_Msg(("Input  Data = "));
		break;
	case 1:
		iTE_eARCRx_Msg(("Input  Cmd  = "));
		break;
	case 2:
		iTE_eARCRx_Msg(("Output Data = "));
		break;
	case 3:
		iTE_eARCRx_Msg(("Output Cmd  = "));
		break;
	}

	if ((data1 & 0x01) == 0x00) {
		iTE_eARCRx_Msg(("0x%02X\r\n", data0));
	} else {
		switch (data0) {
		case 0x01:
			iTE_eARCRx_Msg(("eARC_READ\r\n"));
			break;
		case 0x02:
			iTE_eARCRx_Msg(("eARC_WRITE\r\n"));
			break;
		case 0x04:
			iTE_eARCRx_Msg(("ACK\r\n"));
			break;
		case 0x08:
			iTE_eARCRx_Msg(("NACK\r\n"));
			break;
		case 0x10:
			iTE_eARCRx_Msg(("CONT\r\n"));
			break;
		case 0x20:
			iTE_eARCRx_Msg(("STOP\r\n"));
			break;
		case 0x40:
			iTE_eARCRx_Msg(("RETRY\r\n"));
			break;
		default:
			iTE_eARCRx_Msg(("Unknown !!!\r\n"));
			break;
		}
	}
}
void IT662x_eARC_RX_Bank(iTE_u8 bankno)
{
	eARC_Rxset(0x0f, 0x03, bankno & 0x03);
}
/********************************************/
/*Function: IT662x_eARC_RX_SetRxCapDataStruct*/
/*Note: Write audio capability data structure here*/
/*others: ptr =>  array's pointer , array size should be 256 byte*/
/********************************************/

void IT662x_eARC_RX_SetRxCapDataStruct(iTE_u8 const *ptr)
{
	iTE_u8 i;
	static iTE_u16 test_loop;
	/*iTE_u8 rxcap_data[256] = ;*/

	eARC_Rxwr(0xfb, RxCapAddr | 0x01);
	/* enable RX capabilities data structure slave address*/

	for (i = 0; i < 255; i++) {
		/*if( EnCapTest ) rxcap_data[i] = (test_loop*128+i)&0xFF; */
		/* temp data*/
		RxCapwr(i, *ptr);
		ptr++;
	}

	if (EnCapTest) {
		test_loop++;
		iTE_eARCRx_Msg(
			("==> RX Capabilities change test loop %d ...\r\n",
			 test_loop));
	}
	eARC_Rxset(
		0xfb, 0x01,
		0x00); /* disable RX capabilities data structure slave address*/
	eARC_Rxset(0x52, 0x08, 0x08); /* CAP_CHNG=1*/
}

#if (En_eARCRX == iTE_TRUE)

void IT662x_eARC_RX_CheAdoInfoFrame(void)
{
	iTE_u8 i, expect_value;
	static iTE_u8 test_loop;
	iTE_u8 rxaif_data[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	IT662x_eARC_RX_Bank(1);
	for (i = 0; i < 7; i++) {
		rxaif_data[i] = eARC_Rxrd(0x49 + i);
		expect_value = (test_loop * 7 + i) & 0xFF;
		if (i == 1)
			expect_value &= 0xFB; /* ignore MUTE*/

		if (rxaif_data[i] != expect_value) {
			iTE_eARCRx_Msg((
				"AIF check error: rxaif_data[%d]=0x%02X, expect_value=0x%02X\r\n",
				i, rxaif_data[i], expect_value));
		}
	}
	IT662x_eARC_RX_Bank(0);
	test_loop++;
	iTE_eARCRx_Msg(("==> AIF check loop count = %d\r\n", test_loop));
}

void check_rxpkt1(void)
{
	iTE_u8 i, j, expect_value, crc, data_byte, data_bit;
	iTE_u8 crc_b0, crc_b1, crc_b2, crc_b3, crc_b4, crc_b5, crc_b6, crc_b7,
		crc_tmp;
	static iTE_u8 test_loop1;
	iTE_u8 rxpkt1_data[33] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00};

	IT662x_eARC_RX_Bank(1);
	rxpkt1_data[1] = eARC_Rxrd(0xa1);
	expect_value = 0x00;
	if (rxpkt1_data[1] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt1 check error: rxpkt1_data[1]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt1_data[1], expect_value));
	}

	rxpkt1_data[2] = eARC_Rxrd(0xa2);
	expect_value = 0x04;
	if (rxpkt1_data[2] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt1 check error: rxpkt1_data[2]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt1_data[2], expect_value));
	}

	for (i = 3; i < 33; i++) {
		rxpkt1_data[i] = eARC_Rxrd(0xa0 + i);
		expect_value = (test_loop1 * 33 + i) & 0xFF;

		if (rxpkt1_data[i] != expect_value) {
			iTE_eARCRx_Msg((
				"Pkt1 check error: rxpkt1_data[%d]=0x%02X, expect_value=0x%02X\r\n",
				i, rxpkt1_data[i], expect_value));
		}
	}

	crc_b0 = crc_b1 = crc_b2 = crc_b3 = crc_b4 = crc_b5 = crc_b6 = crc_b7 =
		1;
	for (i = 1; i < 33; i++) {
		data_byte = rxpkt1_data[i];
		for (j = 7; j != 0; j--) {
			data_bit = (data_byte & (0x01 << j)) >> j;
			crc_tmp = crc_b7 ^ data_bit;
			crc_b7 = crc_b6;
			crc_b6 = crc_b5;
			crc_b5 = crc_b4;
			crc_b4 = crc_tmp ^ crc_b3;
			crc_b3 = crc_tmp ^ crc_b2;
			crc_b2 = crc_tmp ^ crc_b1;
			crc_b1 = crc_b0;
			crc_b0 = crc_tmp;
		}
	}
	crc = (crc_b7 << 7) + (crc_b6 << 6) + (crc_b5 << 5) + (crc_b4 << 4) +
	      (crc_b3 << 3) + (crc_b2 << 2) + (crc_b1 << 1) + crc_b0;
	rxpkt1_data[0] = eARC_Rxrd(0xa0);
	expect_value = crc;
	if (rxpkt1_data[0] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt1 check error: rxpkt1_data[0]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt1_data[0], expect_value));
	}
	IT662x_eARC_RX_Bank(0);

	test_loop1++;
	iTE_eARCRx_Msg(("==> Pkt1 check loop count = %d\r\n", test_loop1));
}

void check_rxpkt2(void)
{
	iTE_u8 i, j, expect_value, crc, data_byte, data_bit;
	iTE_u8 crc_b0, crc_b1, crc_b2, crc_b3, crc_b4, crc_b5, crc_b6, crc_b7,
		crc_tmp;
	static iTE_u8 test_loop2;
	iTE_u8 rxpkt2_data[21] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	IT662x_eARC_RX_Bank(1);
	rxpkt2_data[1] = eARC_Rxrd(0xc2);
	expect_value = 0x00;
	if (rxpkt2_data[1] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt2 check error: rxpkt2_data[1]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt2_data[1], expect_value));
	}

	rxpkt2_data[2] = eARC_Rxrd(0xc3);
	expect_value = 0x05;
	if (rxpkt2_data[2] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt2 check error: rxpkt2_data[2]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt2_data[2], expect_value));
	}

	for (i = 3; i < 21; i++) {
		rxpkt2_data[i] = eARC_Rxrd(0xc1 + i);
		expect_value = (test_loop2 * 21 + i) & 0xFF;

		if (rxpkt2_data[i] != expect_value) {
			iTE_eARCRx_Msg((
				"Pkt2 check error: rxpkt2_data[%d]=0x%02X, expect_value=0x%02X\r\n",
				i, rxpkt2_data[i], expect_value));
		}
	}

	crc_b0 = crc_b1 = crc_b2 = crc_b3 = crc_b4 = crc_b5 = crc_b6 = crc_b7 =
		1;
	for (i = 1; i < 21; i++) {
		data_byte = rxpkt2_data[i];
		for (j = 7; j != 0; j--) {
			data_bit = (data_byte & (0x01 << j)) >> j;
			crc_tmp = crc_b7 ^ data_bit;
			crc_b7 = crc_b6;
			crc_b6 = crc_b5;
			crc_b5 = crc_b4;
			crc_b4 = crc_tmp ^ crc_b3;
			crc_b3 = crc_tmp ^ crc_b2;
			crc_b2 = crc_tmp ^ crc_b1;
			crc_b1 = crc_b0;
			crc_b0 = crc_tmp;
		}
	}
	crc = (crc_b7 << 7) + (crc_b6 << 6) + (crc_b5 << 5) + (crc_b4 << 4) +
	      (crc_b3 << 3) + (crc_b2 << 2) + (crc_b1 << 1) + crc_b0;
	rxpkt2_data[0] = eARC_Rxrd(0xc1);
	expect_value = crc;
	if (rxpkt2_data[0] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt2 check error: rxpkt2_data[0]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt2_data[0], expect_value));
	}
	IT662x_eARC_RX_Bank(0);

	test_loop2++;
	iTE_eARCRx_Msg(("==> Pkt2 check loop count = %d\r\n", test_loop2));
}

void check_rxpkt3(void)
{
	iTE_u8 i, j, expect_value, crc, data_byte, data_bit;
	iTE_u8 crc_b0, crc_b1, crc_b2, crc_b3, crc_b4, crc_b5, crc_b6, crc_b7,
		crc_tmp;
	static iTE_u8 test_loop3;
	iTE_u8 rxpkt3_data[21] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	IT662x_eARC_RX_Bank(1);
	rxpkt3_data[1] = eARC_Rxrd(0xd7);
	expect_value = 0x00;
	if (rxpkt3_data[1] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt3 check error: rxpkt3_data[1]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt3_data[1], expect_value));
	}

	rxpkt3_data[2] = eARC_Rxrd(0xd8);
	expect_value = 0x06;
	if (rxpkt3_data[2] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt3 check error: rxpkt3_data[2]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt3_data[2], expect_value));
	}

	for (i = 3; i < 21; i++) {
		rxpkt3_data[i] = eARC_Rxrd(0xd6 + i);
		expect_value = (test_loop3 * 21 + i) & 0xFF;

		if (rxpkt3_data[i] != expect_value) {
			iTE_eARCRx_Msg((
				"Pkt3 check error: rxpkt3_data[%d]=0x%02X, expect_value=0x%02X\r\n",
				i, rxpkt3_data[i], expect_value));
		}
	}

	crc_b0 = crc_b1 = crc_b2 = crc_b3 = crc_b4 = crc_b5 = crc_b6 = crc_b7 =
		1;
	for (i = 1; i < 21; i++) {
		data_byte = rxpkt3_data[i];
		for (j = 7; j != 0; j--) {
			data_bit = (data_byte & (0x01 << j)) >> j;
			crc_tmp = crc_b7 ^ data_bit;
			crc_b7 = crc_b6;
			crc_b6 = crc_b5;
			crc_b5 = crc_b4;
			crc_b4 = crc_tmp ^ crc_b3;
			crc_b3 = crc_tmp ^ crc_b2;
			crc_b2 = crc_tmp ^ crc_b1;
			crc_b1 = crc_b0;
			crc_b0 = crc_tmp;
		}
	}
	crc = (crc_b7 << 7) + (crc_b6 << 6) + (crc_b5 << 5) + (crc_b4 << 4) +
	      (crc_b3 << 3) + (crc_b2 << 2) + (crc_b1 << 1) + crc_b0;
	rxpkt3_data[0] = eARC_Rxrd(0xd6);
	expect_value = crc;
	if (rxpkt3_data[0] != expect_value) {
		iTE_eARCRx_Msg((
			"Pkt3 check error: rxpkt3_data[0]=0x%02X, expect_value=0x%02X\r\n",
			rxpkt3_data[0], expect_value));
	}
	IT662x_eARC_RX_Bank(0);

	test_loop3++;
	iTE_eARCRx_Msg(("==> Pkt3 check loop count = %d\r\n", test_loop3));
}
#ifdef FWI2C
/********************************************/
/*Function: IT662x_eARC_RX_Read_EDID*/
/*Note: Use I2S6 & I2S7 as DDC bus*/
/*others:*/
/********************************************/

void IT662x_eARC_RX_Read_EDID(void)
{
	iTE_u8 tmp[0x90], i, ret;
	iTE_u8 dat;

	eARC_Rxset(0xEB, 0x11, 0x11);
	for (i = 0; i < 0x10; i++) {
		ret = i2c_read_byte_v1(0xA0, i, 1, &dat);
		tmp[i] = dat;
		if (ret) {
		} else {
			printf(" read fail  break \r\n");
			break;
		}
	}
	printf("\r\n");
	printf("Parse EDID Start \r\n");

	for (i = 0; i < 0x80; i++) {
		printf("  ");
		printf("%02x ", tmp[i]);
		if (i && (((i + 1) % 16) == 0))
			pr_debug("\r\n");

	}
	for (i = 0; i < 0x10; i++) {
		ret = i2c_read_byte_v1(0xA0, i + 0x80, 1, &dat);
		tmp[i] = dat;
		if (ret) {
		} else {
			printf(" read fail  break \r\n");
			break;
		}
	}
	printf("\r\n");
	for (i = 0; i < 0x80; i++) {
		printf("  ");
		printf("%02x ", tmp[i]);
		if (i && (((i + 1) % 16) == 0))
			printf("\r\n");

	}
	printf("\r\n");
}

#define logi2c(x)

#if 0
#define ENABLE_TIMER_INTERRUPT(x)                                              \
	{                                                                      \
		ET3CTRL = ET_3_8_EN;                                           \
		ET7CTRL = ET_3_8_EN;                                           \
	}
#define DISABLE_TIMER_INTERRUPT(x)                                             \
	{                                                                      \
		ET7CTRL = 0x00;                                                \
		ET3CTRL = 0x00;                                                \
	}
#else
#define ENABLE_TIMER_INTERRUPT(x)                                              \
	{                                                                      \
	}
#define DISABLE_TIMER_INTERRUPT(x)                                             \
	{                                                                      \
	}
#endif

#define SCL_PORT_0_IN (__get_scl())
#define SDA_PORT_0_IN (__get_sda())

#if 0
#define SDA_OUTPUT_ENABLE(x)                                                   \
	{                                                                      \
		EECON |= SDA_PORT_0_OE_MASK;                                   \
	}
#define SCL_OUTPUT_ENABLE(x)                                                   \
	{                                                                      \
		EECON |= SCL_PORT_0_OE_MASK;                                   \
	}
#define SDA_OUTPUT_DISABLE(x)                                                  \
	{                                                                      \
		SDA_PORT_0_IN = 1;                                             \
		EECON &= ~SDA_PORT_0_OE_MASK;                                  \
	}
#define SCL_OUTPUT_DISABLE(x)                                                  \
	{                                                                      \
		SCL_PORT_0_IN = 1;                                             \
		EECON &= ~SCL_PORT_0_OE_MASK;                                  \
	}
#else
#if 0
#define SDA_OUTPUT_ENABLE(x)                                                   \
	{                                                                      \
		GPIO_Operation_Mode(GPIOA5, OUTPUT, OutputType_Open_Drain);    \
	}
#define SCL_OUTPUT_ENABLE(x)                                                   \
	{                                                                      \
		GPIO_Operation_Mode(GPIOA4, OUTPUT, OutputType_Open_Drain);    \
	}
#define SDA_OUTPUT_DISABLE(x)                                                  \
	{                                                                      \
		GPIO_Operation_Mode(GPIOA5, INPUT | PULL_UP,                   \
				    OutputType_Open_Drain);                    \
	}
#define SCL_OUTPUT_DISABLE(x)                                                  \
	{                                                                      \
		GPIO_Operation_Mode(GPIOA4, INPUT | PULL_UP,                   \
				    OutputType_Open_Drain);                    \
	}
#else
#define SDA_OUTPUT_ENABLE(x)                                                   \
	{                                                                      \
		eARC_Rxset(0xEB, 0x02, 0x02);                                  \
	}
#define SCL_OUTPUT_ENABLE(x)                                                   \
	{                                                                      \
		eARC_Rxset(0xEB, 0x20, 0x20);                                  \
	}
#define SDA_OUTPUT_DISABLE(x)                                                  \
	{                                                                      \
		eARC_Rxset(0xEB, 0x02, 0x00);                                  \
	}
#define SCL_OUTPUT_DISABLE(x)                                                  \
	{                                                                      \
		eARC_Rxset(0xEB, 0x20, 0x00);                                  \
	}
#endif
#endif

#if 0
#define SET_SDA(x)                                                             \
	{                                                                      \
		SDA_PORT_0 = x;                                                \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_R(x)                                                           \
	{                                                                      \
		SCL_PORT_0 = x;                                                \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_W0(x)                                                          \
	{                                                                      \
		SCL_PORT_0 = 0;                                                \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_W1(x)                                                          \
	{                                                                      \
		SCL_PORT_0 = 1;                                                \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_ACK0(x)                                                        \
	{                                                                      \
		SCL_PORT_0 = 0;                                                \
		i2c_clock_delay();                                             \
		;                                                              \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_ACK1(x)                                                        \
	{                                                                      \
		SCL_PORT_0 = 1;                                                \
		i2c_clock_delay();                                             \
		;                                                              \
		i2c_clock_delay();                                             \
	}
#else
#define SET_SDA(x)                                                             \
	{                                                                      \
		__set_sda(x);                                                  \
	}
#define SET_SCL(x)                                                             \
	{                                                                      \
		__set_scl(x);                                                  \
	}
#define SET_SCL_R0(x)                                                          \
	{                                                                      \
		__set_scl(0);                                                  \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_R1(x)                                                          \
	{                                                                      \
		__set_scl(1);                                                  \
	}
#define SET_SCL_R(x)                                                           \
	{                                                                      \
		SET_SCL_R_X(x);                                                \
	}
#define SET_SCL_W0(x)                                                          \
	{                                                                      \
		__set_scl(0);                                                  \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_W1(x)                                                          \
	{                                                                      \
		__set_scl(1);                                                  \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_ACK0(x)                                                        \
	{                                                                      \
		__set_scl(0);                                                  \
		i2c_clock_delay();                                             \
	}
#define SET_SCL_ACK1(x)                                                        \
	{                                                                      \
		__set_scl(1);                                                  \
		i2c_clock_delay();                                             \
	}

void SET_SCL_R_X(iTE_u8 x)
{
	if (x) {
		__set_scl(0);
		i2c_clock_delay();
		i2c_clock_delay();
		i2c_clock_delay();
		i2c_clock_delay();
		i2c_clock_delay();
		i2c_clock_delay();
		i2c_clock_delay();
		i2c_clock_delay();
	} else {
		__set_scl(1);
	}
}
void i2c_clock_delay(void)
{
	/* _nop_();_nop_();_nop_();*/
}

void __set_sda(iTE_u8 x)
{
	if (x == 0) {
		/* SDA_PORT_0 = 0;*/
		eARC_Rxset(0xEB, 0x04, 0x00);
		SDA_OUTPUT_ENABLE();
	} else {
		SDA_OUTPUT_DISABLE();
		/* SDA_PORT_0 = 1;*/
		eARC_Rxset(0xEB, 0x04, 0x04);
	}
}
void __set_scl(iTE_u8 x)
{
	if (x == 0) {
		/*SCL_PORT_0 = 0;*/
		eARC_Rxset(0xEB, 0x40, 0x00);
		SCL_OUTPUT_ENABLE();
	} else {

		SCL_OUTPUT_DISABLE();
		/* SCL_PORT_0 = 1;*/
		eARC_Rxset(0xEB, 0x40, 0x40);
	}
}

iTE_u8 __get_scl(void)
{
	if (eARC_Rxrd(0xEB) & 0x80)
		return 1;

	return 0;
}

iTE_u8 __get_sda(void)
{
	if (eARC_Rxrd(0xEB) & 0x08)
		return 1;

	return 0;
}
#endif
void i2c_start_delay(void)
{
	/*_nop_();_nop_();_nop_();*/
}
iTE_u8 i2c_8051_wait_arbit(void)
{
	iTE_u8 i, count;

	SCL_OUTPUT_DISABLE();
	SDA_OUTPUT_DISABLE();

	for (i = 0, count = 0; i < 20; i++) {
		if (SDA_PORT_0_IN && SCL_PORT_0_IN) {
			count++;
			if (count > 3)
				return 1;
		} else {
			count = 0;
		}
	}

	for (i = 0; i < 23; i++) {
		SET_SCL_W1();
		SET_SCL_W0();
		SET_SCL_W1();
		i2c_clock_delay();
		i2c_clock_delay();
		i2c_clock_delay();

		if (SDA_PORT_0_IN && SCL_PORT_0_IN)
			return 1;

	}

	return 0;
}

void i2c_8051_start(void)
{
	/*SCL_OUTPUT_ENABLE();*/
	/*SDA_OUTPUT_ENABLE();*/

	SET_SDA(1);
	SET_SCL(1);
	i2c_start_delay();
	i2c_start_delay();
	SET_SDA(0);
	/*i2c_start_delay();*/
	SET_SCL(0);
	i2c_start_delay();
	i2c_start_delay();
	i2c_start_delay();
}

iTE_u8 i2c_8051_wait_ack(void)
{
	iTE_u8 ack_bit_value;

	/*SDA_OUTPUT_DISABLE_2(0);*/
	/*SDA_OUTPUT_DISABLE();*/
	SET_SDA(1);
	/*SCL_OUTPUT_ENABLE();*/

	/*SDA_OUTPUT_ENABLE();*/
	/*i2c_clock_delay2(5);*/
	SET_SCL_ACK1();
	/*SCL_PORT_0 = 1;*/

	/*SDA_PORT_0 = 1;*/
	/*SDA_OUTPUT_DISABLE();*/

	i2c_clock_delay();
	ack_bit_value = SDA_PORT_0_IN;
	/*i2c_clock_delay2(10);*/
	/*SCL_PORT_0 = 0;*/
	SET_SCL_ACK0();

	return ack_bit_value;
}

#if 1
iTE_u8 i2c_8051_read(iTE_u8 ack_bit)
{
	iTE_u8 byte_data;
	iTE_u8 bit_value;

	byte_data = 0;

	DISABLE_TIMER_INTERRUPT();

	/*SCL_OUTPUT_ENABLE();*/
	/*SDA_OUTPUT_DISABLE_2(0);*/
	SDA_OUTPUT_DISABLE();
	/*SET_SDA(0);*/

	SET_SCL_R(1);
	byte_data <<= 1;
	bit_value = SDA_PORT_0_IN; /* bit7*/
	SET_SCL_R(0);
	byte_data |= bit_value;

	SET_SCL_R(1);
	byte_data <<= 1;
	bit_value = SDA_PORT_0_IN; /* bit6*/
	SET_SCL_R(0);
	byte_data |= bit_value;

	SET_SCL_R(1);
	byte_data <<= 1;
	bit_value = SDA_PORT_0_IN; /* bit5*/
	SET_SCL_R(0);
	byte_data |= bit_value;

	SET_SCL_R(1);
	byte_data <<= 1;
	bit_value = SDA_PORT_0_IN; /* bit4*/
	SET_SCL_R(0);
	byte_data |= bit_value;

	SET_SCL_R(1);
	byte_data <<= 1;
	bit_value = SDA_PORT_0_IN; /* bit3*/
	SET_SCL_R(0);
	byte_data |= bit_value;

	SET_SCL_R(1);
	byte_data <<= 1;
	bit_value = SDA_PORT_0_IN; /* bit2*/
	SET_SCL_R(0);
	byte_data |= bit_value;

	SET_SCL_R(1);
	byte_data <<= 1;
	bit_value = SDA_PORT_0_IN; /* bit1*/
	SET_SCL_R(0);
	byte_data |= bit_value;

	SET_SCL_R(1);
	byte_data <<= 1;
	bit_value = SDA_PORT_0_IN; /* bit0*/
	SET_SCL_R(0);
	byte_data |= bit_value;

	/*SDA_OUTPUT_ENABLE();*/
	SET_SDA(ack_bit);

	/*SDA_PORT_0 = ack;*/
	/*i2c_clock_delay();*/
	/*i2c_clock_delay();*/
	SET_SCL_R(1);
	SET_SCL_R(0);
	/*SDA_PORT_0 = 1;*/
	SET_SDA(1);

	ENABLE_TIMER_INTERRUPT();

	return byte_data;
}
#endif
void i2c_8051_end(void)
{
	/*SCL_OUTPUT_ENABLE();*/
	/*SDA_OUTPUT_ENABLE();*/

	SET_SDA(0);
	i2c_clock_delay();
	SET_SCL_W1();
	SET_SDA(1);
}
iTE_u8 i2c_write_byte_v1(iTE_u8 address, iTE_u8 offset, iTE_u8 byteno,
			 iTE_u8 *p_data)
{
	iTE_u8 i;
#if 1
	if (!i2c_8051_wait_arbit()) {
		/*logi2c(("i2c W failed - arbit\n"));*/
		return FALSE;
	}
#endif
	i2c_8051_start(); /* S*/

	i2c_8051_write(address & 0xFE); /* slave address (W)*/
	if (i2c_8051_wait_ack() == 1) { /* As*/
		i2c_8051_end();
		/*logi2c(("i2c W failed - slave address(W) not ack\n"));*/
		return 0;
	}

	i2c_8051_write(offset);		/* offset*/
	if (i2c_8051_wait_ack() == 1) { /* As*/
		i2c_8051_end();
		/*logi2c(("i2c W failed - offset not ack\n"));*/

		return 0;
	}

	for (i = 0; i < byteno - 1; i++) {
		i2c_8051_write(*p_data);	/* write d*/
		if (i2c_8051_wait_ack() == 1) { /* As*/
			i2c_8051_end();
			/*logi2c(("i2c W failed - data[%d] not ack\n",*/
			/* (int)i));*/
			/*printf("w write d( i2c_8051_wait_ack()==1 )\n");*/
			return 0;
		}
		p_data++;
	}

	i2c_8051_write(*p_data);	/* write last d*/
	if (i2c_8051_wait_ack() == 1) { /* As*/
		i2c_8051_end();
		/*logi2c(("i2c W failed - data[%d] not ack\n",*/
		 /* (int)(byteno-1)));*/

		/*return 0;*/
	} else {
		i2c_8051_end();
		/*return 1;*/
	}
	return 0;
}
iTE_u8 i2c_read_byte_v1(iTE_u8 address, iTE_u8 offset, iTE_u8 byteno,
			iTE_u8 *p_data)
{
	iTE_u8 i;

#if 1
	if (!i2c_8051_wait_arbit()) {
		/*VGA_DEBUG_PRINTF5(("i2c_read_byte(%02X,%02X,%02X) arbit*/
		 /* fail.\n",(int)address,(int)offset,(int)byteno));*/
		/*printf("i2c arbit  fail\r\n");*/
		return FALSE;
	}
#endif
	i2c_8051_start();		/* S*/
	i2c_8051_write(address & 0xFE); /* slave address (W)*/
	if (i2c_8051_wait_ack() == 1) { /* As*/
		i2c_8051_end();
		/*printf("r slave address (W)( i2c_8051_wait_ack()==1 )\n");*/
		/*printf("slave address 0x%x no ack \r\n",address);*/
		return 0;
	}

	i2c_8051_write(offset);		/* offset*/
	if (i2c_8051_wait_ack() == 1) { /* As*/
		i2c_8051_end();
		/*printf("r offset( i2c_8051_wait_ack()==1 )\n");*/
		/*printf("offset 0x%x no ack \r\n",address);*/
		return 0;
	}

	i2c_8051_start();

	i2c_8051_write(address | 0x01); /* slave address (R)*/
	if (i2c_8051_wait_ack() == 1) { /* As*/
		i2c_8051_end();
		/*printf("r slave address (R)( i2c_8051_wait_ack()==1 )\n");*/
		printf("slave address 0x%x no ack \r\n", address);
		return 0;
	}

	for (i = 0; i < byteno - 1; i++) {
		*p_data = i2c_8051_read(0); /* read d*/
		p_data++;
	}

	*p_data = i2c_8051_read(1); /* read last d*/

	i2c_8051_end();

	return 1;
}

#endif
#endif

#if (IS_IT663XX == IT66321)
void timer(iTE_u32 x)
{
}
void eI2C_API1(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Count, iTE_pu8 pu8Data)
{
}
void eI2C_API2(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Count, iTE_pu8 pu8Data)
{
}
void eI2C_API3(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8InvMask, iTE_u8 u8Data)
{
}
void eI2C_API4(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Data)
{
}
iTE_u8 eI2C_API5(iTE_u8 addr, iTE_u8 u8Offset)
{
	return 0;
}
void eI2C_API6(iTE_u8 addr, iTE_u8 *u8Offset, iTE_u8 u8Data)
{
}
#endif
#endif

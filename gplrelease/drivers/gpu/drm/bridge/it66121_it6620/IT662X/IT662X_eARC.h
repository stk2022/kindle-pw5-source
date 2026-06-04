/*****************************************
 *  Copyright (C) 2009-2019
 *  ITE Tech. Inc. All Rights Reserved
 *  Proprietary and Confidential
 *****************************************
 *   @file   <IT662X_eARC.h>
 *   @author Hojim.Tseng@ite.com.tw
 *   @date   2019/01/16
 *   @fileversion: ITE_eARC_RX_0.70
 */
#ifndef _eARC_H_
#define _eARC_H_

#define _QD980ATC_
#define IT662X_CEC_CodeBase
#define En_eARCRX (iTE_TRUE)

/*#define FWI2C*/

#ifndef IS_IT663XX
#define IS_IT663XX (0)
#endif
#ifndef IT66321
#define IT66321 (66321)
#endif

#if (IS_IT663XX == IT66321)
#include "../IT6634_Customer/IT6634_Config.h"
#else
#include "IT662X_typedef.h"
#include <linux/unistd.h>
#include <linux/delay.h>
#endif

#ifdef IT662X_CEC_CodeBase
#include "IT662X_CecSys.h"
#include "IT662X_Cec.h"
#endif

#ifndef En_eARCTX
#define En_eARCTX (iTE_FALSE)
#endif
#ifndef En_eARCRX
#define En_eARCRX (iTE_FALSE)
#endif

#ifndef iTE_eARC_Msg_Print
#define iTE_eARC_Msg_Print (iTE_TRUE)
#endif

#if (iTE_eARC_Msg_Print == iTE_TRUE)
#define iTE_eARCTx_Msg(x)                                                      \
	do {                                                                   \
		iTE_MsgA(("\r[ eARC_Tx ] "));                                  \
		iTE_MsgA(x);                                                   \
	} while (0)
#define iTE_eARCRx_Msg(x)                                                      \
	do {                                                                   \
		iTE_MsgA(("\r[ eARC_Rx ] "));                                  \
		iTE_MsgA(x);                                                   \
	} while (0)
#else
#define iTE_eARCTx_Msg(x)
#define iTE_eARCRx_Msg(x)
#endif

#define eARCTxAddr 0x9e
#define eARCRxAddr 0x9C
#define eARCTX_CEC_ADR 0xbc
#define eARCRX_CEC_ADR 0xbe
#define RxCapAddr 0xc6

/*#define CEC_ADR          0xC8//IT6220 CEC*/
/*#define CEC_ADR_IT6221   0xCA//IT6221 CEC*/

#define AUD16BIT 0x2
#define AUD18BIT 0x4
#define AUD20BIT 0x3
#define AUD24BIT 0xB

#define AUDCAL1 0x4
#define AUDCAL2 0x0
#define AUDCAL3 0x8

#define UN2CHLPCM 0x00
#define UNMCHLPCM 0x10
#define UNXCHDSD 0x18
#define UN2CHNLPCM 0x02
#define EN2CHNLPCM 0x06
#define ENMCHNLPCM 0x16
#define ENXCHDSD 0x1E

#define LPCM2CH 0x0
#define LPCM8CH 0x7
#define LPCM16CH 0xB
#define LPCM32CH 0x3
#define NLPCM2CH 0x0
#define NLPCM8CH 0x7
#define DSD6CH 0x5
#define DSD12CH 0x9

#define TxDbgGup0 0x0
#define TxDbgGup1 0x1
#define TxDbgGup2 0x2
#define TxDbgGup3 0x3
#define TxDbgGup4 0x4
#define TxDbgGup5 0x5
#define TxDbgGup6 0x6
#define TxDbgGup7 0x7
#define RxDbgGup0 0x8
#define RxDbgGup1 0x9
#define RxDbgGup2 0xa
#define RxDbgGup3 0xb
#define RxDbgGup4 0xc
#define RxDbgGup5 0xd
#define RxDbgGup6 0xe
#define RxDbgGup7 0xf

/*#define EN_CEC 1*/
#define TDM (2)
#define CMDWAITTIME (1) /*000*/
#define CMDWAITNUM (10)
#define eARC_RCLKFreqSel (1) /* 0: 20MHz, 1: 10MHz, 2: 5MHz, 3: 2.5MHz*/
#define RxCapRdByte (16)
#define EnUpdAvg (iTE_TRUE)
#define EnSCKInv (iTE_FALSE)  /* for I2S*/
#define EnTCKInv (iTE_TRUE)   /* for TDM*/
#define EnMCLKInv (iTE_TRUE)  /* for SPDIF*/
#define EnDCLKInv (iTE_FALSE) /* for DSD*/
/* Loop Test option*/
#define EnCapTest (iTE_FALSE)
#define EnAIFTest (iTE_FALSE)
#define EnPkt1Test (iTE_FALSE)
#define EnPkt2Test (iTE_FALSE)
#define EnPkt3Test (iTE_FALSE)
/* AFE option*/
#define TX_DRV_CSW (4)  /* 0; minimum, 7: maximum*/
#define TX_DRV_CSR (4)  /* 0: fastest, 7: slowest*/
#define TX_RC_CKSEN (1) /* +5 HYS @ A1ECO*/
#define TX_ENHYS (7)
#define TX_DRV_DSW (7)
#define TX_DRV_DVAMP (1)
#define TX_XP_DEI (0)
#define TX_XP_ER0 (0)

/* AFE option*/
#define RX_RC_CKSEN (1) /* +5 HYS @ A1ECO*/
#define RX_ENHYS (7)
#define RX_DRV_CSW (4) /* 0; minimum, 7: maximum*/
#define RX_DRV_CSR (4) /* 0: fastest, 7: slowest*/
#define RX_RLV (4)     /* Termination: 0 => min, 7 => max, default => 4*/
#define RX_CDRSEL (0)  /* 0: DLL, 1: PLL*/
#define RX_ENEQ (0)
#define RX_ENI2 (1)
#define RX_VCMSEL (0) /* default: 0*/
/* Discovery Option*/
#define TxEneARC (iTE_TRUE) /* set FALSE for ARC*/
#define RxEneARC (iTE_TRUE) /* set FALSE for ARC*/
#define TxEnARC (iTE_TRUE)
#define RxEnARC (iTE_TRUE)
#define EnterARCNow (iTE_FALSE) /* TRUE: for ARC mode*/
#define ForceARCMode (iTE_FALSE)
/* TX CMDC option*/
#define TxEnCmdTO (iTE_FALSE)
#define TxEnHBRetry (iTE_TRUE)
#define TxHBRtySel (0)		 /* 0: 0ms, 1: 4ms, 2: 8ms, 3: 16ms*/
#define TxAutoWrStat (iTE_FALSE) /* set FALSE to pass SL-870 HFR5-1-35/36/37*/
/* RX CMDC option*/
#define RxEnMaxHBChk (iTE_TRUE)
/* DMCD option*/
#define TurnOverSel   (2)
/* 0: 8us, 1: 16us, 2: 24us, 3: Rsvd    // set 2 for QD980  */
	       /*HFR5-1-21*/
#define TxNxtPktTOSel (1) /* 0: 30us, 1: 50us*/
/* Clock Inversion Option*/
#define TxACLKInv (iTE_FALSE)
#define TxBCLKInv (iTE_FALSE)
#define RxACLKInv (iTE_FALSE)
#define RxBCLKInv (iTE_FALSE)
#define RxEnSCKInv (iTE_FALSE)
#define RxEnMCLKInv (iTE_FALSE)
/* Rx Audio Option*/
#define RxEnTDM (iTE_FALSE)
#define RxEnI2SHBR (iTE_FALSE)
#define RxEnI2SC (iTE_TRUE)
#define RxECCOpt (iTE_FALSE)
#define RxCChOpt (iTE_FALSE)
#define RxUBitOpt (iTE_TRUE)
#define RxEnMuteF (iTE_TRUE)
#define RxEnMuteB (iTE_TRUE)
#define RxMuteLPCM (iTE_TRUE)
#define RxMuteNLPCM (iTE_FALSE)
#define RxMuteDSD (iTE_TRUE)
#define RxMuteStep (4)

#define TxEncOpt (3)
#define TxEncSeed (0xA5C3)
#define TxECCOpt (iTE_FALSE)
#define TxCChOpt (iTE_FALSE)
#define TxUBitOpt (iTE_TRUE)
#define TxEnExtMute                                                            \
	(iTE_TRUE) /* RegEnTxMute=RegF0[1] at FPGA register (SlvAddr=0x88)*/
#define TxI2SFmt (1)
#define TX_VCMSEL (0) /* default: 0*/

typedef struct {
	iTE_u8 eARC_RX_event;
} eARCRX_u8Data;

typedef struct {
	iTE_u8 eARC_TX_event;
	iTE_u8 eARC_TX_ADB_Offset;
} eARCTX_u8Data;
#define eARC_Event_Change(a, b, c) do {(a) = ((a) & (~(b))) | ((c) & (b)); ; } while (0)

#define eARC_Event_Null (0x00)
#define eARC_ARC_Start (0x01)
#define eARC_eARC_CapChg (0x02)
#define eARC_eARC_EdidOk (0x04)
#define eARC_eARC_FwHPD (0x08)

#if (En_eARCRX == iTE_TRUE)
extern eARCRX_u8Data g_u8RX;
#endif

#if (IS_IT663XX == IT66321)
extern iTE_u8 g_u8AdoOut;
extern sTxInfo *g_pstCurTxInfo;
extern void IT66321_SwitchPort(iTE_u8 u8RxPort);
#endif


extern iTE_u16 g_u16CurTxFun;
extern iTE_u8 g_u8AudCodec;


#if 0
void IT662x_eARC_ini(void);
void IT662x_eARC_Main(void);
#endif

/********************************************/
/*eARC RX*/
/********************************************/

iTE_u8 IT662x_eARC_RX_Ini(void);
void IT662x_eARC_RX_Rst(void);
void IT662x_eARC_RX_Irq(void);
void IT662x_eARC_RX_ShowAdoInfo(void);
void IT662x_eARC_RX_ShowAdoChSts(void);
void IT662x_eARC_RX_ShowAclk(void);
void IT662x_eARC_RX_ShowBclk(void);
void IT662x_eARC_RX_SetRxCapDataStruct(iTE_u8 const *ptr);
void IT662x_eARC_RX_CheAdoInfoFrame(void);
void IT662x_eARC_RX_CalRclk(void);
void IT662x_eARC_RX_Bank(iTE_u8 bankno);
void IT662x_eARC_RX_CEC_Init(void);
void IT662x_eARC_RX_ForcePDIV(void);
void IT662x_CEC_ARC_RX_ONOFF(iTE_u8 u8sts);
void IT662x_eARC_RX_SetLat(void);
void eARC_RX_VarInit(void);
#ifdef FWI2C
void IT662x_eARC_RX_Read_EDID(void);
void SET_SCL_R_X(char x);
void i2c_clock_delay(void);
void __set_sda(char x);
void __set_scl(char x);
unsigned char __get_scl(void);
unsigned char __get_sda(void);
void i2c_start_delay(void);
iTE_u8 i2c_8051_wait_arbit(void);
void i2c_8051_start(void);
void i2c_8051_write(iTE_u8 Value);
iTE_u8 i2c_8051_wait_ack(void);
iTE_u8 i2c_8051_read(iTE_u8 ack_bit);
void i2c_8051_end(void);
iTE_u8 i2c_write_byte_v1(iTE_u8 address, iTE_u8 offset, iTE_u8 byteno,
			 iTE_u8 *p_data);
iTE_u8 i2c_read_byte_v1(iTE_u8 address, iTE_u8 offset, iTE_u8 byteno,
			iTE_u8 *p_data);
#endif
/********************************************/
/*eARC TX*/
/********************************************/

void IT662x_eARC_TX_Bank(iTE_u8 bankno);
void IT662x_eARC_TX_CalRclk(void);
void IT662x_eARC_TX_Irq(void);
void IT662x_eARC_TX_VarInit(void);
void IT662x_eARC_TX_Rst(void);
void IT662x_eARC_TX_AudInfo(void);
void IT662x_eARC_TX_ShowAclk(void);
void IT662x_eARC_TX_ShowBclk(void);
void IT662x_eARC_TX_CEC_Init(void);
void IT662x_eARC_TX_HDMI_HPD_Feedback(iTE_u8 u8sts);
void IT662x_eARC_TX_Audio_ChSts_CallBack(iTE_u8 *ptr, iTE_u8 audstschg);
void IT662x_eARC_TX_ForcePDIV(void);
void IT662x_eARC_TX_SetAdoFmt(void);
void IT662x_eARC_TX_ParseRxCap(void);
void IT662x_eARC_TX_ParseAdoDataBlk(iTE_u8 sad_no, iTE_u8 *cea_sad);
void IT662x_eARC_TX_CmdcCmdWrite(iTE_u8 devid, iTE_u8 offset, iTE_u8 wrdata);
void IT662x_eARC_TX_ChkRxCap(void);
void IT662x_eARC_TX_SetAdoInfoFrame(void);
void IT662x_eARC_TX_AudioMute(iTE_u8 mute);
void IT662x_eARC_TX_DrvRst(iTE_u8 on);
void IT662x_eARC_TX_2to8ch(void);
void IT662x_eARC_TX_ArrayInit(void);
void IT662x_eARC_TX_OptionSet(iTE_u8 multich, iTE_u8 layoutB, iTE_u8 ch);
void IT662x_eARC_TX_SetLat(iTE_u8 latency);
iTE_u16 IT662x_eARC_TX_EDID_CapChg_CallBack(iTE_u8 *ADB, iTE_u8 *Spk_AdbPtr,
					    iTE_u8 *Spk_LdbPtr);
iTE_u8 IT662x_eARC_TX_CmdcCmdRead(iTE_u8 devid, iTE_u8 offset);
iTE_u8 IT662x_eARC_TX_CmdcBusWait(void);
iTE_u8 IT662x_eARC_TX_ReadRxCap(void);
iTE_u8 IT662x_eARC_TX_Ini(void);
void IT662x_CEC_ARC_TX_ONOFF(iTE_u8 u8sts);
iTE_u8 IT662x_CEC_ARC_State(void);
iTE_u8 ADB_Compare(iTE_u8 *ptrA, iTE_u8 *ptrB, iTE_u8 len);
iTE_u8 IT662x_eARC_TX_State(void);

/********************************************/

void entxdbgfifo(void);
void enrxdbgfifo(void);
void distxdbgfifo(void);
void disrxdbgfifo(void);
void show_txdbg_fifo(void);
void show_rxdbg_fifo(void);
void parse_dbg_fifo(int data0, int data1);
void setup_txpkt1(void);
void check_rxpkt1(void);
void setup_txpkt2(void);
void check_rxpkt2(void);
void setup_txpkt3(void);
void check_rxpkt3(void);
/********************************************/

void showhelp(void);
iTE_u8 IT662x_eARCTX_EDID_Block1_Parse(iTE_u8 *u8ptr);
void IT662x_eARCTX_EDID_Block1_Compose(iTE_u8 *u8ptrS, iTE_u8 offset,
				       iTE_u8 *u8ptrT);
void IT662x_eARCTX_EDID_Change(void);
void dump_rxcap(iTE_u16 offset, iTE_u8 rddata, iTE_u8 *rxcapfile);
iTE_u8 get_rxcap_data(iTE_u8 offset, iTE_u8 *rddata, iTE_u8 *rxcapfile);
iTE_u8 hex2int(char *data);
iTE_u8 str2int(char *data);
void set_fpga_fmt(void);
void eARCTX_pwron(void);
void eARCTX_pwrdn(void);
void eARCRX_pwron(void);
void eARCRX_pwrdn(void);

#if (IS_IT663XX != IT66321)
#define mDelay1ms(x) msleep(x)
#define eARC_Txbwr(u8Offset, u8Count, pu8Data)                                 \
	iTE_I2C_WriteBurst(eARCTxAddr, u8Offset, u8Count, pu8Data)
#define eARC_Txbrd(u8Offset, u8Count, pu8Data)                                 \
	iTE_I2C_ReadBurst(eARCTxAddr, u8Offset, u8Count, pu8Data)
#define eARC_Txset(u8Offset, u8InvMask, u8Data)                                \
	iTE_I2C_SetByte(eARCTxAddr, u8Offset, u8InvMask, u8Data)
#define eARC_Txwr(u8Offset, u8Data)                                            \
	iTE_I2C_WriteByte(eARCTxAddr, u8Offset, u8Data)
#define eARC_Txrd(u8Offset) iTE_I2C_ReadByte(eARCTxAddr, u8Offset)
#define eARC_Txsettab(pu8TabAdr, u8Count)                                      \
	iTE_I2C_SetTable(eARCTxAddr, pu8TabAdr, u8Count)

#define eARC_Rxbwr(u8Offset, u8Count, pu8Data)                                 \
	iTE_I2C_WriteBurst(eARCRxAddr, u8Offset, u8Count, pu8Data)
#define eARC_Rxbrd(u8Offset, u8Count, pu8Data)                                 \
	iTE_I2C_ReadBurst(eARCRxAddr, u8Offset, u8Count, pu8Data)
#define eARC_Rxset(u8Offset, u8InvMask, u8Data)                                \
	iTE_I2C_SetByte(eARCRxAddr, u8Offset, u8InvMask, u8Data)
#define eARC_Rxwr(u8Offset, u8Data)                                            \
	iTE_I2C_WriteByte(eARCRxAddr, u8Offset, u8Data)
#define eARC_Rxrd(u8Offset) iTE_I2C_ReadByte(eARCRxAddr, u8Offset)
#define eARC_Rxsettab(pu8TabAdr, u8Count)                                      \
	iTE_I2C_SetTable(eARCRxAddr, pu8TabAdr, u8Count)
#define RxCapwr(u8Offset, u8Data) iTE_I2C_WriteByte(RxCapAddr, u8Offset, u8Data)
/*add by myself for debug*/
#define RxCaprd(u8Offset, u8Data) iTE_I2C_ReadByte(eARCRxAddr, u8Offset)

#else
/*Define your timer delay function*/
void timer(iTE_u32 x);
#define mDelay1ms(x) timer(x)

/*Define your I2C driver's API*/
void eI2C_API1(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Count, iTE_pu8 pu8Data);
void eI2C_API2(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Count, iTE_pu8 pu8Data);
void eI2C_API3(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8InvMask, iTE_u8 u8Data);
void eI2C_API4(iTE_u8 addr, iTE_u8 u8Offset, iTE_u8 u8Data);
iTE_u8 eI2C_API5(iTE_u8 addr, iTE_u8 u8Offset);
void eI2C_API6(iTE_u8 addr, iTE_u8 *u8Offset, iTE_u8 u8Data);

#define eARC_Txbwr(u8Offset, u8Count, pu8Data)                                 \
	eI2C_API1(eARCTxAddr, u8Offset, u8Count, pu8Data)
#define eARC_Txbrd(u8Offset, u8Count, pu8Data)                                 \
	eI2C_API2(eARCTxAddr, u8Offset, u8Count, pu8Data)
#define eARC_Txset(u8Offset, u8InvMask, u8Data)                                \
	eI2C_API3(eARCTxAddr, u8Offset, u8InvMask, u8Data)
#define eARC_Txwr(u8Offset, u8Data) eI2C_API4(eARCTxAddr, u8Offset, u8Data)
#define eARC_Txrd(u8Offset) eI2C_API5(eARCTxAddr, u8Offset)
#define eARC_Txsettab(pu8TabAdr, u8Count)                                      \
	eI2C_API6(eARCTxAddr, pu8TabAdr, u8Count)

#define eARC_Rxbwr(u8Offset, u8Count, pu8Data)                                 \
	eI2C_API1(eARCRxAddr, u8Offset, u8Count, pu8Data)
#define eARC_Rxbrd(u8Offset, u8Count, pu8Data)                                 \
	eI2C_API2(eARCRxAddr, u8Offset, u8Count, pu8Data)
#define eARC_Rxset(u8Offset, u8InvMask, u8Data)                                \
	eI2C_API3(eARCRxAddr, u8Offset, u8InvMask, u8Data)
#define eARC_Rxwr(u8Offset, u8Data) eI2C_API4(eARCRxAddr, u8Offset, u8Data)
#define eARC_Rxrd(u8Offset) eI2C_API5(eARCRxAddr, u8Offset)
#define eARC_Rxsettab(pu8TabAdr, u8Count)                                      \
	eI2C_API6(eARCRxAddr, pu8TabAdr, u8Count)
#define RxCapwr(u8Offset, u8Data) eI2C_API4(RxCapAddr, u8Offset, u8Data)

#endif

#endif /* _eARC_H_*/

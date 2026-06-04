/*****************************************
 *  Copyright (C) 2009-2019
 *  ITE Tech. Inc. All Rights Reserved
 *  Proprietary and Confidential
 *****************************************
 *   @file   <IT662X_CecSys_eARC_RX.c>
 *   @author Hojim.Tseng@ite.com.tw
 *   @date   2019/01/16
 *   @fileversion: ITE_eARC_RX_0.70
 */
#include "IT662X_Cec.h"
#include "../inc/hdmi_drv.h"

#if (IS_IT663XX == IT66321)
#include "../IT6634_Source/IT6634_Switch.h"
#include "../IT6634_source/IT6634_HdmiTx.h"
#endif

#if ((En_eARCRX == iTE_TRUE) || (En_eARCTX == iTE_TRUE))

#define _CEC_LOOKUP_TAB_
#include "IT662X_CecSys.h"

/* EN_IT6601_ARC:*/
/* 1: Enable ARC function ( IT6601 is required )*/
/* 0: Disable ARC function*/
#ifndef EN_IT6601_ARC
#define EN_IT6601_ARC 0
#endif

#if (En_eARCRX == iTE_TRUE)
#define EN_IT662x_CEC_ARC iTE_TRUE
/*extern eARCRX_u8Data g_u8RX;*/
#endif

#if EN_IT6601_ARC
#define ARC_ADR                                                                \
	0xC8 /*(PCADR1 ,PCADR0) "00" : 0x9C ,"01":0x9E ,"10":0xAC ,"11":0xAE*/
#define it6601_write_byte(offset, pu8Data)                                     \
	i2c_write_byte(ARC_ADR, offset, 1, pu8Data, 1)

void IT6601_ARCRxInitiation(void)
{
	iTE_u8 u8Temp = 0x0A;

	if (it6601_write_byte(0x25, &u8Temp) == 0)
		iTE_MsgE(("IT6601 I2C write fail\n"));

	/*iTE_MsgE(("ARC init\n");*/
}

void IT6601_ARCRxTermination(void)
{
	iTE_u8 u8Temp = 0x00;

	if (it6601_write_byte(0x25, &u8Temp) == 0)
		iTE_MsgE(("IT6601 I2C write fail\n"));

	/*iTE_MsgE(("ARC termination\n");*/
}
#endif

/* for check Invalid CEC Cmd*/
#ifdef _CEC_LOOKUP_TAB_
#if ((En_eARCRX == iTE_TRUE) || (En_eARCTX == iTE_TRUE))

static iTE_u16 _CODE u16PaMask[6] = {0x0000, 0xF000, 0xFF00,
				     0xFFF0, 0xFFFF, 0x0000};

static iTE_u8 _CODE u8SwitchLA[0x10] = {
	DEVICE_ID_FREEUSE,    /* DEVICE_ID_TV*/
	DEVICE_ID_RECORDING2, /* DEVICE_ID_RECORDING1*/
	DEVICE_ID_RECORDING3, /* DEVICE_ID_RECORDING2*/
	DEVICE_ID_TUNER2,     /* DEVICE_ID_TUNER1*/
	DEVICE_ID_PLAYBACK2,  /* DEVICE_ID_PLAYBACK1*/
	DEVICE_ID_FREEUSE,    /* DEVICE_ID_AUDIO*/
	DEVICE_ID_TUNER3,     /* DEVICE_ID_TUNER2*/
	DEVICE_ID_TUNER4,     /* DEVICE_ID_TUNER3*/
	DEVICE_ID_PLAYBACK3,  /* DEVICE_ID_PLAYBACK2*/
	DEVICE_ID_RESERVED1,  /* DEVICE_ID_RECORDING3*/
	DEVICE_ID_RESERVED1,  /* DEVICE_ID_TUNER4*/
	DEVICE_ID_RESERVED1,  /* DEVICE_ID_PLAYBACK3*/
	DEVICE_ID_RESERVED2,  /* DEVICE_ID_RESERVED1*/
	DEVICE_ID_BROADCAST,  /* DEVICE_ID_RESERVED2*/
	DEVICE_ID_BROADCAST,  /* DEVICE_ID_FREEUSE*/
	DEVICE_ID_BROADCAST   /* DEVICE_ID_BROADCAST*/
};

static _CODE CECcmdFormat LookUpTable_CEC[] =
	/*static _CODE  LookUpTable_CEC[][3]=*/
	{
		/* One Touch Play Feature*/
		{eActiveSource, eBroadcast | 4, eOTPFeature,
		 "Active Source"}, // Header + [0x82] + [Physical Address]
		{eImageViewOn, eDirectly | 2, eOTPFeature,
		 "Image View On"}, // Header + [0x04]
		{eTextViewOn, eDirectly | 2, eOTPFeature,
		 "Text View On"}, // Header + [0x0D]

		/* Routing Control Feature*/
		{eInactiveSource, eDirectly | 4, eRoutingCtlFeature,
		 "Inactive Source"}, // Header + [0x9D]
		{eRequestActiveSource, eBroadcast | 2, eRoutingCtlFeature,
		 "Request Active Source"}, // Header + [0x85]
		{eRoutingChange, eBroadcast | 6, eRoutingCtlFeature,
		 "Routing Change"}, // Header + [0x80] + [Org Address] + [New
				    // Address]
		{eRoutingInformation, eBroadcast | 4, eRoutingCtlFeature,
		 "Routing Information"}, // Header + [0x80] + [Org Address] +
					 // [New Address]
		{eSetStreamPath, eBroadcast | 4, eRoutingCtlFeature,
		 "Set Stream Path"}, // Header + [0x86] + [Physical Address]

		{eStandBy, eBoth | 2, eStandByFeature,
		 "Standby"}, // Header + [0x36]

		/* One Touch Record Feature*/
		{eRecordOff, eDirectly | 2, eOTRFeature, "Record OFF"},
		{eRecordOn, eDirectly | 3, eOTRFeature, "Record ON"},
		{eRecordStatus, eDirectly | 3, eOTRFeature, "Record Status"},
		{eRecordTVScreen, eDirectly | 2, eOTRFeature,
		 "Record TV Screen"},

		/* Timer Programming Feature*/
		{eClearAnalogueTimer, eDirectly | 13, eTimerProgFeature,
		 "Clear Analogue Timer"},
		{eClearDigitalTimer, eDirectly | 16, eTimerProgFeature,
		 "Clear Digital Timer"},
		{eClearExternalTimer, eDirectly | 13, eTimerProgFeature,
		 "Clear External Timer"},
		{eSetAnalogueTimer, eDirectly | 13, eTimerProgFeature,
		 "Set Analogue Timer"},
		{eSetDigitalTimer, eDirectly | 16, eTimerProgFeature,
		 "Set Digital Timer"},
		{eSetExternalTimer, eDirectly | 13, eTimerProgFeature,
		 "Set External Timer"},
		{eSetTimerProgramTitle, eDirectly | 2, eTimerProgFeature,
		 "Set Timer Program Title"},
		{eTimerClearedStatus, eDirectly | 3, eTimerProgFeature,
		 "Timer Cleared Status"},
		{eTimerStatus, eDirectly | 3, eTimerProgFeature,
		 "Timer Status"},

		/* System Information Feature*/
		{eCECVersioin, eDirectly | 3, eSysInfoFeature,
		 "CEC Version"}, /* Header + [0x82] + [CEC Version]*/
		{eGetCECVersion, eDirectly | 2, eSysInfoFeature,
		 "Get CEC Version"}, /* Header + [0x9F]*/
		{eGivePhysicalAddress, eDirectly | 2, eSysInfoFeature,
		 "Give Physical Address"}, /* Header + [0x83]*/
		{eGetMenuLanguage, eDirectly | 2, eSysInfoFeature,
		 "Get Menu Language"}, /* Header + [0x91]*/
		{eReportPhysicalAddress, eBroadcast | 5, eSysInfoFeature,
		 "Report Physical Address"}, /* Header + [0x84] + [Physical*/
						/*Address]+ [Device Type]*/
		{eSetMenuLanguage, eBroadcast | 5, eSysInfoFeature,
		 "Set Menu Language"}, /* Header + [0x32] + [Language]*/
		{eReportFeatures, eBroadcast | 6, eSysInfoFeature,
		 "Report Features"},
		{eGiveFeatures, eDirectly | 2, eSysInfoFeature,
		 "Give Features"},

		/* Deck Control Feature*/
		{eDeckControl, eDirectly | 3, eDeckCtrlFeature,
		 "Deck Control"}, /* Header + [0x41] + [Play Mode]*/
		{eDeckStatus, eDirectly | 3, eDeckCtrlFeature,
		 "Deck Status"}, /* Header + [0x41] + [Play Mode]*/
		{eGiveDeckStatus, eDirectly | 3, eDeckCtrlFeature,
		 "Give Deck Status"}, /* Header + [0x41] + [Play Mode]*/
		{ePlay, eDirectly | 3, eDeckCtrlFeature,
		 "Play"}, /* Header + [0x41] + [Play Mode]*/

		/* Tuner Control Featuer*/
		{eGiveTunerDeviceStatus, eDirectly | 3, eTunerCtrlFeature,
		 "Give Tuner Device Status"},
		{eSelectAnalogueService, eDirectly | 6, eTunerCtrlFeature,
		 "Select Analogue Service"},
		{eSelectDigitalService, eDirectly | 9, eTunerCtrlFeature,
		 "Select Digital Service"},
		{eTunerDeviceStatus, eDirectly | 7, eTunerCtrlFeature,
		 "Tuner Device Status"},
		{eTunerStepDecrement, eDirectly | 2, eTunerCtrlFeature,
		 "Tuner Step Decrement"},
		{eTunerStepIncrement, eDirectly | 2, eTunerCtrlFeature,
		 "Tuner Step Increment"},

		/* Vendor Specific Commands Feature*/
		{eDeviceVendorID, eBroadcast | 5, eVenSpecCmdFeature,
		 "Device Vendor ID"}, /* Header + [0x87] + [Vendor ID]*/
		{eGiveDeviceVendorID, eDirectly | 2, eVenSpecCmdFeature,
		 "Give Device Vendor ID"}, /* Header + [0x8C]*/
		{eVendorCommand, eDirectly | 2, eVenSpecCmdFeature,
		 "Vendor Command"}, /* Header + [0x89] + [Vendor Specific*/
				       /*Data]*/
		{eVendorCommandWithID, eBoth | 5, eVenSpecCmdFeature,
		 "Vendor Command With ID"}, /* Header + [0xA0] + [Vendor ID]+*/
					       /*[Vendor Specific Data]*/
		{eRemoteButtonDown, eBoth | 2, eVenSpecCmdFeature,
		 "Vendor Remote Button Down"}, /* Header + [0x8A] + [Vendor*/
						  /*Specific RC code]*/
		{eRemoteButtonUp, eBoth | 2, eVenSpecCmdFeature,
		 "Vendor Remote Button Up"}, /* Header + [0x8B] + [Vendor*/
						/*Specific RC code]*/

		/* OSD Display Feature*/
		{eSetOSDString, eDirectly | 3, eOSDDisplayFeature,
		 "Set OSD String"},

		/* Device OSD Transfer Feature*/
		{eGiveOSDName, eDirectly | 2, eDevOSDTransferFeature,
		 "Give OSD Name"},
		{eSetOSDName, eDirectly | 2, eDevOSDTransferFeature,
		 "Set OSD Name"},

		/* Device Menu Contro Feature*/
		{eMenuRequest, eDirectly | 3, eDevMenuCtrlFeature,
		 "Menu Request"},
		{eMenuStatus, eDirectly | 3, eDevMenuCtrlFeature,
		 "Menu Status"},

		/* Remote Control PassThrough Feature*/
		{eUserControlPressed, eDirectly | 3,
		 eRemoteCtlPassthroughFeature,
		 "User Control Pressed"}, /* Header + [0x44] + [UI command]*/
		{eUserControlReleased, eDirectly | 2,
		 eRemoteCtlPassthroughFeature,
		 "User Control Released"}, /* Header + [0x45]*/

		/* Power Status Feature*/
		{eGiveDevicePowerStatus, eDirectly | 2, ePowerStatusFeature,
		 "Give Device Power Status"}, /* Header + [0x8F]*/
		{eReportPowerStatus, eDirectly | eBroadcast_2_0 | 3,
		 ePowerStatusFeature,
		 "Report Power Status"}, /* Header + [0x90] + [Power Status]*/

		/* General Protocol Messages*/
		{eFeatureAbort, eDirectly | 4, eGeneralProtocolMessage,
		 "Feature Abort"}, /* [Header] + [0x00] + [Feature OpCode] +*/
				      /*[Abort Reason]*/
		{eAbort, eDirectly | 2, eGeneralProtocolMessage,
		 "Abort"}, /* [Header] + [0xFF]*/
		/* System Audio Control Feature*/
		{eGiveAudioStatus, eDirectly | 2, eSysAudioCtlFeature,
		 "Give Audio Status"}, /* Header + [0x71]*/
		{eGiveSystemAudioModeStatus, eDirectly | 2, eSysAudioCtlFeature,
		 "Give System Audio Mode Status"}, /* Header + [0x7D]*/
		{eReportAudioStatus, eDirectly | 3, eSysAudioCtlFeature,
		 "Report Audio Status"}, /* Header + [0x7A] + [Audio Status]*/
		{eReportShortAudioDescriptor, eDirectly | 3,
		 eSysAudioCtlFeature,
		 "Report Audio Descriptor"}, /* Header + [0xA3] + [Audio*/
						/*Descriptor]*/
		{eRequestShortAudioDescriptor, eDirectly | 3,
		 eSysAudioCtlFeature,
		 "Request Short Audio Descriptor"},
/* Header + [0xA4] + [Audio*/ /*Format ID and Code]*/
		{eSetSystemAudioMode, eBoth | 3, eSysAudioCtlFeature,
		 "Set System Audio Mode"}, /* Header + [0x72] + [System Audio*/
					      /*Status]*/
/*{eSystemAudioModeRequest, eDirectly	|	2,*/
/*eSysAudioCtlFeature, "System Audio Mode Request"},*/
/* Header + [0x70] + [Physical Address]*/
		{eSystemAudioModeRequest, eDirectly | 4, eSysAudioCtlFeature,
		 "System Audio Mode Request"}, /* Header + [0x70] + [Physical*/
						  /*Address]*/
		{eSystemAudioModeStatus, eDirectly | 3,
		 eSysAudioCtlFeature, "System Audio Mode Status"},
		/* Header +*/
		/*[0x7E] +*/
		/* [System*/
		/* Audio*/
		/* Status]*/

		/* Audio Rate Control Feature*/
		{eSetAudioRate, eDirectly | 3, eAudioRateCtrlFeature,
		 "Set Audio Rate"},

		/* ACR*/
		{eInitiateARC, eDirectly | 2, eARCFeature,
		 "Initiate ARC"}, /* Header + [0xC0]*/
		{eReportARCInitiated, eDirectly | 2, eARCFeature,
		 "Report ARC Initiated"}, /* Header + [0xC1]*/
		{eReportARCTerminated, eDirectly | 2, eARCFeature,
		 "Report ARC Terminated"}, /* Header + [0xC2]*/
		{eRequestARCInitiation, eDirectly | 2, eARCFeature,
		 "Request ARC Initiation"}, /* Header + [0xC3]*/
		{eRequestARCTermination, eDirectly | 2, eARCFeature,
		 "Request ARC Termination"}, /* Header + [0xC4]*/
		{eTerminateARC, eDirectly | 2, eARCFeature,
		 "Termiate ARC"}, /* Header + [0xC5]*/

		/* Dynamic Auto Lipsync Featuer*/
		{eRequestCurrentLatency, eBroadcast | 4, eDynAutoLipsyncFeature,
		 "Request Current Latency"},
		{eReportCurrentLatency, eBroadcast | 7, eDynAutoLipsyncFeature,
		 "Report Current Latency"},

		/* Capability Discovery and Control Feature (HEC)*/
		{eCDCMessage, eDirectly | 2, eHECFeature,
		 "CDC Message"}, /* Header + [0xF8]*/

		/* Polling Message*/
		/*ePolling Message,*/
		/*	{eCDC+1,	eBoth,	0,	eNoneFeature},	*/
		/*end of	lookup table !!!*/
};

#define SizeOfLookUpTable_CEC (sizeof(LookUpTable_CEC) / sizeof(CECcmdFormat))
#endif
#endif


CecSys stCecSta;
CecSys *pstCecSta = &stCecSta;
iTE_u8 u8CecSysCmd[20];
iTE_u8 g_u8TxCurHeader, g_u8TxCurOpCode;
iTE_u8 u8CecList[16][3] = {/* PaAB, PaCD, CecType*/
			   {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
			   {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
			   {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
			   {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
			   {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
			   {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
			   {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
			   {0xFF, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF} };

#define CEC_TX_QUEUE_SIZE (0x20)
#define CEC_RX_QUEUE_SIZE (0x40)

iTE_u8 u8TxQueue[CEC_TX_QUEUE_SIZE];
iTE_u8 u8Rxqueue[CEC_RX_QUEUE_SIZE];

iTE_u8 g_u8AudioVolume;
iTE_u8 g_u8CecPwrSta = CEC_POWER_STATE_STANDBY;
iTE_u1 g_u1CecAudioSta = CEC_SYS_AUDIO_STATE_ON;
iTE_u1 g_bAudioMute = CEC_AUDIO_MUTE_OFF;
iTE_u8 g_u8TxInitiater = DEVICE_ID_BROADCAST;
iTE_u1 g_u1HDMIMode;

MenuStatus g_euDevStauts = eDeactived;
/*extern iTE_u16 g_u16CurTxFun;*/
/*extern iTE_u8 g_u8AudCodec;*/

iTE_u8 g_u8VendorId[3] = {0x00, 0xE0, 0x36};
iTE_u8 g_u8Langeuage[3] = {'c', 'h', 'i'};
iTE_u8 g_u8OSDName[14] = "Sound bar";
iTE_u8 g_u8OSDString[13] = "Hello";
stCecQ stTxQ, stRxQ;

/********************************************/
iTE_ps8 _CODE_3K pu8RcpString[0x80] = {"Select/OK",
				       "Up",
				       "Down",
				       "Left",
				       "Right",
				       "Right-Up",
				       "Right-Down",
				       "Left-Up",
				       "Left-Down",
				       "Root Menu",
				       "Setup Menu",
				       "Contents Menu",
				       "Favorite Menu",
				       "Back",
				       NULL,
				       NULL,
				       "Media Top Menu",
				       "Media Contextsensitive Menu",
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       "Number Entry Mode",
				       "Number 11",
				       "Number 12",
				       "Numeric 0/10",
				       "Numeric 1",
				       "Numeric 2",
				       "Numeric 3",
				       "Numeric 4",
				       "Numeric 5",
				       "Numeric 6",
				       "Numeric 7",
				       "Numeric 8",
				       "Numeric 9",
				       "Dot",
				       "Enter",
				       "Clear",
				       NULL,
				       NULL,
				       "Next Favorite",
				       "Channel Up",
				       "Channel Down",
				       "Previous Channel",
				       "Sound Select",
				       "Input Select",
				       "Display Information",
				       "Help",
				       "Page Up",
				       "Page Down",
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       "Power",
				       "Volume Up",
				       "Volume Down",
				       "Mute",
				       "Play",
				       "Stop",
				       "Pause",
				       "Record",
				       "Rewind",
				       "Fast Forward",
				       "Eject",
				       "Skip Forward",
				       "Skip Backward",
				       "Stop-Record",
				       "Pause-Record",
				       NULL,
				       "Angle",
				       "Subpicture",
				       "Video on Demand",
				       "Electronic Program Guide",
				       "Timer Programming",
				       "Initial Configuration",
				       "Select Broadcast Type",
				       "Select Sound Presentation",
				       "Audio Descritption",
				       "Internet",
				       "3D mode",
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       "Play Function",
				       "Pause Play Function",
				       "Record Function",
				       "Pause Record Function",
				       "Stop Function",
				       "Mute Function",
				       "Restore Volume Funation",
				       "Tune Function",
				       "Select Media Function",
				       "Select A/V Input Function",
				       "Select Audio Input Function",
				       "Power Toggle Function",
				       "Power Off Function",
				       "Power On Function",
				       NULL,
				       NULL,
				       NULL,
				       "F1 (Blue)",
				       "F2 (Red)",
				       "F3 (Green)",
				       "F4 (Yellow)",
				       "F5",
				       "Data",
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL};
iTE_u1 CecSys_PushToQueue(iTE_u8 *pu8Cmd, iTE_u8 u8Len, stCecQ *pstQ)
{
	iTE_u8 u8RPtr = pstQ->u8RPtr;
	iTE_u8 u8WPtr = pstQ->u8WPtr;
	iTE_pu8 pu8Queue = pstQ->pu8Buf;
	iTE_u8 u8QMaxSize = pstQ->u8MaxSize;
	iTE_u8 u8FreeSize;

	if (u8WPtr < u8RPtr)
		u8FreeSize = u8RPtr - u8WPtr;
	else
		u8FreeSize = u8QMaxSize - u8WPtr + u8RPtr;


	if (u8Len) {
		if (u8FreeSize < (u8Len + 1)) {
			iTE_MsgE(("[Cec] TX Queue Full, %X, %X, %X\n", u8WPtr,
				  u8RPtr, u8Len));
			pstQ->u8RPtr = 0;
			pstQ->u8WPtr = 0;
			return iTE_FALSE;
		}

		pu8Queue[u8WPtr++ & (u8QMaxSize - 1)] = u8Len;
		while (u8Len--)
			pu8Queue[u8WPtr++ & (u8QMaxSize - 1)] = *pu8Cmd++;

		pstQ->u8WPtr = u8WPtr & (u8QMaxSize - 1);
#if 0
		iTE_Cec_Msg(
			("-CecSys_PushToQueue wPtr=%02X, rPtr=%02X, Len=%X\n",
			 u8WPtr, u8RPtr, u8Len));
#endif
		return iTE_TRUE;
	}
	return iTE_FALSE;
}
void Cec_ReportAudioStatus(void)
{
	iTE_Cec_Msg(("Mute:%s, Volume:%d\r\n",
		     (g_bAudioMute) ? "Mute" : "UnMute", g_u8AudioVolume));
}
/********************************************/
void Cec_SwitchPort(iTE_u8 *u8CecSysCmd)
{

	iTE_u16 u16Temp, u16CecPa;

	u16Temp = pstCecSta->u8PaAB;
	u16Temp <<= 8;
	u16Temp += pstCecSta->u8PaCD;

	u16CecPa = u8CecSysCmd[2];
	u16CecPa <<= 8;
	u16CecPa += u8CecSysCmd[3];

	if (u16CecPa > u16Temp) {
		u16CecPa -= u16Temp;
		if (!(u16CecPa & u16PaMask[pstCecSta->u8Stage])) {
			iTE_u8 u8Temp;

			u16CecPa &= u16PaMask[pstCecSta->u8Stage + 1];
			u8Temp = u16CecPa >> ((3 - pstCecSta->u8Stage) * 4);

#if (IS_IT663XX == IT66321)
			IT66321_SwitchPort(u8Temp - 1);
#endif
			pstCecSta->u8SelPort = u8Temp - 1;
		}
	}
}
/********************************************/
void Cec_OTPFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	case eActiveSource:
		Cec_SwitchPort(u8CecSysCmd);
		break;
	}
}
/********************************************/
void Cec_RoutingCtlFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	case eRequestActiveSource:
		break;
	}
}
/********************************************/
void Cec_StandByFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	case eStandBy:
		// enter standby
		g_u8CecPwrSta = CEC_POWER_STATE_STANDBY;

#if (IS_IT663XX == IT66321)
		IT66321_SwitchPort(IN_RXn);
#endif
		break;
	}
}
/********************************************/
void Cec_OTRFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eRecordOff:
		// ToDo
		break;
	case eRecordOn:
		// ToDo
		break;
	case eRecordStatus:
		// ToDo
		break;
	case eRecordTVScreen:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eFeatureAbort;
		u8CecSysCmd[2] = eRecordTVScreen;
		u8CecSysCmd[3] = CEC_ABORT_REASON_CAN_NOT_PROVIDE_SOURCE;
		CecSys_PushToQueue(u8CecSysCmd, 4, &stTxQ);
		break;
	}
}
/********************************************/
void Cec_TimerProgFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	}
}
/********************************************/
void Cec_SysInfoFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eCECVersioin:
		// ToDo
		break;
	case eGetCECVersion:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eCECVersioin;
		u8CecSysCmd[2] = CEC_VERISION_2_0;
		CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
		break;
	case eGivePhysicalAddress:
		g_u8TxInitiater = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
		u8CecSysCmd[1] = eReportPhysicalAddress;
		u8CecSysCmd[2] = pstCecSta->u8PaAB;
		u8CecSysCmd[3] = pstCecSta->u8PaCD;
		u8CecSysCmd[4] = pstCecSta->u8Type;
		CecSys_PushToQueue(u8CecSysCmd, 5, &stTxQ);

		if (g_u8TxInitiater == DEVICE_ID_TV) {
			CecSys_TxRequestCurrentLatency(pstCecSta);

			if (g_u8CecPwrSta == CEC_POWER_STATE_STANDBY)
				CecSys_TxPowerOn(pstCecSta);

		}
		break;
	case eGetMenuLanguage:
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
		u8CecSysCmd[1] = eSetMenuLanguage;
		u8CecSysCmd[2] = g_u8Langeuage[0];
		u8CecSysCmd[3] = g_u8Langeuage[1];
		u8CecSysCmd[4] = g_u8Langeuage[2];
		CecSys_PushToQueue(u8CecSysCmd, 5, &stTxQ);
		break;
	case eReportPhysicalAddress:
		u8CecList[u8CecSysCmd[0] >> 4][0] = u8CecSysCmd[2];
		u8CecList[u8CecSysCmd[0] >> 4][1] = u8CecSysCmd[3];
		u8CecList[u8CecSysCmd[0] >> 4][2] = u8CecSysCmd[4];
		break;
	case eSetMenuLanguage:
		/* ToDo*/
		break;
	case eReportFeatures:
		/* ToDo*/
		break;
	case eGiveFeatures:
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
		u8CecSysCmd[1] = eReportFeatures;
		u8CecSysCmd[2] = CEC_VERISION_2_0;
		break;
	}
}
/********************************************/
void Cec_DeckCtrlFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	}
}
/********************************************/
void Cec_TunerCtrlFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	}
}
/********************************************/
void Cec_VenSpecCmdFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
		iTE_u8 u8Temp;
	case eGiveDeviceVendorID:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
		u8CecSysCmd[1] = eDeviceVendorID;
		// type vendor ID here
		u8CecSysCmd[2] = g_u8VendorId[0];
		u8CecSysCmd[3] = g_u8VendorId[1];
		u8CecSysCmd[4] = g_u8VendorId[2];
		CecSys_PushToQueue(u8CecSysCmd, 5, &stTxQ);
		break;
	}
}
/********************************************/
void Cec_OSDDisplayFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	}
}
/********************************************/
void Cec_DevOSDTransferFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eGiveOSDName:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eSetOSDName;
		u8CecSysCmd[2] = g_u8OSDName[0];
		for (u8Temp = 1; u8Temp < 13; u8Temp++) {
			if (g_u8OSDName[u8Temp] != '\0')
				u8CecSysCmd[2 + u8Temp] = g_u8OSDName[u8Temp];
			else
				break;

		}
		CecSys_PushToQueue(u8CecSysCmd, 2 + u8Temp, &stTxQ);
		break;
	case eSetOSDName:
		break;
	}
}
/********************************************/
void Cec_DevMenuCtrlFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eMenuRequest:
		switch (u8CecSysCmd[2]) {
		case eActive:
			/*enter Device Menu Active*/
			u8Temp = u8CecSysCmd[0] >> 4;
			u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
			u8CecSysCmd[1] = eMenuStatus;
			u8CecSysCmd[2] = eActived;
			CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);

			g_euDevStauts = eActived;
			break;
		case eDeactive:
			/*exit Device Menu Active*/
			u8Temp = u8CecSysCmd[0] >> 4;
			u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
			u8CecSysCmd[1] = eMenuStatus;
			u8CecSysCmd[2] = eDeactived;
			CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);

			g_euDevStauts = eDeactived;
			break;
		case eQuery:
			u8Temp = u8CecSysCmd[0] >> 4;
			u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
			u8CecSysCmd[1] = eMenuStatus;
			u8CecSysCmd[2] = g_euDevStauts; // device menu status
			CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
			break;
		}
		break;
	case eMenuStatus:
		break;
	}
}
/********************************************/
void Cec_RemoteCtlPassthroughFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eUserControlPressed:
		switch (u8CecSysCmd[2]) {
		case eVolumeUp:
			/* Volume Up Here*/
			g_u8AudioVolume++;
			if (g_u8AudioVolume > 100)
				g_u8AudioVolume = 100;

			Cec_ReportAudioStatus();
			u8Temp = u8CecSysCmd[0] >> 4;
			u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
			u8CecSysCmd[1] = eReportAudioStatus;
			u8CecSysCmd[2] =
				(((g_bAudioMute) ? 1 : 0) << 7) +
				g_u8AudioVolume; // Mute Status + Volume Status
			CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
			break;
		case eVolumeDown:
			/* volume down Here*/
			g_u8AudioVolume--;
			if (g_u8AudioVolume == 0xFF)
				g_u8AudioVolume = 0;

			Cec_ReportAudioStatus();
			u8Temp = u8CecSysCmd[0] >> 4;
			u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
			u8CecSysCmd[1] = eReportAudioStatus;
			u8CecSysCmd[2] =
				(((g_bAudioMute) ? 1 : 0) << 7) +
				g_u8AudioVolume; // Mute Status + Volume Status
			CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
			break;
		case eMute:
			/*volume Mute Here*/
			g_bAudioMute = !g_bAudioMute;
			Cec_ReportAudioStatus();
			u8Temp = u8CecSysCmd[0] >> 4;
			u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
			u8CecSysCmd[1] = eReportAudioStatus;
			u8CecSysCmd[2] =
				(((g_bAudioMute) ? 1 : 0) << 7) +
				g_u8AudioVolume; // Mute Status + Volume Status
			CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
			break;
		}
		break;
	case eUserControlReleased:
		break;
	}
}
/********************************************/
void Cec_PowerStatusFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eGiveDevicePowerStatus:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eReportPowerStatus;
		u8CecSysCmd[2] = g_u8CecPwrSta;
		CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
		break;
	case eReportPowerStatus:
		break;
	}
}
/********************************************/
void Cec_GeneralProtocolMessage(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eFeatureAbort:
		if (u8CecSysCmd[2] == eSetSystemAudioMode) {
			iTE_Cec_Msg(("SetSystemAudioMode =>Broadcast OFF\n"));
			u8CecSysCmd[0] =
				(pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
			u8CecSysCmd[1] = eSetSystemAudioMode;
			u8CecSysCmd[2] = CEC_SYS_AUDIO_STATE_OFF;
/*mute speaker here*/
#if EN_IT6601_ARC
			if (g_u1HDMIMode)
				CecSys_ByPassAudioMode();

#endif
			g_u1CecAudioSta = CEC_SYS_AUDIO_STATE_OFF;
			CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
		}
		break;
	case eAbort:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eFeatureAbort;
		u8CecSysCmd[2] = eAbort;
		u8CecSysCmd[3] = CEC_ABORT_REASON_REFUSED;
		CecSys_PushToQueue(u8CecSysCmd, 4, &stTxQ);
		break;
	}
}
/********************************************/
void Cec_SysAudioCtlFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eGiveSystemAudioModeStatus:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eSystemAudioModeStatus;
		u8CecSysCmd[2] = g_u1CecAudioSta;
		CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
		break;
	case eSystemAudioModeRequest:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eSetSystemAudioMode;
		u8CecSysCmd[2] = CEC_SYS_AUDIO_STATE_ON;
		/*come out standby here*/
		g_u8CecPwrSta = CEC_POWER_STATE_ON;
		g_u1CecAudioSta = CEC_SYS_AUDIO_STATE_ON;
/*unMute Speaker here*/
#if EN_IT6601_ARC
		if (g_u1HDMIMode)
			CecSys_HDMIAudioMode();
		else
			CecSys_ARCAudioMode();

#endif

		CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
		break;
	case eGiveAudioStatus:
		u8Temp = u8CecSysCmd[0] >> 4;
		Cec_ReportAudioStatus();
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eReportAudioStatus;
		u8CecSysCmd[2] = (((g_bAudioMute) ? 1 : 0) << 7) +
				 g_u8AudioVolume; // Mute Status + Volume Status
		CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
		break;
	}
}
/********************************************/
void Cec_AudioRateCtrlFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	}
}
/********************************************/
void Cec_ARCFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8Temp;

	switch (u8CecSysCmd[1]) {
	case eInitiateARC:
		break;
	case eReportARCInitiated:
		break;
	case eReportARCTerminated:
/* function implement  for ARC Rx De-activate*/
#if EN_IT6601_ARC
		IT6601_ARCRxTermination();
#endif
		break;
	case eRequestARCInitiation:
/* function implement for ARC Rx Initiation*/
#if EN_IT6601_ARC
		IT6601_ARCRxInitiation();
#endif
#if EN_IT662x_CEC_ARC
		IT662x_CEC_ARC_RX_ONOFF(iTE_TRUE);
#endif
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eInitiateARC;
		CecSys_PushToQueue(u8CecSysCmd, 2, &stTxQ);
		break;
	case eRequestARCTermination:
		u8Temp = u8CecSysCmd[0] >> 4;
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | u8Temp;
		u8CecSysCmd[1] = eTerminateARC;
		CecSys_PushToQueue(u8CecSysCmd, 2, &stTxQ);
		break;
	case eTerminateARC:
#if EN_IT662x_CEC_ARC
		IT662x_CEC_ARC_RX_ONOFF(iTE_FALSE);
#endif
		break;
	}
}
/********************************************/
void Cec_DynAutoLipsyncFeature(iTE_u8 *u8CecSysCmd)
{
	iTE_u8 u8VidLatency = 0, u8AudiLatency = 0;

	switch (u8CecSysCmd[1]) {
	case eRequestCurrentLatency:
		break;
	case eReportCurrentLatency: {
	/*u8CecSysCmd[2], u8CecSysCmd[3] :Tx physical address*/
		u8VidLatency = u8CecSysCmd[4];
		if ((u8CecSysCmd[5] & 0x03) == 0x03)
			u8AudiLatency = u8CecSysCmd[6];

		/*ToDo*/
	} break;
	}
}
/********************************************/
void Cec_HECFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	}
}
/********************************************/
void Cec_AudioFormatFeature(iTE_u8 *u8CecSysCmd)
{
	switch (u8CecSysCmd[1]) {
	case eRequestShortAudioDescription:

		/*report audio type here*/
		break;
	}
}
/********************************************/
iTE_u1 CecSys_PullFromQueue(iTE_pu8 pu8Cmd, iTE_pu8 pu8Len, stCecQ *pstQ)
{
	iTE_u8 u8RPtr = pstQ->u8RPtr;
	iTE_u8 u8WPtr = pstQ->u8WPtr;
	iTE_pu8 pu8Queue = pstQ->pu8Buf;
	iTE_u8 u8QMaxSize = pstQ->u8MaxSize;
	iTE_u8 u8FreeSize;

	if (u8WPtr < u8RPtr)
		u8FreeSize = u8QMaxSize - u8RPtr + u8WPtr;
	else
		u8FreeSize = u8WPtr - u8RPtr;


	if (u8FreeSize) {
		iTE_u8 u8Len;

		u8Len = pu8Queue[u8RPtr++ & (u8QMaxSize - 1)];
		*pu8Len = u8Len;
		if (u8Len > (u8FreeSize - 1)) {
			iTE_MsgE(("[Cec] Queue Empty, %X, %X, %X\n", u8WPtr,
				  u8RPtr, u8Len));
			pstQ->u8RPtr = 0;
			pstQ->u8WPtr = 0;
			return iTE_FALSE;
		}
		u8FreeSize = u8Len;
		while (u8FreeSize--)
			*pu8Cmd++ = pu8Queue[u8RPtr++ & (u8QMaxSize - 1)];

		pstQ->u8RPtr = u8RPtr & (u8QMaxSize - 1);
#if 0
		iTE_Cec_Msg((
			"-CecSys_PullFromQueue wPtr=%02X, rPtr=%02X, u8Len=%X\n",
			u8WPtr, u8RPtr, *pu8Len));
#endif
		return iTE_TRUE;
	}
	*pu8Len = 0;
	return iTE_FALSE;
}
/********************************************/
void CecSys_TxCmdFire(CecSys *pstCecSta, stCecQ *pstTxQ)
{
	iTE_u8 u8Len;

	if (CecSys_PullFromQueue(u8CecSysCmd, &u8Len, pstTxQ) == iTE_TRUE) {
		Cec_CmdFire(u8CecSysCmd, u8Len);
		STA_CHANGE(pstCecSta->u8Sta, CEC_TX_FIRE_MASK, CEC_TX_FIRE_SET);
		pstCecSta->u8TxReFireCnt = 0;
	}
}
/********************************************/
void CecSys_TxPollingMsg(CecSys *pstCecSta)
{
	u8CecSysCmd[0] = (pstCecSta->u8La & 0xF) | (pstCecSta->u8La << 4);

	Cec_CmdFire(u8CecSysCmd, 1);
	STA_CHANGE(pstCecSta->u8Sta, CEC_TX_FIRE_MASK, CEC_TX_FIRE_SET);
	pstCecSta->u8TxReFireCnt = 0;
	STA_CHANGE(pstCecSta->u8Sta, CEC_TX_POLL_MASK, CEC_TX_POLL_SET);
}
/********************************************/
void CecSys_TxReFire(CecSys *pstCecSta)
{
	if (pstCecSta->u8TxReFireCnt < CEC_RE_FIRE_MAX) {
		Cec_Fire();
		pstCecSta->u8TxReFireCnt++;
		STA_CHANGE(pstCecSta->u8Sta, CEC_TX_FIRE_MASK, CEC_TX_FIRE_SET);
	} else {
		iTE_MsgE(("[Cec] Fire Fail > %d\n", CEC_RE_FIRE_MAX));
		pstCecSta->u8TxReFireCnt = 0;
	}
}

void test_msg(int cmd)
{
	pstCecSta->u8La = 5;
	switch (cmd) {
	case 0:
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
		u8CecSysCmd[1] = eReportPhysicalAddress;
		u8CecSysCmd[2] = pstCecSta->u8PaAB;
		u8CecSysCmd[3] = pstCecSta->u8PaCD;
		u8CecSysCmd[4] = pstCecSta->u8Type;
		CecSys_PushToQueue(u8CecSysCmd, 5, &stTxQ);
		CecSys_TxCmdFire(pstCecSta, &stTxQ);
		iTE_Cec_Msg(("w-y cec cmd: %d\n", cmd));
		break;
	case 1:
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
		u8CecSysCmd[1] = eInitiateARC;
		CecSys_PushToQueue(u8CecSysCmd, 2, &stTxQ);
		CecSys_TxCmdFire(pstCecSta, &stTxQ);
		iTE_Cec_Msg(("w-y cec cmd: %d\n", cmd));
		break;
	case 2:
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
		u8CecSysCmd[1] = eSetSystemAudioMode;
		u8CecSysCmd[2] = CEC_SYS_AUDIO_STATE_ON;
		CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
		CecSys_TxCmdFire(pstCecSta, &stTxQ);
		iTE_Cec_Msg(("w-y cec cmd: %d\n", cmd));
		break;
	case 3:
		u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
		u8CecSysCmd[1] = eRequestActiveSource;
		CecSys_PushToQueue(u8CecSysCmd, 2, &stTxQ);
		CecSys_TxCmdFire(pstCecSta, &stTxQ);
		iTE_Cec_Msg(("w-y cec cmd: %d\n", cmd));
		break;
	default:
		break;

	}

}

/********************************************/
void CecSys_TxReportPA(CecSys *pstCecSta)
{
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
	u8CecSysCmd[1] = eReportPhysicalAddress;
	u8CecSysCmd[2] = pstCecSta->u8PaAB;
	u8CecSysCmd[3] = pstCecSta->u8PaCD;
	u8CecSysCmd[4] = pstCecSta->u8Type;
	CecSys_PushToQueue(u8CecSysCmd, 5, &stTxQ);
	/*power on*/
	if (g_u8CecPwrSta == CEC_POWER_STATE_STANDBY) {
		if ((g_u8RX.eARC_RX_event & eARC_ARC_Start) == eARC_ARC_Start)
			CecSys_TxPowerOn(pstCecSta);

	}
}
#if 0 /*(IS_IT663XX == IT66321)*/

void CecSys_HDMIAudioMode(void)
{
	g_u8AudCodec = ADO_DEC_RX0;
	HSw_TxAdoPacketSkip(g_u8CurTxPort, ADO_PKT_MUTE);
	/*ARC terminate*/
#if EN_IT6601_ARC
	IT6601_ARCRxTermination();
#endif
#if EN_IT662x_CEC_ARC
	IT662x_CEC_ARC_RX_ONOFF(iTE_FALSE);
#endif
	if ((g_u8AudCodec & ADO_DEC_MASK) == (ADO_DEC_EN |
		g_u8CurRxPort << ADO_DEC_SHIFT))
		HSw_AudioDecEn(iTE_TRUE, g_u8AdoOut);

	STA_CHANGE(g_pstCurTxInfo->u16TxPortSta,
		TXPORT_ADO_MUTE_MASK, TXPORT_ADO_MUTE_SET);
}
void CecSys_ByPassAudioMode(void)
{
	g_u8AudCodec = 0;
	HSw_TxAdoPacketSkip(g_u8CurTxPort, ADO_PKT_NORMAL);
	HSw_AudioDecEn(0, g_u8AdoOut);

	STA_CHANGE(g_pstCurTxInfo->u16TxPortSta,
		TXPORT_ADO_MUTE_MASK, TXPORT_ADO_MUTE_CLR);
}
void CecSys_ARCAudioMode(void)
{
	g_u8AudCodec = 0;
	HSw_TxAdoPacketSkip(g_u8CurTxPort, ADO_PKT_NORMAL);
	/*ARC On*/
#if EN_IT6601_ARC
	IT6601_ARCRxInitiation();
#endif
#if EN_IT662x_CEC_ARC
	IT662x_CEC_ARC_RX_ONOFF(iTE_TRUE);
#endif
	HSw_AudioDecEn(0, g_u8AdoOut);

	STA_CHANGE(g_pstCurTxInfo->u16TxPortSta,
		TXPORT_ADO_MUTE_MASK, TXPORT_ADO_MUTE_CLR);
}
#endif
/********************************************/
void CecSys_TxAudioModeOn(CecSys *pstCecSta)
{
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
	u8CecSysCmd[1] = eSetSystemAudioMode;
	u8CecSysCmd[2] = CEC_SYS_AUDIO_STATE_ON;

	g_u1CecAudioSta = CEC_SYS_AUDIO_STATE_ON;
/*UnMute Speaker here*/
#if EN_IT6601_ARC
	if (g_u1HDMIMode)
		CecSys_HDMIAudioMode();
	else
		CecSys_ARCAudioMode();
#endif
	CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
}

void CecSys_TxRequestActiveSource(CecSys *pstCecSta)
{
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
	u8CecSysCmd[1] = eRequestActiveSource;
	CecSys_PushToQueue(u8CecSysCmd, 2, &stTxQ);
}

void CecSys_TxChangeAudioMode(CecSys *pstCecSta)
{
	if (g_u8CecPwrSta != CEC_POWER_STATE_ON)
		return;


	if (g_u1CecAudioSta == CEC_SYS_AUDIO_STATE_OFF)
		CecSys_TxAudioModeOn(pstCecSta);

	g_u1HDMIMode = !g_u1HDMIMode;
	iTE_Cec_Msg(("Change Audio Mode:%s \r\n",
		     (g_u1HDMIMode) ? "HDMI Mode" : "ARC/ByPass Mode"));
#if (IS_IT663XX == IT66321)
	if ((g_u16CurTxFun & TX_FUN_MASK) == TX_FUN_PATGEN) {
		;
	} else {
#if EN_IT6601_ARC
		if (g_u1HDMIMode)
			CecSys_HDMIAudioMode();
		else
			CecSys_ARCAudioMode();

#endif
	}
#endif
}
void CecSys_TxVolumeUp(CecSys *pstCecSta)
{
	if (g_u8CecPwrSta != CEC_POWER_STATE_ON)
		return;


	if (g_u1CecAudioSta == CEC_SYS_AUDIO_STATE_OFF)
		CecSys_TxAudioModeOn(pstCecSta);


	/*Volume up here*/
	g_u8AudioVolume++;
	if (g_u8AudioVolume > 100)
		g_u8AudioVolume = 100;

	Cec_ReportAudioStatus();
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eReportAudioStatus;
	u8CecSysCmd[2] = (((g_bAudioMute) ? 1 : 0) << 7) + g_u8AudioVolume;
	/*Mute Status + Volume Status*/
	CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
}

void CecSys_TxVolumeDown(CecSys *pstCecSta)
{
	if (g_u8CecPwrSta != CEC_POWER_STATE_ON)
		return;


	if (g_u1CecAudioSta == CEC_SYS_AUDIO_STATE_OFF)
		CecSys_TxAudioModeOn(pstCecSta);


	/*Volume down here*/
	g_u8AudioVolume--;
	if (g_u8AudioVolume == 0xFF)
		g_u8AudioVolume = 0;

	Cec_ReportAudioStatus();
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eReportAudioStatus;
	u8CecSysCmd[2] = (((g_bAudioMute) ? 1 : 0) << 7) + g_u8AudioVolume;
	/*Mute Status + Volume Status*/
	CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
}

void CecSys_TxVolumeMute(CecSys *pstCecSta)
{
	if (g_u8CecPwrSta != CEC_POWER_STATE_ON)
		return;


	if (g_u1CecAudioSta == CEC_SYS_AUDIO_STATE_OFF)
		CecSys_TxAudioModeOn(pstCecSta);


	/*Volume Mute On/Off here*/
	g_bAudioMute = !g_bAudioMute;
	Cec_ReportAudioStatus();
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eReportAudioStatus;
	u8CecSysCmd[2] = (((g_bAudioMute) ? 1 : 0) << 7) + g_u8AudioVolume;
	/*Mute Status + Volume Status*/
	CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
}

void CecSys_TxPowerOff(CecSys *pstCecSta)
{
	/*enter standby*/
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
	u8CecSysCmd[1] = eSetSystemAudioMode;
	u8CecSysCmd[2] = CEC_SYS_AUDIO_STATE_OFF;
/*Mute Speaker here*/
#if EN_IT6601_ARC
	if (g_u1HDMIMode)
		CecSys_ByPassAudioMode();

#endif

	g_u8CecPwrSta = CEC_POWER_STATE_STANDBY;
	g_u1CecAudioSta = CEC_SYS_AUDIO_STATE_OFF;

	CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);

	/*Terminate ARC*/
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eTerminateARC;
	CecSys_PushToQueue(u8CecSysCmd, 2, &stTxQ);
}

void CecSys_TxPowerOn(CecSys *pstCecSta)
{
	/*power On here*/
	iTE_Cec_Msg(("Power on\n"));
	g_u8CecPwrSta = CEC_POWER_STATE_ON;
	g_u1HDMIMode = 0;
#if EN_IT6601_ARC
	IT6601_ARCRxInitiation();
#endif
#if EN_IT662x_CEC_ARC
	IT662x_CEC_ARC_RX_ONOFF(iTE_TRUE);
#endif
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eInitiateARC;
	CecSys_PushToQueue(u8CecSysCmd, 2, &stTxQ);
	/*audio mode on*/
	CecSys_TxAudioModeOn(pstCecSta);

	CecSys_TxRequestActiveSource(pstCecSta);
}
void CecSys_ARC_InitiateARC(CecSys *pstCecSta)
{
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eInitiateARC;
	CecSys_PushToQueue(u8CecSysCmd, 2, &stTxQ);
}
/*OSD Display*/
void CecSys_TxDisplayDefaultTime(CecSys *pstCecSta)
{

	iTE_u8 u8Temp = 0;

	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eSetOSDString;
	u8CecSysCmd[2] = eDisplayDedaultTime;
	u8CecSysCmd[3] = g_u8OSDString[0];

	for (u8Temp = 1; u8Temp < 12; u8Temp++) {
		if (g_u8OSDString[u8Temp] != '\0')
			u8CecSysCmd[3 + u8Temp] = g_u8OSDString[u8Temp];
		else
			break;

	}

	CecSys_PushToQueue(u8CecSysCmd, 3 + u8Temp, &stTxQ);
}

void CecSys_TxDisplayUntilClear(CecSys *pstCecSta)
{
	iTE_u8 u8Temp = 0;

	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eSetOSDString;
	u8CecSysCmd[2] = eDisplayUntilClear;
	u8CecSysCmd[3] = g_u8OSDString[0];

	for (u8Temp = 1; u8Temp < 12; u8Temp++) {
		if (g_u8OSDString[u8Temp] != '\0')
			u8CecSysCmd[3 + u8Temp] = g_u8OSDString[u8Temp];
		else
			break;

	}

	CecSys_PushToQueue(u8CecSysCmd, 3 + u8Temp, &stTxQ);
}

void CecSys_TxClearPreviousMessage(CecSys *pstCecSta)
{
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eSetOSDString;
	u8CecSysCmd[2] = eClearPreviousMessage;
	CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);
}
/*Dynamic Auto lipsync*/
void CecSys_TxRequestCurrentLatency(CecSys *pstCecSta)
{
	iTE_u8 u8PaAB = 0x00, u8PaCD = 0x00;
	/*iTE_Cec_Msg(("Request Current latency\n"));*/
	u8CecSysCmd[0] = (pstCecSta->u8La << 4) | DEVICE_ID_TV;
	u8CecSysCmd[1] = eRequestCurrentLatency;
	u8CecSysCmd[2] = u8PaAB;
	u8CecSysCmd[3] = u8PaCD;
	CecSys_PushToQueue(u8CecSysCmd, 4, &stTxQ);
}

/********************************************/
/********************************************/

void CecSys_TxDoneCb(iTE_u8 u8TxSta1, iTE_u8 u8TxSta2, iTE_u8 u8TxHeader,
		     iTE_u8 u8TxOpCode)
{
	STA_CHANGE(pstCecSta->u8Sta, CEC_TX_STA_MASK | CEC_TX_DONE_MASK,
		   (u8TxSta1 << CEC_TX_STA_SHIFT) | CEC_TX_DONE_SET);
	if (u8TxSta1 == 0x03) {
		if (u8TxSta2 == 0x02) {
			STA_CHANGE(pstCecSta->u8Sta,
				   CEC_TX_STA_MASK | CEC_TX_DONE_MASK,
				   CEC_TX_STA_NACK | CEC_TX_DONE_SET);
		} else if (u8TxSta2 == 0x01) {
			STA_CHANGE(pstCecSta->u8Sta,
				   CEC_TX_STA_MASK | CEC_TX_DONE_MASK,
				   CEC_TX_STA_ACK | CEC_TX_DONE_SET);
		}
	}
	STA_CHANGE(pstCecSta->u8Sta, CEC_TX_FIRE_MASK, CEC_TX_FIRE_CLR);
	g_u8TxCurHeader = u8TxHeader;
	g_u8TxCurOpCode = u8TxOpCode;
}
/********************************************/

void CecSys_RxDoneCb(iTE_pu8 pu8CecSysCmd, iTE_u8 u8Size)
{
	/*	iTE_pu8	pu8Ptr = u8CecSysCmd;*/
	CecSys_PushToQueue(pu8CecSysCmd, u8Size, &stRxQ);

	iTE_Cec_Msg(("*********\n"));
	while (u8Size--)
		iTE_Cec_Msg(("%02X\n", *pu8CecSysCmd++));

	iTE_Cec_Msg(("*********\n"));
}
/********************************************/

void CecSys_Init(iTE_u8 u8PaAB, iTE_u8 u8PaCD, iTE_u8 u8SelPort)
{
	iTE_u8 u8Temp;
	iTE_pu8 pu8Ptr;
	/*	iTE_u16	u16Temp;*/
	iTE_Cec_Msg(("CecSys_Init %02X%02X, %x\n", u8PaAB, u8PaCD, u8SelPort));

	Cec_TxDoneCb = CecSys_TxDoneCb;
	Cec_RxDoneCb = CecSys_RxDoneCb;

	stTxQ.u8RPtr = 0;
	stTxQ.u8WPtr = 0;
	stTxQ.pu8Buf = u8TxQueue;
	stTxQ.u8MaxSize = CEC_TX_QUEUE_SIZE;

	stRxQ.u8RPtr = 0;
	stRxQ.u8WPtr = 0;
	stRxQ.pu8Buf = u8Rxqueue;
	stRxQ.u8MaxSize = CEC_RX_QUEUE_SIZE;

	u8Temp = sizeof(u8CecList);
	pu8Ptr = (iTE_pu8)u8CecList;
	while (u8Temp--)
		*pu8Ptr++ = 0xFF;

	/* reset CEC eng*/

	pstCecSta->u8PaAB = u8PaAB;
	pstCecSta->u8PaCD = u8PaCD;
	pstCecSta->u8La = DEVICE_ID_AUDIO;
	pstCecSta->u8Type = CEC_DEV_TYPE_AUDIO_SYS;
	pstCecSta->u8Sta = 0;

#if 0
	u16Temp = u8PaAB;
	u16Temp <<= 8;
	u16Temp |= u8PaCD;
	for (u8Temp = 0; u8Temp < 5; u8Temp++) {
		if ((u16Temp & (~u16PaMask[u8Temp])) == 0x00)
			pstCecSta->u8Stage = u8Temp;

	}
#else
	if (u8PaCD) {
		if (u8PaCD & 0x0F)
			pstCecSta->u8Stage = 4;
		else
			pstCecSta->u8Stage = 3;

	} else {
		if (u8PaAB) {
			if (u8PaAB & 0x0F)
				pstCecSta->u8Stage = 2;
			else
				pstCecSta->u8Stage = 1;

		} else
			pstCecSta->u8Stage = 0;

	}
#endif

	CecSys_TxPollingMsg(pstCecSta);
}
/********************************************/

void CecSys_TxHandler(void)
{
	if ((pstCecSta->u8Sta & CEC_TX_DONE_MASK) == CEC_TX_DONE_SET) {
		iTE_u8 u8TxID = g_u8TxCurHeader & 0x0F;

		STA_CHANGE(pstCecSta->u8Sta, CEC_TX_DONE_MASK, CEC_TX_DONE_CLR);
		/*if((pstCecSta->u8Sta & CEC_TX_POLL_MASK) ==*/
		/* CEC_TX_POLL_SET){*/
		if (u8TxID == pstCecSta->u8La) {
			/*	iTE_Cec_Msg(("Tx polling set\n")) ;*/
			STA_CHANGE(pstCecSta->u8Sta, CEC_TX_POLL_MASK,
				   CEC_TX_POLL_CLR);
			switch (pstCecSta->u8Sta & CEC_TX_STA_MASK) {
			case CEC_TX_STA_ACK:
				pstCecSta->u8La = u8SwitchLA[pstCecSta->u8La];
				if (pstCecSta->u8La != 0x0F)
					CecSys_TxPollingMsg(pstCecSta);
				else
					iTE_MsgE(("[Cec] no LogAdr\n"));

				break;
			case CEC_TX_STA_NACK:
				STA_CHANGE(pstCecSta->u8Sta, CEC_TX_INIT_MASK,
					   CEC_TX_INIT_SET);
				Cec_LaSet(pstCecSta->u8La);
				CecSys_TxReportPA(pstCecSta);
				break;
			}
		} else if ((g_u8TxCurOpCode == eSetSystemAudioMode) &&
			   u8TxID == DEVICE_ID_TV) {
			iTE_Cec_Msg(("SetSystemAudioMode =>Broadcast On\n"));
			u8CecSysCmd[0] =
				(pstCecSta->u8La << 4) | DEVICE_ID_BROADCAST;
			u8CecSysCmd[1] = eSetSystemAudioMode;
			u8CecSysCmd[2] = CEC_SYS_AUDIO_STATE_ON;
			CecSys_PushToQueue(u8CecSysCmd, 3, &stTxQ);

			g_u1CecAudioSta = CEC_SYS_AUDIO_STATE_ON;
		} else {
			if ((pstCecSta->u8Sta & CEC_TX_STA_MASK) !=
			    CEC_TX_STA_ACK) {
				CecSys_TxReFire(pstCecSta);
			}
		}
	}
	if ((pstCecSta->u8Sta & (CEC_TX_DONE_MASK | CEC_TX_FIRE_MASK)) ==
	    (CEC_TX_DONE_CLR | CEC_TX_FIRE_CLR)) {
		/* Fire new Cmd;*/
		CecSys_TxCmdFire(pstCecSta, &stTxQ);
	}
	CecSys_ARC_Start();
}
/********************************************/

void CecSys_RxHandler(void)
{
	iTE_u8 u8Len;

	if (CecSys_PullFromQueue(u8CecSysCmd, &u8Len, &stRxQ) == iTE_TRUE) {
		iTE_u8 u8Cnt;

		iTE_Cec_Msg(("Cec header = %02X, %02X\n", u8CecSysCmd[0],
			     u8CecSysCmd[1]));
		for (u8Cnt = 0; u8Cnt < SizeOfLookUpTable_CEC; u8Cnt++) {
			if (u8CecSysCmd[1] == LookUpTable_CEC[u8Cnt].cmd) {
				iTE_u8 u8Size = LookUpTable_CEC[u8Cnt].size;

				iTE_Cec_Msg(("CEC Rx Cmd => %s\n",
					     LookUpTable_CEC[u8Cnt].CmdString));
				if (u8Len != (u8Size & 0x1F)) {
					if ((u8CecSysCmd[1] ==
					     eSystemAudioModeRequest) &&
					    (u8Len == 2)) {
						u8CecSysCmd[0] =
							(pstCecSta->u8La << 4) |
							DEVICE_ID_BROADCAST;
						u8CecSysCmd[1] =
							eSetSystemAudioMode;
						u8CecSysCmd[2] =
							CEC_SYS_AUDIO_STATE_OFF;

/*Mute Speaker here*/
#if EN_IT6601_ARC
if (g_u1HDMIMode)
	CecSys_ByPassAudioMode();

#endif
						g_u1CecAudioSta =
							CEC_SYS_AUDIO_STATE_OFF;

						CecSys_PushToQueue(u8CecSysCmd,
								   3, &stTxQ);
					} else {
						iTE_MsgE((
							"[Cec] Invalid Size %d != %d\n",
							u8Len,
							(u8Size & 0x1F)));
					}
				} else {
					iTE_u8 u8Follower =
						u8CecSysCmd[0] & 0xF;
					iTE_u8 u8Temp = u8Size & 0xE0;

					if (((u8Temp == eDirectly) &&
					     (u8Follower ==
					      DEVICE_ID_BROADCAST)) ||
					    ((u8Temp == eBroadcast) &&
					     (u8Follower !=
					      DEVICE_ID_BROADCAST))) {
						iTE_MsgE((
						"[Cec] Wrong Follower\n"));
					} else {

					switch (LookUpTable_CEC[u8Cnt]
								.eType) {
						case eOTPFeature:
							Cec_OTPFeature(
								u8CecSysCmd);
							break;
						case eRoutingCtlFeature:
							Cec_RoutingCtlFeature(
								u8CecSysCmd);
							break;
						case eStandByFeature:
							Cec_StandByFeature(
								u8CecSysCmd);
							break;
						case eOTRFeature:
							Cec_OTRFeature(
								u8CecSysCmd);
							break;
						case eTimerProgFeature:
							Cec_TimerProgFeature(
								u8CecSysCmd);
							break;
						case eSysInfoFeature:
							Cec_SysInfoFeature(
								u8CecSysCmd);
							break;
						case eDeckCtrlFeature:
							Cec_DeckCtrlFeature(
								u8CecSysCmd);
							break;
						case eTunerCtrlFeature:
							Cec_TunerCtrlFeature(
								u8CecSysCmd);
							break;
						case eVenSpecCmdFeature:
							Cec_VenSpecCmdFeature(
								u8CecSysCmd);
							break;
						case eOSDDisplayFeature:
							Cec_OSDDisplayFeature(
								u8CecSysCmd);
							break;
						case eDevOSDTransferFeature:
						Cec_DevOSDTransferFeature(
								u8CecSysCmd);
							break;
						case eDevMenuCtrlFeature:
							Cec_DevMenuCtrlFeature(
								u8CecSysCmd);
							break;
				case eRemoteCtlPassthroughFeature:
					Cec_RemoteCtlPassthroughFeature(
								u8CecSysCmd);
							break;
						case ePowerStatusFeature:
							Cec_PowerStatusFeature(
								u8CecSysCmd);
							break;
						case eGeneralProtocolMessage:
						Cec_GeneralProtocolMessage(
								u8CecSysCmd);
							break;
						case eSysAudioCtlFeature:
							Cec_SysAudioCtlFeature(
								u8CecSysCmd);
							break;
						case eAudioRateCtrlFeature:
						Cec_AudioRateCtrlFeature(
								u8CecSysCmd);
							break;
						case eARCFeature:
							Cec_ARCFeature(
								u8CecSysCmd);
							break;
						case eDynAutoLipsyncFeature:
						Cec_DynAutoLipsyncFeature(
								u8CecSysCmd);
							break;
						case eHECFeature:
							Cec_HECFeature(
								u8CecSysCmd);
							break;
						case eAudioFormatFeature:
							Cec_AudioFormatFeature(
								u8CecSysCmd);
						default:
							break;
						}
					}
				}
				break;
			}
		}
	}
}
/********************************************/

void CecSys_TxTest(iTE_u8 u8Cmd)
{
	switch (u8Cmd) {
	}
}

void CecSys_TxAudioTest(iTE_u8 u8Cmd)
{
	switch (u8Cmd) {
	case 0:
		CecSys_TxVolumeUp(pstCecSta);
		break;
	case 1:
		CecSys_TxVolumeDown(pstCecSta);
		break;
	case 2:
		CecSys_TxPowerOn(pstCecSta);
		break;
	case 3:
		CecSys_TxPowerOff(pstCecSta);
		break;
	case 4:
		CecSys_TxVolumeMute(pstCecSta);
		break;
	case 5:
		CecSys_TxChangeAudioMode(pstCecSta);
		break;
	case 6:
		CecSys_TxDisplayUntilClear(pstCecSta);
		break;
	}
}
#if (En_eARCRX == iTE_TRUE)
void CecSys_ARC_Start(void) /*waiting...*/
{
	if ((g_u8RX.eARC_RX_event & eARC_ARC_Start) == eARC_ARC_Start) {
		if (IT662x_CEC_ARC_State() == iTE_FALSE) {
			CecSys_TxPowerOn(pstCecSta);
			CecSys_ARC_InitiateARC(pstCecSta);
		}
	}
}
#endif

#endif

#include <stdint.h>

#define i8 char
#define i16 short
#define i32 int
#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int

typedef struct
{
	i8 FwName[32]; 
	i8 Date[32]; 
	i32 HwVer; 
	i32 FwVer; 
	i32 SN[3]; 
	i32 Funs; 
}TDevInfo;

typedef struct
{
	u32 ID; 
	u32 TimeStamp; 
	u8 RemoteFlag; 
	u8 ExternFlag; 
	u8 Len; 
	u8 Data[8]; 
	u8 Res;
}TCanInfo;

typedef struct
{
	u32 BRP; 
	u8 SJW; 
	u8 BS1; 
	u8 BS2; 
	u8 Mode; 
	u8 ABOM; 
	u8 NART; 
	u8 RFLM; 
	u8 TXFP; 
}TCanCfg;


typedef struct
{
	u32 ID; 
	u8 DLC; 
	u8 Flags; 
	u8 Res[2]; 
	u32 TimeStamp; 
	u8 Data[64]; 
}TCanFdInfo;

typedef struct
{
	u8 Mode; 
	u8 IsoEn;
	u8 Retry;
	u8 ResEn;
	u8 NBRP;
	u8 NSEG1;
	u8 NSEG2;
	u8 NSJW;
	u8 DBRP;
	u8 DSEG1;
	u8 DSEG2;
	u8 DSJW;
	u8 Res[8];
}TCanFdCfg;

typedef struct
{
	u32 Timestamp; 
	u8 MsgType; 
	u8 CheckType; 
	u8 Len; 
	u8 Sync; 
	u8 Id; 
	u8 Data[8]; 
	u8 Check; 
	u8 BreakBits; 
	u8 Reserve1;
}TLinInfo;


i32 USB_ScanDevice(i32 *pDevHandle);
bool USB_OpenDevice(i32 Handle);

bool USB_CloseDevice(i32 Handle);
bool USB_ResetDevice(i32 Handle);
bool DEV_GetDeviceInfo(i32 Handle,TDevInfo* pDevInfo,i8 *Str);
bool DEV_GetTimestamp(i32 Handle,i8 BusType,u32 *pTimestamp);

i32 CAN_Init(i32 Handle, u8 Index, TCanCfg* pCanConfig);
i32 CAN_StartGetMsg(i32 Handle, u8 Index);
i32 CAN_StopGetMsg(i32 Handle, u8 Index);
i32 CAN_SendMsg(i32 Handle, u8 Index, TCanInfo* CanInfo,u32 SendMsgNum);
i32 CAN_GetMsg(i32 Handle, u8 Index, TCanInfo* CanInfo);
i32 CAN_GetMsgWithSize(i32 Handle, u8 Index, TCanInfo *CanInfo,i32 BufferSize);
i32 CAN_ClearMsg(i32 Handle, u8 Index);

i32 CANFD_Init(i32 Handle, u8 Index, TCanFdCfg *pCanConfig);
i32 CANFD_StartGetMsg(i32 Handle, u8 Index);
i32 CANFD_StopGetMsg(i32 Handle, u8 Index);
i32 CANFD_SendMsg(i32 Handle, u8 Index, TCanFdInfo *CanFdInfo,u32 SendMsgNum);
i32 CANFD_GetMsg(i32 Handle, u8 Index, TCanFdInfo *CanFdInfo,i32 BufferSize);

i32 LIN_EX_Init(i32 Handle,u8 Index,u32 BaudRate,u8 MasterMode);
i32 LIN_EX_MasterSync(i32 Handle,u8 Index,TLinInfo *pInMsg,TLinInfo *pOutMsg,u32 MsgLen);
i32 LIN_EX_MasterBreak(i32 Handle,u8 Index);
i32 LIN_EX_MasterWrite(i32 Handle,u8 Index,u8 PID,u8 *pData,u8 DataLen,u8 CheckType);
i32 LIN_EX_MasterRead(i32 Handle,u8 Index,u8 PID,u8 *pData);
i32 LIN_EX_SlaveSetIDMode(i32 Handle,u8 Index,TLinInfo *LinInfo,u32 MsgLen);
i32 LIN_EX_SlaveGetIDMode(i32 Handle,u8 Index,TLinInfo *LinInfo);
i32 LIN_EX_SlaveGetData(i32 Handle,u8 Index,TLinInfo *LinInfo);
i32 LIN_EX_CtrlPowerOut(i32 Handle,u8 Index,u8 VbatValue);
i32 LIN_EX_GetVbatValue(i32 Handle,u16 *pBatValue);
i32 LIN_EX_MasterStartSch(i32 Handle,u8 Index,TLinInfo *LinInfo,u32 MsgLen);
i32 LIN_EX_MasterStopSch(i32 Handle,u8 Index);
i32 LIN_EX_MasterGetSch(i32 Handle,u8 Index,TLinInfo *LinInfo);

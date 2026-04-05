/*
*********************************************************************************************************
*	                                  
*	模块名称 : 示波器主界面上的状态窗口
*	文件名称 : DSO_Status2Dlg.c
*	版    本 : V1.0
*	说    明 : 示波器主界面上的状态窗口，用于显示频率和RMS。
*	修改记录 :
*		版本号    日期          作者           说明
*		V1.0    2018-01-06     Eric2013        首发 
*                                           
*	Copyright (C), 2018-2028, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "includes.h"
#include "MainTask.h"



/*
*********************************************************************************************************
*	                                  变量
*********************************************************************************************************
*/
static GUI_MEMDEV_Handle   hMemStatus2Dlg;


///*
//*********************************************************************************************************
//*				                      宏定义
//*********************************************************************************************************
//*/
//#define ID_TEXT_0 		(GUI_ID_USER + 0x00)
//#define ID_TEXT_1  		(GUI_ID_USER + 0x01)
//#define ID_TEXT_2 		(GUI_ID_USER + 0x02)
//#define ID_TEXT_3 		(GUI_ID_USER + 0x03)
//#define ID_TEXT_4 		(GUI_ID_USER + 0x04)
//#define ID_TEXT_5 		(GUI_ID_USER + 0x05)
//#define ID_TEXT_6 		(GUI_ID_USER + 0x06)
//#define ID_TEXT_7 		(GUI_ID_USER + 0x07)
//#define ID_TEXT_8 		(GUI_ID_USER + 0x08)


///*
//*********************************************************************************************************
//*					        GUI_WIDGET_CREATE_INFO类型数组
//*********************************************************************************************************
//*/
//static const GUI_WIDGET_CREATE_INFO _aDialogCreateStatus2[] = 
//{ 
//    { WINDOW_CreateIndirect,    "",       0,        663,  373,  135,  66, 0},
//	
//	{ TEXT_CreateIndirect,   "RMS",      ID_TEXT_0,  35,  2,  30, 16, 0, 0},
//	{ TEXT_CreateIndirect,   "0.000V",   ID_TEXT_1,  72,  2,  50, 16, 0, 0},
//	{ TEXT_CreateIndirect,   "Freq",     ID_TEXT_2,  35,  32,  25, 16, 0, 0},
//	{ TEXT_CreateIndirect,   "000000Hz", ID_TEXT_3,  72,  32,  70, 16, 0, 0},
//	
//	{ TEXT_CreateIndirect,   "RMS",      ID_TEXT_4,  35,  17, 30, 16, 0, 0},
//	{ TEXT_CreateIndirect,   "0.000V",   ID_TEXT_5,  72,  17, 50, 16, 0, 0},
//	{ TEXT_CreateIndirect,   "Freq",     ID_TEXT_6,  35,  47,  25, 16, 0, 0},
//	{ TEXT_CreateIndirect,   "000000Hz", ID_TEXT_7,  72,  47,  70, 16, 0, 0},
//};

///*
//*********************************************************************************************************
//*	函 数 名: PaintDialogStatus2()
//*	功能说明: 时基窗口的回调函数重绘
//*	形    参: pMsg  指针地址         	
//*	返 回 值: 无
//*********************************************************************************************************
//*/
//static void PaintDialogStatus2(WM_MESSAGE * pMsg)
//{
//	/* 清背景色 */
//	GUI_SetBkColor(0x905040);
//	GUI_Clear();

//	/* 绘制填充的抗锯齿圆角矩形 */	
//	GUI_SetColor(GUI_BLACK);
//	GUI_AA_FillRoundedRect(0, 0, 134, 65, 10);

//	/* 绘制抗锯齿圆角矩形 */	
//	GUI_SetColor(0XEBCD9E);
//	GUI_SetPenSize(2);
//	GUI_AA_DrawRoundedRect(0, 0, 134, 65, 10);
//	
//	/* 绘制抗锯齿圆角矩形，并填数值1 */
//	GUI_SetColor(GUI_YELLOW);
//	GUI_AA_FillRoundedRect(10, 4, 30, 16, 3);
//	
//	GUI_SetColor(GUI_BLACK);
//    GUI_SetFont(&GUI_Font16_1);
//	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
//	GUI_DispCharAt('1', 16, 3);

//	/* 绘制抗锯齿圆角矩形，并填数值2 */
//	GUI_SetColor(GUI_GREEN);
//	GUI_AA_FillRoundedRect(10, 19, 30, 31, 3);
//						   
//	GUI_SetColor(GUI_BLACK);
//    GUI_SetFont(&GUI_Font16_1);
//	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
//	GUI_DispCharAt('2', 16, 18);

//	/* 绘制抗锯齿圆角矩形，并填数值1 */
//	GUI_SetColor(GUI_YELLOW);
//	GUI_AA_FillRoundedRect(10, 34, 30, 47, 3);
//	
//	GUI_SetColor(GUI_BLACK);
//    GUI_SetFont(&GUI_Font16_1);
//	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
//	GUI_DispCharAt('1', 16, 32);

//	/* 绘制抗锯齿圆角矩形，并填数值2 */
//	GUI_SetColor(GUI_GREEN);
//	GUI_AA_FillRoundedRect(10, 50, 30, 62, 3);
//						   
//	GUI_SetColor(GUI_BLACK);
//    GUI_SetFont(&GUI_Font16_1);
//	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
//	GUI_DispCharAt('2', 16, 49);
//}

///*
//*********************************************************************************************************
//*	函 数 名: InitDialogStatus2
//*	功能说明: 时基窗口的回调函数的初始化
//*	形    参: pMsg  指针地址         	
//*	返 回 值: 无
//*********************************************************************************************************
//*/
//static void InitDialogStatus2(WM_MESSAGE * pMsg)
//{
//	int i;
//    WM_HWIN hWin = pMsg->hWin;

//	//
//    // 初始化文本控件
//    //
//	for(i = ID_TEXT_0; i < ID_TEXT_4; i++)
//	{
//		TEXT_SetTextColor(WM_GetDialogItem(hWin, i), GUI_GREEN);
//		TEXT_SetFont(WM_GetDialogItem(hWin, i), &GUI_Font16_1);		
//	}
//	
//	for(i = ID_TEXT_4; i < ID_TEXT_8; i++)
//	{
//		TEXT_SetTextColor(WM_GetDialogItem(hWin, i), 0x00ffff);
//		TEXT_SetFont(WM_GetDialogItem(hWin, i), &GUI_Font16_1);		
//	}
//	
//	/* 创建定时器 */
//	WM_CreateTimer(hWin,   /* 接受信息的窗口的句柄 */
//				   0, 	   /* 用户定义的Id。如果不对同一窗口使用多个定时器，此值可以设置为零。 */
//				   1000,   /* 周期，此周期过后指定窗口应收到消息*/
//				   0);	   /* 留待将来使用，应为0 */	
//}

///*
//*********************************************************************************************************
//*	函 数 名: _cbCallbackStatus2()
//*	功能说明: 时基窗口的回调函数
//*	形    参：pMsg  指针地址         	
//*	返 回 值: 无
//*********************************************************************************************************
//*/
//static void _cbCallbackStatus2(WM_MESSAGE * pMsg) 
//{
//	char buf[20];
//    WM_HWIN hWin = pMsg->hWin;
//	
//	
//    switch (pMsg->MsgId) 
//    {
//		case WM_INIT_DIALOG:
//            InitDialogStatus2(pMsg);
//            break;
//				
//		case WM_PAINT:
//			GUI_MEMDEV_WriteAt(hMemStatus2Dlg, 663, 373);
//            break;
//		
//		case WM_TIMER:
//			/* 均方根 */
//			if(g_DSO1->ucMeasureFlag[22] == 1)
//			{
//				sprintf(buf, "%5.3fV", g_DSO1->WaveRMS);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_1), buf);
//			
//				sprintf(buf, "%5.3fV", g_DSO2->WaveRMS);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_5), buf);
//			}
//		
//			/* 波形频率估算 */
//			if(g_DSO1->ucMeasureFlag[1] == 1)
//			{
//				if(TimeBaseId < 10)
//				{
//					sprintf(buf, "%06dHz", g_DSO1->uiFreq);
//					TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_3), buf);				
//				}
//				else
//				{
//					TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_3), "000000Hz");					
//				}
//			}

//			WM_RestartTimer(pMsg->Data.v, 500);
//			break;
//			
//        default:
//            WM_DefaultProc(pMsg);
//    }
//}

/*
*********************************************************************************************************
*				                      宏定义
*********************************************************************************************************
*/
#define ID_TEXT_0 		(GUI_ID_USER + 0x00)
#define ID_TEXT_1  		(GUI_ID_USER + 0x01)
#define ID_TEXT_2 		(GUI_ID_USER + 0x02)
#define ID_TEXT_3 		(GUI_ID_USER + 0x03)
#define ID_TEXT_4 		(GUI_ID_USER + 0x04)
#define ID_TEXT_5 		(GUI_ID_USER + 0x05)
#define ID_TEXT_6 		(GUI_ID_USER + 0x06)
#define ID_TEXT_7 		(GUI_ID_USER + 0x07)
#define ID_TEXT_8 		(GUI_ID_USER + 0x08)
#define ID_TEXT_9 		(GUI_ID_USER + 0x09)
#define ID_TEXT_10  	(GUI_ID_USER + 0x0A)
#define ID_TEXT_11 		(GUI_ID_USER + 0x0B)
#define ID_TEXT_12 		(GUI_ID_USER + 0x0C)

#define ID_TEXT_13 		(GUI_ID_USER + 0x0D)
#define ID_TEXT_14  	(GUI_ID_USER + 0x0E)
#define ID_TEXT_15 		(GUI_ID_USER + 0x0F)
#define ID_TEXT_16 		(GUI_ID_USER + 0x10)
#define ID_TEXT_17 		(GUI_ID_USER + 0x11)

#define ID_TEXT_18 		(GUI_ID_USER + 0x12)
#define ID_TEXT_19  	(GUI_ID_USER + 0x13)
#define ID_TEXT_20 		(GUI_ID_USER + 0x14)
#define ID_TEXT_21 		(GUI_ID_USER + 0x15)
#define ID_TEXT_22 		(GUI_ID_USER + 0x16)
#define ID_TEXT_23 		(GUI_ID_USER + 0x17)

#define ID_TEXT_24 		(GUI_ID_USER + 0x18)
#define ID_TEXT_25 		(GUI_ID_USER + 0x19)
#define ID_TEXT_26 		(GUI_ID_USER + 0x1A)
#define ID_TEXT_27 		(GUI_ID_USER + 0x1B)
#define ID_TEXT_28 		(GUI_ID_USER + 0x1C)
#define ID_TEXT_29 		(GUI_ID_USER + 0x1D)
#define ID_TEXT_30 		(GUI_ID_USER + 0x1E)
#define ID_TEXT_31 		(GUI_ID_USER + 0x1F)
#define ID_TEXT_32 		(GUI_ID_USER + 0x20)


/*
*********************************************************************************************************
*					GUI_WIDGET_CREATE_INFO类型数组
*********************************************************************************************************
*/
static const GUI_WIDGET_CREATE_INFO _aDialogCreateStatus2[] = 
{
    { WINDOW_CreateIndirect,     "",     0,          722,  648, 550, 69, 0},
	
	/* 第一行 */
	{ TEXT_CreateIndirect,    "RMS",    ID_TEXT_0,    35,  0, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_1,   75,  0, 60, 16, 0, 0},
	
	{ TEXT_CreateIndirect,    "RMS",   	 ID_TEXT_2,    175, 0, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_3,   215, 0, 60, 16, 0, 0},
	
	{ TEXT_CreateIndirect,    "RMS",   	ID_TEXT_4,    315, 0, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_5,   355, 0, 60, 16, 0, 0},	

	{ TEXT_CreateIndirect,    "RMS",      ID_TEXT_6,   455, 0, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.00V",    ID_TEXT_7,   495, 0, 60, 16, 0, 0},	
	
	/* 第二行 */		
    { TEXT_CreateIndirect,    "Min",    ID_TEXT_8,    35, 16, 40, 16, 0, 0},
    { TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_9,   75, 16, 60, 16, 0, 0},	

    { TEXT_CreateIndirect,    "Min",     ID_TEXT_10,  	175, 16, 40, 16, 0, 0},
    { TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_11,    215, 16, 60, 16, 0, 0},	

	{ TEXT_CreateIndirect,    "Min",   ID_TEXT_12,   315, 16, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_13,  355, 16, 60, 16, 0, 0},
	
	{ TEXT_CreateIndirect,    "Min",      ID_TEXT_14,   455, 16, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.00V",    ID_TEXT_15,   495, 16, 60, 16, 0, 0},
	
	/* 第三行 */
	{ TEXT_CreateIndirect,    "Max",     ID_TEXT_16,   35, 32, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_17,  75, 32, 60, 16, 0, 0},
	
	{ TEXT_CreateIndirect,    "Max",     ID_TEXT_18,   175, 32, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_19,   215, 32, 60, 16, 0, 0},	
	
	{ TEXT_CreateIndirect,    "Max",    ID_TEXT_20,   315, 32, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "000000Hz", ID_TEXT_21,   355, 32, 60, 16, 0, 0},
	
	{ TEXT_CreateIndirect,    "Max",      ID_TEXT_22,   455, 32, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00:00:00",     ID_TEXT_23,   495, 32, 60, 16, 0, 0},

	/* 第四行 */
	{ TEXT_CreateIndirect,    "Pk-Pk",     ID_TEXT_24,   35, 48, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_25,   75, 48, 60, 16, 0, 0},
	
	{ TEXT_CreateIndirect,    "Pk-Pk",     ID_TEXT_26,   175, 48, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.0000V",  ID_TEXT_27,   215, 48, 60, 16, 0, 0},	
	
	{ TEXT_CreateIndirect,    "Pk-Pk",    ID_TEXT_28,   315, 48, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "000000Hz",  ID_TEXT_29,   355, 48, 60, 16, 0, 0},
	
	{ TEXT_CreateIndirect,    "Pk-Pk",      ID_TEXT_30,   455, 48, 40, 16, 0, 0},
	{ TEXT_CreateIndirect,    "00.00:00",     ID_TEXT_31,   495, 48, 60, 16, 0, 0},	
	
	/* 新增的最后一列 */
	
};

/*
*********************************************************************************************************
*	函 数 名: PaintDialogStatus1
*	功能说明: 状态窗口的回调函数重绘消息
*	形    参：pMsg  指针地址         	
*	返 回 值: 无
*********************************************************************************************************
*/
void PaintDialogStatus2(WM_MESSAGE * pMsg)
{
	/* 清背景色 */
	GUI_SetBkColorIndex(10);
  	GUI_Clear();
	
	/* 绘制填充的抗锯齿圆角矩形 */
	GUI_SetColor(GUI_DARKGRAY);
	GUI_AA_FillRoundedRect(0, 0, 549, 65, 10);
	
	/* 绘制抗锯齿圆角矩形 */
	GUI_SetColor(0x434343);
	GUI_SetPenSize(2);
	GUI_AA_DrawRoundedRect(0, 0, 549, 65, 10);
	
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(CHXColor[4]);
	GUI_AA_FillRoundedRect(10, 4, 30, 16, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('5', 17,  4);

	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(CHXColor[4]);
	GUI_AA_FillRoundedRect(10, 19, 30, 31, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('5', 17, 19);
	
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(CHXColor[4]);
	GUI_AA_FillRoundedRect(10, 34, 30, 46, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('5', 17,  34);
	
	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(CHXColor[4]);
	GUI_AA_FillRoundedRect(10, 49, 30, 61, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('5', 17, 49);
	
	//////////////////////////////////////////////////////
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(CHXColor[5]);
	GUI_AA_FillRoundedRect(150, 4, 170, 16, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('6', 157,  4);

	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(CHXColor[5]);
	GUI_AA_FillRoundedRect(150, 19, 170, 31, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('6', 157, 19);
	
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(CHXColor[5]);
	GUI_AA_FillRoundedRect(150, 34, 170, 46, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('6', 157,  34);
	
	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(CHXColor[5]);
	GUI_AA_FillRoundedRect(150, 49, 170, 61, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('6', 157, 49);
	
	//////////////////////////////////////////////////////
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(CHXColor[6]);
	GUI_AA_FillRoundedRect(290, 4, 310, 16, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('7', 297,  4);

	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(CHXColor[6]);
	GUI_AA_FillRoundedRect(290, 19, 310, 31, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('7', 297, 19);
	
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(CHXColor[6]);
	GUI_AA_FillRoundedRect(290, 34, 310, 46, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('7', 297,  34);
	
	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(CHXColor[6]);
	GUI_AA_FillRoundedRect(290, 49, 310, 61, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('7', 297, 49);
	
	
	//////////////////////////////////////////////////////
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(CHXColor[7]);
	GUI_AA_FillRoundedRect(430, 4, 450, 16, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('8', 437,  4);

	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(CHXColor[7]);
	GUI_AA_FillRoundedRect(430, 19, 450, 31, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('8', 437, 19);
	
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(CHXColor[7]);
	GUI_AA_FillRoundedRect(430, 34, 450, 46, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('8', 437,  34);
	
	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(CHXColor[7]);
	GUI_AA_FillRoundedRect(430, 49, 450, 61, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('8', 437, 49);
}


/*
*********************************************************************************************************
*	函 数 名: InitDialogStatus1()
*	功能说明: 状态窗口的回调函数初始化消息
*	形    参：pMsg  指针地址         	
*	返 回 值: 无
*********************************************************************************************************
*/

void InitDialogStatus2(WM_MESSAGE * pMsg)
{
	int i;
    WM_HWIN hWin = pMsg->hWin;

	//
    // 初始化文本控件
    //
	
	for(i = ID_TEXT_0; i < ID_TEXT_8; i++)
	{
		TEXT_SetTextColor(WM_GetDialogItem(hWin, i), CHXColor[(i-ID_TEXT_0)/2+4]);
		TEXT_SetFont(WM_GetDialogItem(hWin, i), &GUI_Font16_1);	
	}
	
	for(i = ID_TEXT_8; i < ID_TEXT_16; i++)
	{
		TEXT_SetTextColor(WM_GetDialogItem(hWin, i),CHXColor[(i-ID_TEXT_8)/2+4]);
		TEXT_SetFont(WM_GetDialogItem(hWin, i), &GUI_Font16_1);		
	}
	
	for(i = ID_TEXT_16; i < ID_TEXT_24; i++)
	{
		TEXT_SetTextColor(WM_GetDialogItem(hWin, i), CHXColor[(i-ID_TEXT_16)/2+4]);
		TEXT_SetFont(WM_GetDialogItem(hWin, i), &GUI_Font16_1);		
	}
	
	for(i = ID_TEXT_24; i < ID_TEXT_32; i++)
	{
		TEXT_SetTextColor(WM_GetDialogItem(hWin, i), CHXColor[(i-ID_TEXT_24)/2+4]);
		TEXT_SetFont(WM_GetDialogItem(hWin, i), &GUI_Font16_1);		
	}

	/* 创建定时器 */
	WM_CreateTimer(hWin,   /* 接受信息的窗口的句柄 */
				   0, 	   /* 用户定义的Id。如果不对同一窗口使用多个定时器，此值可以设置为零。 */
				   1000,   /* 周期，此周期过后指定窗口应收到消息*/
				   0);	   /* 留待将来使用，应为0 */	
}

/*
*********************************************************************************************************
*	函 数 名: _cbCallbackStatus1
*	功能说明: 状态窗口的回调函数
*	形    参: pMsg  指针地址         	
*	返 回 值: 无
*********************************************************************************************************
*/
static void _cbCallbackStatus2(WM_MESSAGE * pMsg) 
{
    WM_HWIN hWin = pMsg->hWin;
	char buf[20];
	
    switch (pMsg->MsgId) 
    {
        case WM_PAINT:
			PaintDialogStatus2(pMsg);
			//GUI_MEMDEV_WriteAt(hMemStatus1Dlg, 145, 444);
            break;
		
        case WM_INIT_DIALOG:
            InitDialogStatus2(pMsg);
            break;
		
		case WM_TIMER:
//			/* 平均值 */
//			if(g_DSO1->ucMeasureFlag[24] == 1)
//			{
//				sprintf(buf, "%5.3fV", g_DSO1->WaveMean);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_0),  g_MeasureTable[20]);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_1),  buf);
//				
//				sprintf(buf, "%5.3fV", g_DSO2->WaveMean);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_8),  g_MeasureTable[20]);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_9),  buf);
//			}
//			
//			/* 峰峰值 */
//			if(g_DSO1->ucMeasureFlag[12] == 1)
//			{
//				sprintf(buf, "%5.3fV", g_DSO1->WavePkPk);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_2),  g_MeasureTable[12]);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_3),  buf);
//				
//				sprintf(buf, "%5.3fV", g_DSO2->WavePkPk);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_10),  g_MeasureTable[12]);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_11),  buf);				
//			}

//			/* 最小值 */
//			if(g_DSO1->ucMeasureFlag[15] == 1)
//			{
//				sprintf(buf, "%5.3fV", g_DSO1->WaveMin);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_4),  g_MeasureTable[15]);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_5),  buf);
//				
//				sprintf(buf, "%5.3fV", g_DSO2->WaveMin);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_12),  g_MeasureTable[15]);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_13),  buf);
//			}

//			/* 最大值 */
//			if(g_DSO1->ucMeasureFlag[14] == 1)
//			{
//				sprintf(buf, "%5.3fV", g_DSO1->WaveMax);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_6),  g_MeasureTable[14]);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_7),  buf);
//				
//				sprintf(buf, "%5.3fV", g_DSO2->WaveMax);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_14),  g_MeasureTable[14]);
//				TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_15),  buf);
//			}
			
			WM_RestartTimer(pMsg->Data.v, 500);
			break;
			
        default:
            WM_DefaultProc(pMsg);
    }
}

/*
*********************************************************************************************************
*	函 数 名: CreateStatus2Dlg
*	功能说明: 时基窗口的回调函数
*	形    参: pMsg  指针地址         	
*	返 回 值: 无
*********************************************************************************************************
*/
WM_HWIN CreateStatus2Dlg(void) 
{
	WM_HWIN hWin;

	hWin = GUI_CreateDialogBox(_aDialogCreateStatus2, 
	                           GUI_COUNTOF(_aDialogCreateStatus2), 
	                           &_cbCallbackStatus2, 
	                           0, 
	                           0, 
	                           0);

	return hWin;
}

/*
*********************************************************************************************************
*	函 数 名: DrawWinStatus2Bk
*	功能说明: 将窗口背景绘制到存储设备里面
*	形    参: pMsg  指针地址         	
*	返 回 值: 无
*********************************************************************************************************
*/
void DrawWinStatus2Bk(void) 
{
	hMemStatus2Dlg = GUI_MEMDEV_CreateFixed(0, 
										    0, 
									        510, 
									        35,
									        GUI_MEMDEV_HASTRANS, 
										    GUI_MEMDEV_APILIST_16, 
										    GUICC_M565);
	GUI_MEMDEV_Select(hMemStatus2Dlg);
	PaintDialogStatus2(NULL);
	GUI_MEMDEV_Select(0);
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/

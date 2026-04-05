/*
*********************************************************************************************************
*	                                  
*	模块名称 : 示波器主界面上的幅值窗口
*	文件名称 : DSO_AmplititudeDlg.c
*	版    本 : V1.0
*	说    明 : 显示示波器当前的幅值。
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
static GUI_MEMDEV_Handle   hMemAmpWindow;


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


/*
*********************************************************************************************************
*	                   GUI_WIDGET_CREATE_INFO类型数组
*********************************************************************************************************
*/
static const GUI_WIDGET_CREATE_INFO _aDialogCreateAmp[] = 
{
    { WINDOW_CreateIndirect,     "",      0,              8,   648,  140, 66, 0},
	
	{ TEXT_CreateIndirect,      "Amp",    ID_TEXT_0,  45,  0, 35, 16, 0, 0},
	{ TEXT_CreateIndirect,      "1.00V",  ID_TEXT_1,  80,  0, 50, 16, 0, 0},
	
	{ TEXT_CreateIndirect,      "Amp",    ID_TEXT_2,  45,  16, 35, 16, 0, 0},
	{ TEXT_CreateIndirect,      "1.00V",  ID_TEXT_3,  80,  16, 50, 16, 0, 0},
	
	{ TEXT_CreateIndirect,     "2Msps  500ns",  ID_TEXT_4, 45, 32,  100, 16, 0, 0},
	{ TEXT_CreateIndirect,     "2Msps  500ns",  ID_TEXT_5, 45, 48,  100, 16, 0, 0},
};

/*
*********************************************************************************************************
*	函 数 名: PaintDialogAmp
*	功能说明: 幅值窗口重绘消息中的函数
*	形    参：pMsg  指针地址      	
*	返 回 值: 无
*********************************************************************************************************
*/
static void PaintDialogAmp(WM_MESSAGE * pMsg)
{
//	GUI_SetColor(0x888888);
//	GUI_DrawRect(DSOSCREEN_STARTX - 1,  /* 左上角X轴位置 */
//			     DSOSCREEN_STARTY - 1,  /* 左上角Y轴位置 */
//	             DSOSCREEN_ENDX + 1,    /* 右下角X轴位置 */
//	             DSOSCREEN_ENDY + 1);   /* 右下角Y轴位置 */

//	GUI_SetColor(0x434343);
	/* 清背景色 */
	GUI_SetBkColorIndex(10);
  	GUI_Clear();
	
	/* 绘制填充的抗锯齿圆角矩形 */
	GUI_SetColor(GUI_DARKGRAY);
	GUI_AA_FillRoundedRect(0, 0, 139, 65, 10);
	
	/* 绘制抗锯齿圆角矩形 */
	GUI_SetColor(0x434343);
	GUI_SetPenSize(2);
	GUI_AA_DrawRoundedRect(0, 0, 139, 65, 10);
	
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(GUI_FREQ);
	GUI_AA_FillRoundedRect(10, 4, 35, 16, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('A', 20,  4);

	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(GUI_FREQ);
	GUI_AA_FillRoundedRect(10, 19, 35, 31, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('O', 20, 19);
	
	////////////////
	/* 绘制抗锯齿圆角矩形，并填数值1 */
	GUI_SetColor(GUI_FREQ);
	GUI_AA_FillRoundedRect(10, 34, 35, 46, 3);
	
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('C', 20,  34);
	
	/* 绘制抗锯齿圆角矩形，并填数值2 */
	GUI_SetColor(GUI_FREQ);
	GUI_AA_FillRoundedRect(10, 49, 35, 61, 3);
						   
	GUI_SetColor(GUI_BLACK);
    GUI_SetFont(GUI_FONT_13_1);
	GUI_SetTextMode(GUI_TEXTMODE_TRANS);					
	GUI_DispCharAt('V', 20, 49);
}

/*
*********************************************************************************************************
*	函 数 名: PaintDialogAmp
*	功能说明: 幅值窗口初始化的消息处理
*	形    参：pMsg  指针地址      	
*	返 回 值: 无
*********************************************************************************************************
*/
static void InitDialogAmp(WM_MESSAGE * pMsg)
{
	WM_HWIN hWin = pMsg->hWin;

	//
	// 初始化文本控件
	//
	TEXT_SetTextColor(WM_GetDialogItem(hWin, ID_TEXT_0), GUI_FREQ);
	TEXT_SetFont(WM_GetDialogItem(hWin, ID_TEXT_0), &GUI_Font16_1);

	TEXT_SetTextColor(WM_GetDialogItem(hWin, ID_TEXT_1), GUI_FREQ);
	TEXT_SetFont(WM_GetDialogItem(hWin, ID_TEXT_1), &GUI_Font16_1);
	
	TEXT_SetTextColor(WM_GetDialogItem(hWin, ID_TEXT_2), GUI_FREQ);
	TEXT_SetFont(WM_GetDialogItem(hWin, ID_TEXT_2), &GUI_Font16_1);

	TEXT_SetTextColor(WM_GetDialogItem(hWin, ID_TEXT_3), GUI_FREQ);
	TEXT_SetFont(WM_GetDialogItem(hWin, ID_TEXT_3), &GUI_Font16_1);
	
	TEXT_SetTextColor(WM_GetDialogItem(hWin, ID_TEXT_4), GUI_FREQ);
	TEXT_SetFont(WM_GetDialogItem(hWin, ID_TEXT_4), &GUI_Font16_1);

	TEXT_SetTextColor(WM_GetDialogItem(hWin, ID_TEXT_5), GUI_FREQ);
	TEXT_SetFont(WM_GetDialogItem(hWin, ID_TEXT_5), &GUI_Font16_1);
}

/*
*********************************************************************************************************
*	函 数 名: _cbCallbackAmp()
*	功能说明: 幅值窗口回调函数
*	形    参: pMsg  指针地址         	
*	返 回 值: 无
*********************************************************************************************************
*/
static void _cbCallbackAmp(WM_MESSAGE * pMsg) 
{
    WM_HWIN hWin = pMsg->hWin;
	
    switch (pMsg->MsgId) 
    {
        case WM_TextUpDate:
			TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_1), g_AttText[Ch1AmpId]);
			TEXT_SetText(WM_GetDialogItem(hWin, ID_TEXT_3), g_AttText[Ch1AmpId]);
			break;
			
		case WM_PAINT:
			//GUI_MEMDEV_WriteAt(hMemAmpWindow, 2, 544);
			PaintDialogAmp(pMsg);
            break;
		
        case WM_INIT_DIALOG:
            InitDialogAmp(pMsg);
            break;
		
        default:
            WM_DefaultProc(pMsg);
    }
}

/*
*********************************************************************************************************
*	函 数 名: CreateWindowAmplitude
*	功能说明: 创建幅值窗口
*	形    参: 无         	
*	返 回 值: 所创建窗口的句柄
*********************************************************************************************************
*/
WM_HWIN CreateAmplitudeDlg(void) 
{
	WM_HWIN hWin;

	hWin = GUI_CreateDialogBox(_aDialogCreateAmp, 
	                           GUI_COUNTOF(_aDialogCreateAmp), 
	                           &_cbCallbackAmp, 
	                           0, 
	                           0, 
	                           0);
	
	return hWin;
}

/*
*********************************************************************************************************
*	函 数 名: DrawWinAmpBk
*	功能说明: 将窗口背景绘制到存储设备里面
*	形    参: 无         	
*	返 回 值: 无
*********************************************************************************************************
*/
void DrawWinAmpBk(void) 
{
	hMemAmpWindow = GUI_MEMDEV_CreateFixed(0, 
										   0, 
									       135, 
									       35, 
									       GUI_MEMDEV_HASTRANS, 
										   GUI_MEMDEV_APILIST_16, 
										   GUICC_M565);
	GUI_MEMDEV_Select(hMemAmpWindow);
	PaintDialogAmp(NULL);
	GUI_MEMDEV_Select(0);
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/

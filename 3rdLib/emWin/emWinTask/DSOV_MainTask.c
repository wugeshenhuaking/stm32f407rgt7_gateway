/*
*********************************************************************************************************
*	                                  
*	模块名称 : OLED界面
*	文件名称 : MainTask.c
*	版    本 : V1.0
*	说    明 : OLED界面
*              
*	修改记录 :
*		版本号   日期         作者          说明
*		V1.0    2019-07-10   Eric2013  	    首版    
*                                     
*	Copyright (C), 2018-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "includes.h"
#include "MainTask.h"



/*
*********************************************************************************************************
*	                                     变量
*********************************************************************************************************
*/
static GRAPH_SCALE_Handle hScaleV;     
static GRAPH_DATA_Handle  ahData;
static GRAPH_DATA_Handle  ahData1;

/*
*********************************************************************************************************
*	                                     宏定义
*********************************************************************************************************
*/
#define ID_FRAMEWIN_0 (GUI_ID_USER + 0x00)
#define ID_GRAPH_0    (GUI_ID_USER + 0x01)

#define ID_TEXT_0  (GUI_ID_USER + 0x02)
#define ID_TEXT_1  (GUI_ID_USER + 0x03)
#define ID_TEXT_2  (GUI_ID_USER + 0x04)
#define ID_TEXT_3  (GUI_ID_USER + 0x05)

#define ID_TEXT_4  (GUI_ID_USER + 0x06)
#define ID_TEXT_5  (GUI_ID_USER + 0x07)

/* 波形显示区左侧边上波形显示的参考位置 */
const GUI_POINT aPointsA[3] = 
{
	{  0,  0},
	{ 10, 10},
	{ 0,  20},
};

/*
*********************************************************************************************************
*                               对话框GUI_WIDGET_CREATE_INFO类型数组
*********************************************************************************************************
*/ 
static const GUI_WIDGET_CREATE_INFO _aDialogCreate1[] = {
    { WINDOW_CreateIndirect,  "Caption",           0,     0,  0,  240,240,0,0},
	{ GRAPH_CreateIndirect, "Graph", ID_GRAPH_0, 20, 20, 200, 200, 0, 0x0, 0 },
	
  { TEXT_CreateIndirect, "DSO3.0",      ID_TEXT_0, 2, 2, 50, 16, 0, 0x0, 0 },
  { TEXT_CreateIndirect, "3.6Msps",     ID_TEXT_1, 54, 2, 55, 16, 0, 0x0, 0 },
  { TEXT_CreateIndirect, "AC1:1 13.6V", ID_TEXT_2, 110, 2, 76, 16, 0, 0x64, 0 },
  { TEXT_CreateIndirect, "AdjFreq",     ID_TEXT_3, 189, 2, 50, 20, 0, 0x0, 0 },
  
  { TEXT_CreateIndirect, "RMS 00.000V", ID_TEXT_4, 20, 222, 90, 16, 0, 0x0, 0 },
  { TEXT_CreateIndirect, "RMS 00.000V", ID_TEXT_5, 130, 222, 90, 16, 0, 0x0, 0 },
};

/*
*********************************************************************************************************
*	函 数 名: _cbCallback1
*	功能说明: 回调函数
*	形    参: pMsg 指针参数            
*	返 回 值: 无
*********************************************************************************************************
*/
static void _cbCallback1(WM_MESSAGE * pMsg) 
{
    int NCode, Id;
    WM_HWIN hWin = pMsg->hWin;
	WM_HWIN hItem;
	int i;
	
    switch (pMsg->MsgId) 
    {
		
        case WM_INIT_DIALOG:
			//
			// Initialization of 'DSO3.0'
			//
			hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_0);
			TEXT_SetFont(hItem, GUI_FONT_16_1);
			TEXT_SetTextColor(hItem, GUI_WHITE);
			//
			// Initialization of '3.6Msps'
			//
			hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_1);
			TEXT_SetFont(hItem, GUI_FONT_16_1);
					TEXT_SetTextColor(hItem, GUI_WHITE);
			//
			// Initialization of 'AC1:1 13.6V'
			//
			hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_2);
			TEXT_SetFont(hItem, GUI_FONT_16_1);
					TEXT_SetTextColor(hItem, GUI_WHITE);
			//
			// Initialization of 'AdjFreq'
			//
			hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_3);
			TEXT_SetFont(hItem, GUI_FONT_16_1);
			TEXT_SetTextColor(hItem, GUI_WHITE);
						
			hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_4);
			TEXT_SetFont(hItem, GUI_FONT_16_1);
			TEXT_SetTextColor(hItem, GUI_GREEN);
			
			hItem = WM_GetDialogItem(pMsg->hWin, ID_TEXT_5);
			TEXT_SetFont(hItem, GUI_FONT_16_1);
			TEXT_SetTextColor(hItem, GUI_YELLOW);
			
			//
			// 初始化Graph控件
			//
			hItem = WM_GetDialogItem(pMsg->hWin, ID_GRAPH_0);
		
			GRAPH_SetLineStyleH(hItem, GUI_LS_DOT);
			GRAPH_SetLineStyleV(hItem, GUI_LS_DOT);
		
//			GRAPH_SetColor(hItem, GUI_WHITE, GRAPH_CI_GRID);
			GRAPH_SetColor(hItem, 0xFF9ECDEB, GRAPH_CI_FRAME);
		
			/* 创建数据对象 *******************************************************/
			ahData = GRAPH_DATA_YT_Create(GUI_YELLOW, 240, 0, 0);

			/* 数据对象添加到图形控件 */
			GRAPH_AttachData(hItem, ahData);
		
			/* 创建数据对象 *******************************************************/
			ahData1 = GRAPH_DATA_YT_Create(GUI_GREEN, 240, 0, 0);

			/* 数据对象添加到图形控件 */
			GRAPH_AttachData(hItem, ahData1);

			/* 设置Y轴方向的栅格间距 */
			GRAPH_SetGridDistY(hItem, 50);

			/* 固定X轴方向的栅格 */
			GRAPH_SetGridFixedX(hItem, 1);

			/* 设置栅格可见 */
			GRAPH_SetGridVis(hItem, 1);

			/* 设置上下左右边界的大小 */
			GRAPH_SetBorder(hItem, 2, 2, 2, 2);
			
			WM_CreateTimer(hWin, 0, 100, 0);
            break;
		
		case WM_PAINT:
			GUI_SetBkColor(0xFFCC187C);
			GUI_Clear();
		
			GUI_SetColor(GUI_GREEN);
			GUI_FillPolygon(&aPointsA[0], GUI_COUNTOF(aPointsA), 8, 60);
		
			GUI_SetColor(GUI_YELLOW);
			GUI_FillPolygon(&aPointsA[0], GUI_COUNTOF(aPointsA), 8, 160);
			break;
				
		/* 定时器消息 */
		case WM_TIMER:
			for(i=0; i<200; i++)
			{
				GRAPH_DATA_YT_AddValue(ahData, rand()%50+25);
			}
			for(i=0; i<200; i++)
			{
				GRAPH_DATA_YT_AddValue(ahData1, rand()%50+125);
			}
			WM_RestartTimer(pMsg->Data.v, 50);
            break;
		
        case WM_NOTIFY_PARENT:
            Id = WM_GetId(pMsg->hWinSrc); 
            NCode = pMsg->Data.v;        
            switch (Id) 
            {
                case GUI_ID_OK:
                    if(NCode==WM_NOTIFICATION_RELEASED)
					{
						GUI_EndDialog(hWin, 0);					
					}
                    break;
					
                case GUI_ID_CANCEL:
                    if(NCode==WM_NOTIFICATION_RELEASED)
					{
						GUI_EndDialog(hWin, 0);	
					}
                    break;

            }
            break;
			
        default:
            WM_DefaultProc(pMsg);
		
    }
}

/*
*********************************************************************************************************
*	函 数 名: DrawWinSysBk
*	功能说明: 将窗口背景绘制到存储设备里面
*	形    参: pMsg  指针地址         	
*	返 回 值: 无
*********************************************************************************************************
*/
WM_HWIN CreateFramewin(void) 
{
	WM_HWIN hWin;
	hWin = GUI_CreateDialogBox(_aDialogCreate1, GUI_COUNTOF(_aDialogCreate1), &_cbCallback1, 0, 0, 0);
	
	return hWin;
}

// USER START (Optionally insert additional public code)
// USER END

/*************************** End of file ****************************/

/*********************************************************************
*                     SEGGER Microcontroller GmbH                    *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 1996 - 2026  SEGGER Microcontroller GmbH                *
*                                                                    *
*        Internet: www.segger.com    Support:  support@segger.com    *
*                                                                    *
**********************************************************************
----------------------------------------------------------------------
File        : ID_SCREEN_01_Slots.c
Purpose     : AppWizard managed file, function content could be changed
---------------------------END-OF-HEADER------------------------------
*/

#include "Application.h"
#include "../Generated/Resource.h"
#include "../Generated/ID_SCREEN_01.h"

/*** Begin of user code area ***/
#include "GUI.h"
#include "WM.h"

#define SCREEN01_BG_COLOR      0x1E1E1E
#define SCREEN01_PANEL_COLOR   0x262626
#define SCREEN01_PANEL_EDGE    0x6B6B6B
#define SCREEN01_WAVE_BG       0x000000
#define SCREEN01_GRID_MINOR    0x113434
#define SCREEN01_GRID_MAJOR    0x255B5B
#define SCREEN01_GRID_BORDER   0x9A9A9A

#define SCREEN01_TOP_X0        0
#define SCREEN01_TOP_Y0        0
#define SCREEN01_TOP_X1        799
#define SCREEN01_TOP_Y1        27

#define SCREEN01_MAIN_X0       8
#define SCREEN01_MAIN_Y0       30
#define SCREEN01_MAIN_X1       623
#define SCREEN01_MAIN_Y1       410

#define SCREEN01_WAVE_X0       26
#define SCREEN01_WAVE_Y0       38
#define SCREEN01_WAVE_X1       620
#define SCREEN01_WAVE_Y1       400
#define SCREEN01_WAVE_W        (SCREEN01_WAVE_X1 - SCREEN01_WAVE_X0 + 1)
#define SCREEN01_WAVE_H        (SCREEN01_WAVE_Y1 - SCREEN01_WAVE_Y0 + 1)

#define SCREEN01_RIGHT_X0      632
#define SCREEN01_RIGHT_Y0      30
#define SCREEN01_RIGHT_X1      747
#define SCREEN01_RIGHT_Y1      472

#define SCREEN01_BOTTOM_X0     8
#define SCREEN01_BOTTOM_Y0     418
#define SCREEN01_BOTTOM_X1     623
#define SCREEN01_BOTTOM_Y1     472

#define SCREEN01_GRID_DIV_X    10
#define SCREEN01_GRID_DIV_Y    8
#define SCREEN01_GRID_SUBDIV   5

static WM_CALLBACK * s_pfBoxCallback;

static void _DrawScopeStaticLayout(void) {
  int X;
  int Y;
  int XPos;
  int YPos;

  GUI_SetColor(SCREEN01_BG_COLOR);
  GUI_FillRect(0, 0, 799, 479);

  GUI_SetColor(0x181818);
  GUI_FillRect(SCREEN01_TOP_X0, SCREEN01_TOP_Y0, SCREEN01_TOP_X1, SCREEN01_TOP_Y1);
  GUI_SetColor(SCREEN01_PANEL_EDGE);
  GUI_DrawHLine(SCREEN01_TOP_Y1, SCREEN01_TOP_X0, SCREEN01_TOP_X1);

  GUI_SetColor(SCREEN01_PANEL_COLOR);
  GUI_FillRect(SCREEN01_MAIN_X0, SCREEN01_MAIN_Y0, SCREEN01_MAIN_X1, SCREEN01_MAIN_Y1);
  GUI_SetColor(SCREEN01_PANEL_EDGE);
  GUI_DrawRect(SCREEN01_MAIN_X0, SCREEN01_MAIN_Y0, SCREEN01_MAIN_X1, SCREEN01_MAIN_Y1);

  GUI_SetColor(SCREEN01_WAVE_BG);
  GUI_FillRect(SCREEN01_WAVE_X0, SCREEN01_WAVE_Y0, SCREEN01_WAVE_X1, SCREEN01_WAVE_Y1);

  GUI_SetColor(SCREEN01_GRID_MINOR);
  for (X = 1; X < SCREEN01_GRID_DIV_X * SCREEN01_GRID_SUBDIV; X++) {
    if ((X % SCREEN01_GRID_SUBDIV) != 0) {
      XPos = SCREEN01_WAVE_X0 + (SCREEN01_WAVE_W * X) / (SCREEN01_GRID_DIV_X * SCREEN01_GRID_SUBDIV);
      GUI_DrawVLine(XPos, SCREEN01_WAVE_Y0, SCREEN01_WAVE_Y1);
    }
  }
  for (Y = 1; Y < SCREEN01_GRID_DIV_Y * SCREEN01_GRID_SUBDIV; Y++) {
    if ((Y % SCREEN01_GRID_SUBDIV) != 0) {
      YPos = SCREEN01_WAVE_Y0 + (SCREEN01_WAVE_H * Y) / (SCREEN01_GRID_DIV_Y * SCREEN01_GRID_SUBDIV);
      GUI_DrawHLine(YPos, SCREEN01_WAVE_X0, SCREEN01_WAVE_X1);
    }
  }

  GUI_SetColor(SCREEN01_GRID_MAJOR);
  for (X = 0; X <= SCREEN01_GRID_DIV_X; X++) {
    XPos = SCREEN01_WAVE_X0 + (SCREEN01_WAVE_W * X) / SCREEN01_GRID_DIV_X;
    GUI_DrawVLine(XPos, SCREEN01_WAVE_Y0, SCREEN01_WAVE_Y1);
  }
  for (Y = 0; Y <= SCREEN01_GRID_DIV_Y; Y++) {
    YPos = SCREEN01_WAVE_Y0 + (SCREEN01_WAVE_H * Y) / SCREEN01_GRID_DIV_Y;
    GUI_DrawHLine(YPos, SCREEN01_WAVE_X0, SCREEN01_WAVE_X1);
  }

  GUI_SetColor(SCREEN01_GRID_BORDER);
  GUI_DrawRect(SCREEN01_WAVE_X0, SCREEN01_WAVE_Y0, SCREEN01_WAVE_X1, SCREEN01_WAVE_Y1);

  GUI_SetColor(SCREEN01_PANEL_COLOR);
  GUI_FillRect(SCREEN01_RIGHT_X0, SCREEN01_RIGHT_Y0, SCREEN01_RIGHT_X1, SCREEN01_RIGHT_Y1);
  GUI_SetColor(SCREEN01_PANEL_EDGE);
  GUI_DrawRect(SCREEN01_RIGHT_X0, SCREEN01_RIGHT_Y0, SCREEN01_RIGHT_X1, SCREEN01_RIGHT_Y1);

  GUI_SetColor(SCREEN01_PANEL_COLOR);
  GUI_FillRect(SCREEN01_BOTTOM_X0, SCREEN01_BOTTOM_Y0, SCREEN01_BOTTOM_X1, SCREEN01_BOTTOM_Y1);
  GUI_SetColor(SCREEN01_PANEL_EDGE);
  GUI_DrawRect(SCREEN01_BOTTOM_X0, SCREEN01_BOTTOM_Y0, SCREEN01_BOTTOM_X1, SCREEN01_BOTTOM_Y1);
}

static void _cbScreen01Box(WM_MESSAGE * pMsg) {
  switch (pMsg->MsgId) {
  case WM_PAINT:
    if (s_pfBoxCallback && (s_pfBoxCallback != _cbScreen01Box)) {
      s_pfBoxCallback(pMsg);
    } else {
      WM_DefaultProc(pMsg);
    }
    _DrawScopeStaticLayout();
    break;
  default:
    if (s_pfBoxCallback && (s_pfBoxCallback != _cbScreen01Box)) {
      s_pfBoxCallback(pMsg);
    } else {
      WM_DefaultProc(pMsg);
    }
    break;
  }
}
/*** End of user code area ***/

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       cbID_SCREEN_01
*/
void cbID_SCREEN_01(WM_MESSAGE * pMsg) {
  WM_HWIN hBox;

  switch (pMsg->MsgId) {
  case WM_INIT_DIALOG:
    hBox = WM_GetDialogItem(pMsg->hWin, ID_BOX_00);
    if (hBox && (WM_GetCallback(hBox) != _cbScreen01Box)) {
      s_pfBoxCallback = WM_SetCallback(hBox, _cbScreen01Box);
      WM_InvalidateWindow(hBox);
    }
    break;
  default:
    GUI_USE_PARA(pMsg);
    break;
  }
}

void SCREEN01_ScopeExec(void) {
}

void SCREEN01_ScopeUseDemoData(int OnOff) {
  GUI_USE_PARA(OnOff);
}

void SCREEN01_ScopeSetChannelSamples(int Channel, const short * pSamples, unsigned NumSamples) {
  GUI_USE_PARA(Channel);
  GUI_USE_PARA(pSamples);
  GUI_USE_PARA(NumSamples);
}

void SCREEN01_ScopePushSample(short Ch1Sample, short Ch2Sample) {
  GUI_USE_PARA(Ch1Sample);
  GUI_USE_PARA(Ch2Sample);
}

/*********************************************************************
*
*       ID_SCREEN_01__ID_BUTTON_00__WM_NOTIFICATION_CLICKED
*/
void ID_SCREEN_01__ID_BUTTON_00__WM_NOTIFICATION_CLICKED(APPW_ACTION_ITEM * pAction, WM_HWIN hScreen, WM_MESSAGE * pMsg, int * pResult) {
  GUI_USE_PARA(pAction);
  GUI_USE_PARA(hScreen);
  GUI_USE_PARA(pMsg);
  GUI_USE_PARA(pResult);
}

/*************************** End of file ****************************/
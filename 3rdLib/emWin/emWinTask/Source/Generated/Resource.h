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
File        : Resource.h
Purpose     : Generated file do NOT edit!
---------------------------END-OF-HEADER------------------------------
*/

#ifndef RESOURCE_H
#define RESOURCE_H

#include "AppWizard.h"

/*********************************************************************
*
*       Text
*/
#define ID_RTEXT_0 0
#define ID_RTEXT_1 1

#define APPW_MANAGE_TEXT APPW_MANAGE_TEXT_EXT
extern GUI_CONST_STORAGE unsigned char acAPPW_Language_0[];

/*********************************************************************
*
*       Fonts
*/
extern GUI_CONST_STORAGE unsigned char ac_24_Normal_EXT_AA4[];

/*********************************************************************
*
*       Images
*/
extern GUI_CONST_STORAGE unsigned char acbk[];
extern GUI_CONST_STORAGE unsigned char acnet[];
extern GUI_CONST_STORAGE unsigned char acDARK_Button_Up_100x30[];
extern GUI_CONST_STORAGE unsigned char acDARK_Button_Down_100x30[];

/*********************************************************************
*
*       Screens
*/
#define ID_SCREEN_00 (GUI_ID_USER + 4096)
#define ID_SCREEN_01 (GUI_ID_USER + 4097)

extern APPW_ROOT_INFO ID_SCREEN_00_RootInfo;
extern APPW_ROOT_INFO ID_SCREEN_01_RootInfo;

#define APPW_INITIAL_SCREEN &ID_SCREEN_00_RootInfo

/*********************************************************************
*
*       Project path
*/
#define APPW_PROJECT_PATH "D:/Code_Project/emWin_AppWizard_Proj/V636/gateway"

#endif  // RESOURCE_H

/*************************** End of file ****************************/

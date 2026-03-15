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
File        : Resource.c
Purpose     : Generated file do NOT edit!
---------------------------END-OF-HEADER------------------------------
*/

#include "Resource.h"

/*********************************************************************
*
*       Defines
*
**********************************************************************
*/
#define _aVarList        NULL
#define _appDrawing      NULL
#define _NumDrawings     0
#define _CreateFlags     0

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
/*********************************************************************
*
*       _apRootList
*/
static APPW_ROOT_INFO * _apRootList[] = {
  &ID_SCREEN_00_RootInfo,
};

/*********************************************************************
*
*       _NumScreens
*/
static unsigned _NumScreens = GUI_COUNTOF(_apRootList);

/*********************************************************************
*
*       _NumVars
*/
static unsigned _NumVars = 0;

/*********************************************************************
*
*       _aScrollerList
*/
static GUI_CONST_STORAGE APPW_SCROLLER_DEF _aScrollerList[] = {
  { { 0x50606060ul, 0x00000000ul },
    { 200, 200, 800 },
    { 6, 24, 3, -3, 20 },
    0
  }
};

/*********************************************************************
*
*       _NumScrollers
*/
static unsigned _NumScrollers = GUI_COUNTOF(_aScrollerList);

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       APPW__GetTextInit
*/
void APPW__GetTextInit(GUI_CONST_STORAGE APPW_TEXT_INIT ** ppTextInit) {
  *ppTextInit = NULL;
}

/*********************************************************************
*
*       APPW__GetResource
*/
void APPW__GetResource(APPW_ROOT_INFO         *** pppRootInfo,    int * pNumScreens,
                       APPW_VAR_OBJECT         ** ppaVarList,     int * pNumVars,
                       const APPW_SCROLLER_DEF ** ppaScrollerDef, int * pNumScrollers,
                       APPW_DRAWING_ITEM      *** pppDrawingList, int * pNumDrawings,
                                                                  int * pCreateFlags) {
  *pppRootInfo    = _apRootList;
  *ppaVarList     = _aVarList;
  *ppaScrollerDef = _aScrollerList;
  *pppDrawingList = (APPW_DRAWING_ITEM **)_appDrawing;
  *pNumScreens    = _NumScreens;
  *pNumVars       = _NumVars;
  *pNumScrollers  = _NumScrollers;
  *pNumDrawings   = _NumDrawings;
  *pCreateFlags   = _CreateFlags;
}

/*************************** End of file ****************************/

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
File        : ID_SCREEN_00.c
Purpose     : Generated file do NOT edit!
---------------------------END-OF-HEADER------------------------------
*/

#include "Resource.h"
#include "ID_SCREEN_00.h"

/*********************************************************************
*
*       Static data
*
**********************************************************************
*/
/*********************************************************************
*
*       _aCreate
*/
static APPW_CREATE_ITEM _aCreate[] = {
  { WM_OBJECT_WINDOW_Create,
    ID_SCREEN_00, 0,
    { { { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
      },
      0, 0, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_IMAGE_Create,
    ID_IMAGE_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      800, 480, 0, 0, 0, 0
    },
    { 0, 0 }
  },
  { WM_OBJECT_BUTTON_Create,
    ID_BUTTON_00, ID_SCREEN_00,
    { { { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_REL_PARENT, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
        { DISPOSE_MODE_NULL, 0, 0, 0 },
      },
      108, 115, 0, 0, 0, 0
    },
    { 0, 0 }
  },
};

/*********************************************************************
*
*       _aSetup
*/
static GUI_CONST_STORAGE APPW_SETUP_ITEM _aSetup[] = {
  { ID_SCREEN_00, APPW_SET_PROP_MOTIONH,      { ARG_VP(0, NULL),
                                                ARG_VP(0, NULL),
                                                ARG_V(300) } },
  { ID_IMAGE_00,  APPW_SET_PROP_TILE,         { ARG_V(0) } },
  { ID_IMAGE_00,  APPW_SET_PROP_SBITMAP,      { ARG_VP(0, acbk),
                                                ARG_V(16903), } },
  { ID_BUTTON_00, APPW_SET_PROP_SBITMAPS,     { ARG_VP(0, acnet), } },
  { ID_BUTTON_00, APPW_SET_PROP_COLORS,       { ARG_V(GUI_RED),
                                                ARG_V(GUI_INVALID_COLOR),
                                                ARG_V(GUI_INVALID_COLOR) } },
  { ID_BUTTON_00, APPW_SET_PROP_BKCOLORS,     { ARG_V(GUI_INVALID_COLOR),
                                                ARG_V(GUI_INVALID_COLOR),
                                                ARG_V(GUI_INVALID_COLOR) } },
  { ID_BUTTON_00, APPW_SET_PROP_FOCUSABLE,    { ARG_V(2) } },
  { ID_BUTTON_00, APPW_SET_PROP_TEXTID,       { ARG_V(ID_RTEXT_0) } },
  { ID_BUTTON_00, APPW_SET_PROP_FONT,         { ARG_VP(0, ac_24_Normal_EXT_AA4) } },
  { ID_BUTTON_00, APPW_SET_PROP_RADIUS,       { ARG_V(0) } },
  { ID_BUTTON_00, APPW_SET_PROP_FRAME,        { ARG_V(1) } },
  { ID_BUTTON_00, APPW_SET_PROP_COLOR,        { ARG_V(GUI_INVALID_COLOR) } },
  { ID_BUTTON_00, APPW_SET_PROP_ALIGNTEXT,    { ARG_V(GUI_ALIGN_HCENTER | GUI_ALIGN_BOTTOM),
                                                ARG_V(0),
                                                ARG_V(0) } },
  { ID_BUTTON_00, APPW_SET_PROP_ALIGNBITMAP,  { ARG_V(GUI_ALIGN_HCENTER | GUI_ALIGN_VCENTER),
                                                ARG_V(0),
                                                ARG_V(0) } },
  { ID_BUTTON_00, APPW_SET_PROP_COLORS2,      { ARG_V(GUI_WHITE),
                                                ARG_V(GUI_WHITE),
                                                ARG_V(GUI_WHITE) } },
};

/*********************************************************************
*
*       _aAction
*/
static GUI_CONST_STORAGE APPW_ACTION_ITEM _aAction[] = {
  { ID_BUTTON_00, WM_NOTIFICATION_CLICKED,          0,            APPW_JOB_SHOWSCREEN,     ID_SCREEN_00__ID_BUTTON_00__WM_NOTIFICATION_CLICKED,
    { ARG_V(ID_SCREEN_01),
    }, 0, NULL
  },
};

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
/*********************************************************************
*
*       ID_SCREEN_00_RootInfo
*/
APPW_ROOT_INFO ID_SCREEN_00_RootInfo = {
  ID_SCREEN_00,
  _aCreate, GUI_COUNTOF(_aCreate),
  _aSetup,  GUI_COUNTOF(_aSetup),
  _aAction, GUI_COUNTOF(_aAction),
  cbID_SCREEN_00,
  0
};

/*************************** End of file ****************************/

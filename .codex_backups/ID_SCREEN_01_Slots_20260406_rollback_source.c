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

#define SCREEN01_BG_COLOR            0x1E1E1E
#define SCREEN01_PANEL_COLOR         0x262626
#define SCREEN01_PANEL_EDGE          0x6B6B6B
#define SCREEN01_WAVE_BG             0x000000
#define SCREEN01_GRID_MINOR          0x113434
#define SCREEN01_GRID_MAJOR          0x255B5B
#define SCREEN01_GRID_BORDER         0x9A9A9A
#define SCREEN01_CURSOR_COLOR        0x1E56FF
#define SCREEN01_CH1_COLOR           0x20E7F2
#define SCREEN01_CH2_COLOR           0x25FF46
#define SCREEN01_TEXT_COLOR          0xF2F2F2
#define SCREEN01_MUTED_TEXT          0xBCBCBC
#define SCREEN01_BUTTON_FILL         0x505050
#define SCREEN01_BUTTON_EDGE         0x6E6E6E
#define SCREEN01_INFO_EDGE           0x777777
#define SCREEN01_ACCENT_YELLOW       0xF4F04A
#define SCREEN01_ACCENT_BLUE         0x1636FF

#define SCREEN01_TOP_Y0              0
#define SCREEN01_TOP_Y1              27

#define SCREEN01_SCOPE_WIN_X0        0
#define SCREEN01_SCOPE_WIN_Y0        30
#define SCREEN01_SCOPE_WIN_X1        623
#define SCREEN01_SCOPE_WIN_Y1        410
#define SCREEN01_SCOPE_WIN_W         (SCREEN01_SCOPE_WIN_X1 - SCREEN01_SCOPE_WIN_X0 + 1)
#define SCREEN01_SCOPE_WIN_H         (SCREEN01_SCOPE_WIN_Y1 - SCREEN01_SCOPE_WIN_Y0 + 1)

#define SCREEN01_MAIN_PANEL_X0       8
#define SCREEN01_MAIN_PANEL_Y0       0
#define SCREEN01_MAIN_PANEL_X1       623
#define SCREEN01_MAIN_PANEL_Y1       380

#define SCREEN01_WAVE_X0             26
#define SCREEN01_WAVE_Y0             8
#define SCREEN01_WAVE_X1             620
#define SCREEN01_WAVE_Y1             370
#define SCREEN01_WAVE_W              (SCREEN01_WAVE_X1 - SCREEN01_WAVE_X0 + 1)
#define SCREEN01_WAVE_H              (SCREEN01_WAVE_Y1 - SCREEN01_WAVE_Y0 + 1)

#define SCREEN01_RIGHT_X0            632
#define SCREEN01_RIGHT_Y0            30
#define SCREEN01_RIGHT_X1            747
#define SCREEN01_RIGHT_Y1            472

#define SCREEN01_BOTTOM_X0           8
#define SCREEN01_BOTTOM_Y0           418
#define SCREEN01_BOTTOM_X1           623
#define SCREEN01_BOTTOM_Y1           472

#define SCREEN01_GRID_DIV_X          10
#define SCREEN01_GRID_DIV_Y          8
#define SCREEN01_GRID_SUBDIV         5

#define SCREEN01_TAG_CENTER_X        6
#define SCREEN01_REF_LINE_X0         22
#define SCREEN01_REF_LINE_X1         (SCREEN01_WAVE_X0 - 2)
#define SCREEN01_CH_Y_MIN            (SCREEN01_WAVE_Y0 + 16)
#define SCREEN01_CH_Y_MAX            (SCREEN01_WAVE_Y1 - 16)
#define SCREEN01_ROW_HIT_HALF_H      18
#define SCREEN01_SCOPE_FRAME_MS      25U
#define SCREEN01_WAVE_SAMPLE_COUNT   SCREEN01_WAVE_W
#define SCREEN01_WAVE_SAMPLE_MAX     ((SCREEN01_WAVE_H / 2) - 8)
#define SCREEN01_DEMO_PUSH_STEP      3U

#define SCREEN01_DRAG_NONE           0
#define SCREEN01_DRAG_CH1            1
#define SCREEN01_DRAG_CH2            2

static WM_CALLBACK * s_pfBoxCallback;
static WM_HWIN       s_hScopePanelWin;
static WM_HWIN       s_hWaveTouchWin;

static int      s_Ch1CenterY = SCREEN01_WAVE_Y0 + 88;
static int      s_Ch2CenterY = SCREEN01_WAVE_Y0 + 228;
static int      s_DragChannel = SCREEN01_DRAG_NONE;
static int      s_DragOffsetY = 0;
static int      s_WaveTouchPressed = 0;
static int      s_DemoWaveEnabled = 1;
static int      s_ScopeDirty = 1;
static unsigned s_DemoSampleIndex = 0U;
static U32      s_LastScopeUpdateTime = 0U;
static short    s_aCh1Wave[SCREEN01_WAVE_SAMPLE_COUNT];
static short    s_aCh2Wave[SCREEN01_WAVE_SAMPLE_COUNT];

static const GUI_POINT _aTagPoints[] = {
  { 0, -10 },
  { 16, -10 },
  { 26, 0 },
  { 16, 10 },
  { 0, 10 },
};

static int _TriWave(int X, int Period) {
  int Half;

  if (Period <= 0) {
    return 0;
  }
  X %= Period;
  if (X < 0) {
    X += Period;
  }
  Half = Period / 2;
  if (X < Half) {
    return X;
  }
  return Period - X;
}

static int _Abs32(int Value) {
  if (Value < 0) {
    return -Value;
  }
  return Value;
}

static short _ClampSampleOffset(int Offset) {
  if (Offset < -SCREEN01_WAVE_SAMPLE_MAX) {
    Offset = -SCREEN01_WAVE_SAMPLE_MAX;
  }
  if (Offset > SCREEN01_WAVE_SAMPLE_MAX) {
    Offset = SCREEN01_WAVE_SAMPLE_MAX;
  }
  return (short)Offset;
}

static int _ClampWaveY(int Y) {
  if (Y < SCREEN01_WAVE_Y0 + 2) {
    return SCREEN01_WAVE_Y0 + 2;
  }
  if (Y > SCREEN01_WAVE_Y1 - 2) {
    return SCREEN01_WAVE_Y1 - 2;
  }
  return Y;
}

static int _SampleToWaveY(short SampleOffset, int CenterY) {
  return _ClampWaveY(CenterY - (int)SampleOffset);
}

static int _ClampChannelCenterY(int Y) {
  if (Y < SCREEN01_CH_Y_MIN) {
    return SCREEN01_CH_Y_MIN;
  }
  if (Y > SCREEN01_CH_Y_MAX) {
    return SCREEN01_CH_Y_MAX;
  }
  return Y;
}

static int _IsScopePanelAlive(void) {
  if ((s_hScopePanelWin != 0) && WM_IsWindow(s_hScopePanelWin)) {
    return 1;
  }
  s_hScopePanelWin = 0;
  return 0;
}

static void _InvalidateScopePanelRect(int x0, int y0, int x1, int y1) {
  GUI_RECT Rect;

  if (_IsScopePanelAlive() == 0) {
    return;
  }

  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 >= SCREEN01_SCOPE_WIN_W) {
    x1 = SCREEN01_SCOPE_WIN_W - 1;
  }
  if (y1 >= SCREEN01_SCOPE_WIN_H) {
    y1 = SCREEN01_SCOPE_WIN_H - 1;
  }
  if ((x0 > x1) || (y0 > y1)) {
    return;
  }

  Rect.x0 = x0;
  Rect.y0 = y0;
  Rect.x1 = x1;
  Rect.y1 = y1;
  WM_InvalidateRect(s_hScopePanelWin, &Rect);
}

static void _RequestScopeFullRedraw(void) {
  s_ScopeDirty = 1;
  _InvalidateScopePanelRect(0, 0, SCREEN01_SCOPE_WIN_W - 1, SCREEN01_SCOPE_WIN_H - 1);
}

static void _RequestScopeRedraw(void) {
  s_ScopeDirty = 1;
  _InvalidateScopePanelRect(0, SCREEN01_WAVE_Y0 - 12, SCREEN01_WAVE_X1, SCREEN01_WAVE_Y1 + 12);
}

static void _ClearWave(short * pWave) {
  int Index;

  if (pWave == 0) {
    return;
  }
  for (Index = 0; Index < SCREEN01_WAVE_SAMPLE_COUNT; Index++) {
    pWave[Index] = 0;
  }
}

static void _ShiftAppendWave(short * pWave, short Sample) {
  int Index;

  if (pWave == 0) {
    return;
  }
  for (Index = 0; Index < (SCREEN01_WAVE_SAMPLE_COUNT - 1); Index++) {
    pWave[Index] = pWave[Index + 1];
  }
  pWave[SCREEN01_WAVE_SAMPLE_COUNT - 1] = _ClampSampleOffset((int)Sample);
}

static short _BuildDemoWaveSample(unsigned SampleIndex, int Channel) {
  int MainTerm;
  int DetailTerm;
  int RippleTerm;
  int Offset;

  if (Channel == SCREEN01_DRAG_CH1) {
    MainTerm   = _TriWave((int)(SampleIndex * 3U), 192) - 96;
    DetailTerm = _TriWave((int)(SampleIndex * 11U + 17U), 48) - 24;
    RippleTerm = _TriWave((int)(SampleIndex * 23U + 9U), 20) - 10;
    Offset  = (MainTerm * 72) / 96;
    Offset += (DetailTerm * 18) / 24;
    Offset += (RippleTerm * 8) / 10;
  } else {
    MainTerm   = _TriWave((int)(SampleIndex * 4U + 31U), 224) - 112;
    DetailTerm = _TriWave((int)(SampleIndex * 9U + 7U), 60) - 30;
    RippleTerm = _TriWave((int)(SampleIndex * 21U + 13U), 24) - 12;
    Offset  = (MainTerm * 54) / 112;
    Offset += (DetailTerm * 20) / 30;
    Offset += (RippleTerm * 9) / 12;
  }

  return _ClampSampleOffset(Offset);
}

static void _BuildDemoWaveFrame(void) {
  int X;

  for (X = 0; X < SCREEN01_WAVE_SAMPLE_COUNT; X++) {
    s_aCh1Wave[X] = _BuildDemoWaveSample((unsigned)X, SCREEN01_DRAG_CH1);
    s_aCh2Wave[X] = _BuildDemoWaveSample((unsigned)X, SCREEN01_DRAG_CH2);
  }
  s_DemoSampleIndex = (unsigned)SCREEN01_WAVE_SAMPLE_COUNT;
}

static void _ResampleWave(short * pDst, const short * pSrc, unsigned NumSamples) {
  unsigned Index;
  unsigned X;
  long     Sum;
  long     Mean;
  int      Peak;

  _ClearWave(pDst);
  if ((pDst == 0) || (pSrc == 0) || (NumSamples == 0U)) {
    return;
  }

  Sum = 0;
  for (Index = 0; Index < NumSamples; Index++) {
    Sum += pSrc[Index];
  }
  Mean = Sum / (long)NumSamples;

  Peak = 0;
  for (Index = 0; Index < NumSamples; Index++) {
    int Offset;

    Offset = (int)((long)pSrc[Index] - Mean);
    if (_Abs32(Offset) > Peak) {
      Peak = _Abs32(Offset);
    }
  }
  if (Peak <= 0) {
    Peak = 1;
  }

  if (NumSamples == 1U) {
    pDst[0] = _ClampSampleOffset((int)(((long)pSrc[0] - Mean) * SCREEN01_WAVE_SAMPLE_MAX / Peak));
    return;
  }

  for (X = 0; X < SCREEN01_WAVE_SAMPLE_COUNT; X++) {
    unsigned SampleIndex;
    int      Offset;

    SampleIndex = (unsigned)(((U32)X * (U32)(NumSamples - 1U)) / (U32)(SCREEN01_WAVE_SAMPLE_COUNT - 1));
    Offset      = (int)((long)pSrc[SampleIndex] - Mean);
    Offset      = (Offset * SCREEN01_WAVE_SAMPLE_MAX) / Peak;
    pDst[X]     = _ClampSampleOffset(Offset);
  }
}

static void _InitWaveSamples(void) {
  _ClearWave(s_aCh1Wave);
  _ClearWave(s_aCh2Wave);
  _BuildDemoWaveFrame();
  s_LastScopeUpdateTime = (U32)GUI_GetTime();
  s_ScopeDirty = 1;
}

static int _HitWaveRowLocal(int X, int Y) {
  int Ch1LocalY;
  int Ch2LocalY;

  if ((X < 0) || (X >= SCREEN01_WAVE_W) || (Y < 0) || (Y >= SCREEN01_WAVE_H)) {
    return SCREEN01_DRAG_NONE;
  }

  Ch1LocalY = s_Ch1CenterY - SCREEN01_WAVE_Y0;
  Ch2LocalY = s_Ch2CenterY - SCREEN01_WAVE_Y0;

  if ((Y >= (Ch1LocalY - SCREEN01_ROW_HIT_HALF_H)) && (Y <= (Ch1LocalY + SCREEN01_ROW_HIT_HALF_H))) {
    return SCREEN01_DRAG_CH1;
  }
  if ((Y >= (Ch2LocalY - SCREEN01_ROW_HIT_HALF_H)) && (Y <= (Ch2LocalY + SCREEN01_ROW_HIT_HALF_H))) {
    return SCREEN01_DRAG_CH2;
  }
  return SCREEN01_DRAG_NONE;
}

static void _StartChannelDragLocal(const GUI_PID_STATE * pState) {
  int Ch1LocalY;
  int Ch2LocalY;

  s_DragChannel = _HitWaveRowLocal(pState->x, pState->y);
  Ch1LocalY = s_Ch1CenterY - SCREEN01_WAVE_Y0;
  Ch2LocalY = s_Ch2CenterY - SCREEN01_WAVE_Y0;

  if (s_DragChannel == SCREEN01_DRAG_CH1) {
    s_DragOffsetY = pState->y - Ch1LocalY;
  } else if (s_DragChannel == SCREEN01_DRAG_CH2) {
    s_DragOffsetY = pState->y - Ch2LocalY;
  } else {
    s_DragOffsetY = 0;
  }
}

static void _UpdateChannelDragLocal(const GUI_PID_STATE * pState) {
  int NewCenterY;

  if (s_DragChannel == SCREEN01_DRAG_NONE) {
    return;
  }

  NewCenterY = _ClampChannelCenterY(pState->y - s_DragOffsetY + SCREEN01_WAVE_Y0);
  if (s_DragChannel == SCREEN01_DRAG_CH1) {
    if (s_Ch1CenterY != NewCenterY) {
      s_Ch1CenterY = NewCenterY;
      _RequestScopeRedraw();
    }
  } else if (s_DragChannel == SCREEN01_DRAG_CH2) {
    if (s_Ch2CenterY != NewCenterY) {
      s_Ch2CenterY = NewCenterY;
      _RequestScopeRedraw();
    }
  }
}

static void _StopChannelDrag(void) {
  s_DragChannel = SCREEN01_DRAG_NONE;
  s_DragOffsetY = 0;
}

static int _RectIntersects(const GUI_RECT * pClipRect, int x0, int y0, int x1, int y1) {
  GUI_RECT Rect;

  Rect.x0 = x0;
  Rect.y0 = y0;
  Rect.x1 = x1;
  Rect.y1 = y1;

  return GUI_RectsIntersect(pClipRect, &Rect);
}

static void _FillRoundPanel(int x0, int y0, int x1, int y1, GUI_COLOR Fill, GUI_COLOR Edge, int Radius) {
  GUI_SetColor(Fill);
  GUI_FillRoundedRect(x0, y0, x1, y1, Radius);
  GUI_SetColor(Edge);
  GUI_DrawRoundedRect(x0, y0, x1, y1, Radius);
}

static void _DrawSideButton(int x0, int y0, const char * pText) {
  _FillRoundPanel(x0, y0, x0 + 50, y0 + 34, SCREEN01_BUTTON_FILL, SCREEN01_BUTTON_EDGE, 6);
  GUI_SetColor(SCREEN01_TEXT_COLOR);
  GUI_SetFont(GUI_FONT_16_1);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  GUI_DispStringHCenterAt(pText, x0 + 25, y0 + 8);
}

static void _DrawOptionItem(int x0, int y0, const char * pText, int Selected) {
  if (Selected) {
    GUI_SetColor(SCREEN01_TEXT_COLOR);
    GUI_FillCircle(x0 + 5, y0 + 6, 4);
    GUI_SetColor(SCREEN01_PANEL_COLOR);
    GUI_FillCircle(x0 + 5, y0 + 6, 1);
  } else {
    GUI_SetColor(SCREEN01_TEXT_COLOR);
    GUI_DrawCircle(x0 + 5, y0 + 6, 4);
  }
  GUI_SetColor(SCREEN01_TEXT_COLOR);
  GUI_SetFont(GUI_FONT_16_1);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  GUI_DispStringAt(pText, x0 + 14, y0 - 2);
}

static void _DrawCheckItem(int x0, int y0, GUI_COLOR Color, const char * pText) {
  GUI_SetColor(GUI_WHITE);
  GUI_FillRect(x0, y0, x0 + 17, y0 + 17);
  GUI_SetColor(0x6A6A6A);
  GUI_DrawRect(x0, y0, x0 + 17, y0 + 17);
  GUI_SetColor(Color);
  GUI_SetFont(GUI_FONT_20_1);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  GUI_DispStringAt(pText, x0 + 22, y0 - 2);
}

static void _DrawReferenceMarker(int YCenter, GUI_COLOR Color, const char * pText, int Selected) {
  GUI_SetColor(Color);
  GUI_DrawHLine(YCenter, SCREEN01_REF_LINE_X0, SCREEN01_REF_LINE_X1);
  GUI_FillPolygon(_aTagPoints, GUI_COUNTOF(_aTagPoints), SCREEN01_TAG_CENTER_X, YCenter);
  if (Selected) {
    GUI_SetColor(GUI_WHITE);
    GUI_DrawPolygon(_aTagPoints, GUI_COUNTOF(_aTagPoints), SCREEN01_TAG_CENTER_X, YCenter);
  }
  GUI_SetColor(0x111111);
  GUI_SetFont(GUI_FONT_16_1);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  GUI_DispStringAt(pText, 11, YCenter - 8);
}

static void _DrawStatusPanel(int x0, int y0, int x1, int y1, GUI_COLOR Accent, const char * pTitle) {
  _FillRoundPanel(x0, y0, x1, y1, 0x3A3A3A, SCREEN01_BUTTON_EDGE, 6);
  GUI_SetColor(Accent);
  GUI_SetFont(GUI_FONT_16_1);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  GUI_DispStringAt(pTitle, x0 + 8, y0 + 6);
}

static void _DrawBufferedWave(const short * pWave, int CenterY, GUI_COLOR Color) {
  GUI_RECT ClipRect;
  int      X;
  int      PrevX;
  int      PrevY;
  int      CurY;

  if (pWave == 0) {
    return;
  }

  ClipRect.x0 = SCREEN01_WAVE_X0 + 1;
  ClipRect.y0 = SCREEN01_WAVE_Y0 + 1;
  ClipRect.x1 = SCREEN01_WAVE_X1 - 1;
  ClipRect.y1 = SCREEN01_WAVE_Y1 - 1;
  GUI_SetClipRect(&ClipRect);

  GUI_SetColor(Color);
  PrevX = SCREEN01_WAVE_X0;
  PrevY = _SampleToWaveY(pWave[0], CenterY);
  for (X = 1; X < SCREEN01_WAVE_SAMPLE_COUNT; X++) {
    CurY = _SampleToWaveY(pWave[X], CenterY);
    GUI_DrawLine(PrevX, PrevY, SCREEN01_WAVE_X0 + X, CurY);
    PrevX = SCREEN01_WAVE_X0 + X;
    PrevY = CurY;
  }

  GUI_SetClipRect(NULL);
}

static void _DrawScopePanelContent(void) {
  int X;
  int Y;
  int XPos;
  int YPos;
  int CursorAX;
  int CursorBX;

  GUI_SetColor(SCREEN01_PANEL_COLOR);
  GUI_FillRect(0, 0, SCREEN01_SCOPE_WIN_W - 1, SCREEN01_SCOPE_WIN_H - 1);
  GUI_FillRect(SCREEN01_MAIN_PANEL_X0, SCREEN01_MAIN_PANEL_Y0, SCREEN01_MAIN_PANEL_X1, SCREEN01_MAIN_PANEL_Y1);
  GUI_SetColor(SCREEN01_PANEL_EDGE);
  GUI_DrawRect(SCREEN01_MAIN_PANEL_X0, SCREEN01_MAIN_PANEL_Y0, SCREEN01_MAIN_PANEL_X1, SCREEN01_MAIN_PANEL_Y1);

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

  CursorAX = SCREEN01_WAVE_X0 + 95;
  CursorBX = SCREEN01_WAVE_X0 + 270;
  GUI_SetColor(SCREEN01_CURSOR_COLOR);
  GUI_DrawVLine(CursorAX, SCREEN01_WAVE_Y0, SCREEN01_WAVE_Y1);
  GUI_DrawVLine(CursorBX, SCREEN01_WAVE_Y0, SCREEN01_WAVE_Y1);
  GUI_SetFont(GUI_FONT_16_1);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  GUI_DispStringAt("a", CursorAX + 2, SCREEN01_WAVE_Y0 + 2);
  GUI_DispStringAt("b", CursorBX + 2, SCREEN01_WAVE_Y0 + 2);

  _DrawBufferedWave(s_aCh1Wave, s_Ch1CenterY, SCREEN01_CH1_COLOR);
  _DrawBufferedWave(s_aCh2Wave, s_Ch2CenterY, SCREEN01_CH2_COLOR);
  _DrawReferenceMarker(s_Ch1CenterY, SCREEN01_CH1_COLOR, "1", s_DragChannel == SCREEN01_DRAG_CH1);
  _DrawReferenceMarker(s_Ch2CenterY, SCREEN01_CH2_COLOR, "2", s_DragChannel == SCREEN01_DRAG_CH2);

  _FillRoundPanel(SCREEN01_WAVE_X1 - 120, SCREEN01_WAVE_Y0 + 10,
                  SCREEN01_WAVE_X1 - 10, SCREEN01_WAVE_Y0 + 58,
                  0x060606, SCREEN01_INFO_EDGE, 6);
  GUI_SetColor(SCREEN01_CH1_COLOR);
  GUI_SetFont(GUI_FONT_13_ASCII);
  GUI_DispStringAt("@ -75.0us    1.000V", SCREEN01_WAVE_X1 - 112, SCREEN01_WAVE_Y0 + 18);
  GUI_SetColor(SCREEN01_CH2_COLOR);
  GUI_DispStringAt("@  75.0us   -1.000V", SCREEN01_WAVE_X1 - 112, SCREEN01_WAVE_Y0 + 34);
}

static void _DrawTopBar(void) {
  GUI_SetColor(SCREEN01_BG_COLOR);
  GUI_FillRect(0, SCREEN01_TOP_Y0, 799, SCREEN01_TOP_Y1);

  GUI_SetColor(SCREEN01_TEXT_COLOR);
  GUI_SetFont(GUI_FONT_20_1);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  GUI_DispStringAt("DSO3.0", 8, 4);
  GUI_DispStringAt("Run", 74, 4);
  GUI_DispStringAt("Auto", 120, 4);

  GUI_SetColor(GUI_BLACK);
  GUI_FillRect(164, 3, 560, 24);
  GUI_SetColor(SCREEN01_INFO_EDGE);
  GUI_DrawRect(164, 3, 560, 24);
  GUI_SetColor(SCREEN01_CH1_COLOR);
  GUI_DrawHLine(14, 176, 545);
  GUI_FillCircle(171, 14, 1);

  GUI_SetColor(SCREEN01_ACCENT_YELLOW);
  GUI_DrawLine(567, 18, 575, 18);
  GUI_DrawLine(575, 18, 582, 5);
  GUI_DrawLine(582, 5, 592, 5);
  GUI_SetColor(SCREEN01_TEXT_COLOR);
  GUI_DispStringAt("1.0000V", 596, 4);
  GUI_DispStringAt("AdjFreq", 664, 4);

  _FillRoundPanel(718, 2, 742, 26, SCREEN01_ACCENT_BLUE, SCREEN01_ACCENT_BLUE, 12);
  GUI_SetColor(GUI_WHITE);
  GUI_DispStringHCenterAt("T", 730, 4);
}

static void _DrawRightPanel(void) {
  _FillRoundPanel(SCREEN01_RIGHT_X0, SCREEN01_RIGHT_Y0, SCREEN01_RIGHT_X1, SCREEN01_RIGHT_Y1,
                  SCREEN01_PANEL_COLOR, SCREEN01_PANEL_EDGE, 6);

  _DrawSideButton(638, 38,  "Measure");
  _DrawSideButton(694, 38,  "AD7606");
  _DrawSideButton(638, 80,  "ADS1256");
  _DrawSideButton(694, 80,  "Filter");
  _DrawSideButton(638, 122, "AD9XXX");
  _DrawSideButton(694, 122, "Settings");

  _DrawCheckItem(640, 174, SCREEN01_CH1_COLOR, "CH1");
  _DrawCheckItem(694, 174, SCREEN01_CH2_COLOR, "CH2");

  _DrawOptionItem(640, 222, "AdjFreq",       1);
  _DrawOptionItem(640, 246, "AdjAmplitude",  0);
  _DrawOptionItem(640, 270, "ChangeRefPos",  0);
  _DrawOptionItem(640, 294, "CursorA",       0);
  _DrawOptionItem(640, 318, "CursorB",       0);
  _DrawOptionItem(640, 342, "Trigger",       0);

  _DrawSideButton(650, 428, "FFT");
  _DrawSideButton(704, 428, "BUS");
}

static void _DrawBottomBar(void) {
  _DrawStatusPanel(8,   424, 112, 472, 0xB9F26A, "Scale");
  _DrawStatusPanel(120, 424, 367, 472, SCREEN01_CH1_COLOR, "CH1");
  _DrawStatusPanel(375, 424, 623, 472, SCREEN01_CH2_COLOR, "CH2");

  GUI_SetColor(SCREEN01_MUTED_TEXT);
  GUI_SetFont(GUI_FONT_13_ASCII);
  GUI_SetTextMode(GUI_TEXTMODE_TRANS);
  GUI_DispStringAt("Amp  1.00V", 18, 442);
  GUI_DispStringAt("Time 500ns", 18, 456);

  GUI_SetColor(SCREEN01_CH1_COLOR);
  GUI_DispStringAt("RMS  00.000V", 130, 438);
  GUI_DispStringAt("Min -00.000V", 130, 452);
  GUI_DispStringAt("Max  00.000V", 250, 438);
  GUI_DispStringAt("PkPk 00.000V", 250, 452);

  GUI_SetColor(SCREEN01_CH2_COLOR);
  GUI_DispStringAt("RMS  00.000V", 385, 438);
  GUI_DispStringAt("Min -00.000V", 385, 452);
  GUI_DispStringAt("Max  00.000V", 505, 438);
  GUI_DispStringAt("PkPk 00.000V", 505, 452);
}

static void _DrawScreen01Overlay(WM_HWIN hWin) {
  GUI_RECT InvalidRect;

  if (WM_GetInvalidRect(hWin, &InvalidRect) == 0) {
    InvalidRect.x0 = 0;
    InvalidRect.y0 = 0;
    InvalidRect.x1 = 799;
    InvalidRect.y1 = 479;
  }

  GUI_SetTextMode(GUI_TEXTMODE_TRANS);

  if (_RectIntersects(&InvalidRect, 0, 0, 799, SCREEN01_SCOPE_WIN_Y0 - 1)) {
    GUI_SetColor(SCREEN01_BG_COLOR);
    GUI_FillRect(0, 0, 799, SCREEN01_SCOPE_WIN_Y0 - 1);
    _DrawTopBar();
  }

  if (_RectIntersects(&InvalidRect,
                      SCREEN01_SCOPE_WIN_X1 + 1,
                      SCREEN01_SCOPE_WIN_Y0,
                      799,
                      SCREEN01_SCOPE_WIN_Y1)) {
    GUI_SetColor(SCREEN01_BG_COLOR);
    GUI_FillRect(SCREEN01_SCOPE_WIN_X1 + 1, SCREEN01_SCOPE_WIN_Y0, 799, SCREEN01_SCOPE_WIN_Y1);
    _DrawRightPanel();
  }

  if (_RectIntersects(&InvalidRect,
                      0,
                      SCREEN01_SCOPE_WIN_Y1 + 1,
                      799,
                      479)) {
    GUI_SetColor(SCREEN01_BG_COLOR);
    GUI_FillRect(0, SCREEN01_SCOPE_WIN_Y1 + 1, 799, 479);
    _DrawBottomBar();
  }

  GUI_USE_PARA(hWin);
}

static void _HandleWavePointerState(const GUI_PID_STATE * pState) {
  if ((pState != 0) && (pState->Pressed != 0)) {
    if (s_WaveTouchPressed == 0) {
      _StartChannelDragLocal(pState);
      s_WaveTouchPressed = 1;
      if (s_DragChannel != SCREEN01_DRAG_NONE) {
        _RequestScopeRedraw();
      }
    } else {
      _UpdateChannelDragLocal(pState);
    }
  } else if (s_WaveTouchPressed != 0) {
    s_WaveTouchPressed = 0;
    if (s_DragChannel != SCREEN01_DRAG_NONE) {
      _StopChannelDrag();
      _RequestScopeRedraw();
    }
  }
}

static void _cbScreen01WaveTouch(WM_MESSAGE * pMsg) {
  switch (pMsg->MsgId) {
  case WM_TOUCH:
    _HandleWavePointerState((const GUI_PID_STATE *)pMsg->Data.p);
    break;
  case WM_DELETE:
    s_hWaveTouchWin = 0;
    s_WaveTouchPressed = 0;
    _StopChannelDrag();
    break;
  default:
    WM_DefaultProc(pMsg);
    break;
  }
}

static void _cbScreen01ScopePanel(WM_MESSAGE * pMsg) {
  switch (pMsg->MsgId) {
  case WM_PAINT:
    _DrawScopePanelContent();
    s_ScopeDirty = 0;
    break;
  case WM_DELETE:
    s_hScopePanelWin = 0;
    s_hWaveTouchWin = 0;
    s_WaveTouchPressed = 0;
    _StopChannelDrag();
    break;
  default:
    WM_DefaultProc(pMsg);
    break;
  }
}

static void _cbScreen01Box(WM_MESSAGE * pMsg) {
  if (pMsg->MsgId == WM_PAINT) {
    _DrawScreen01Overlay(pMsg->hWin);
    return;
  }

  if (s_pfBoxCallback && (s_pfBoxCallback != _cbScreen01Box)) {
    s_pfBoxCallback(pMsg);
  } else {
    WM_DefaultProc(pMsg);
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
  WM_HWIN hWin;

  switch (pMsg->MsgId) {
  case WM_INIT_DIALOG:
    hWin = WM_GetDialogItem(pMsg->hWin, ID_BOX_00);
    if (hWin) {
      if (WM_GetCallback(hWin) != _cbScreen01Box) {
        s_pfBoxCallback = WM_SetCallback(hWin, _cbScreen01Box);
      }
      if (s_hWaveTouchWin && WM_IsWindow(s_hWaveTouchWin)) {
        WM_DeleteWindow(s_hWaveTouchWin);
      }
      s_hWaveTouchWin = 0;
      if (s_hScopePanelWin && WM_IsWindow(s_hScopePanelWin)) {
        WM_DeleteWindow(s_hScopePanelWin);
      }
      s_hScopePanelWin = 0;
      s_hScopePanelWin = WM_CreateWindowAsChild(SCREEN01_SCOPE_WIN_X0,
                                                SCREEN01_SCOPE_WIN_Y0,
                                                SCREEN01_SCOPE_WIN_W,
                                                SCREEN01_SCOPE_WIN_H,
                                                hWin,
                                                WM_CF_SHOW,
                                                _cbScreen01ScopePanel,
                                                0);
      s_hWaveTouchWin = WM_CreateWindowAsChild(SCREEN01_WAVE_X0,
                                               SCREEN01_WAVE_Y0,
                                               SCREEN01_WAVE_W,
                                               SCREEN01_WAVE_H,
                                               s_hScopePanelWin,
                                               WM_CF_SHOW | WM_CF_HASTRANS,
                                               _cbScreen01WaveTouch,
                                               0);
      _InitWaveSamples();
      WM_InvalidateWindow(hWin);
      _RequestScopeFullRedraw();
    }
    break;
  case WM_DELETE:
    s_pfBoxCallback = 0;
    s_hWaveTouchWin = 0;
    s_hScopePanelWin = 0;
    s_WaveTouchPressed = 0;
    _StopChannelDrag();
    break;
  default:
    GUI_USE_PARA(pMsg);
    break;
  }
}

void SCREEN01_ScopeExec(void) {
  U32      Now;
  unsigned Step;

  if (_IsScopePanelAlive() == 0) {
    return;
  }

  Now = (U32)GUI_GetTime();
  if (s_DemoWaveEnabled != 0) {
    if ((U32)(Now - s_LastScopeUpdateTime) < SCREEN01_SCOPE_FRAME_MS) {
      return;
    }
    s_LastScopeUpdateTime = Now;
    for (Step = 0; Step < SCREEN01_DEMO_PUSH_STEP; Step++) {
      _ShiftAppendWave(s_aCh1Wave, _BuildDemoWaveSample(s_DemoSampleIndex, SCREEN01_DRAG_CH1));
      _ShiftAppendWave(s_aCh2Wave, _BuildDemoWaveSample(s_DemoSampleIndex, SCREEN01_DRAG_CH2));
      s_DemoSampleIndex++;
    }
    _RequestScopeRedraw();
  } else if (s_ScopeDirty != 0) {
    _InvalidateScopePanelRect(0, 0, SCREEN01_SCOPE_WIN_W - 1, SCREEN01_SCOPE_WIN_H - 1);
  }
}

void SCREEN01_ScopeUseDemoData(int OnOff) {
  s_DemoWaveEnabled = (OnOff != 0) ? 1 : 0;
  if (s_DemoWaveEnabled != 0) {
    _InitWaveSamples();
  } else {
    _RequestScopeRedraw();
  }
}

void SCREEN01_ScopeSetChannelSamples(int Channel, const short * pSamples, unsigned NumSamples) {
  s_DemoWaveEnabled = 0;

  if ((Channel == 0) || (Channel == 1)) {
    _ResampleWave(s_aCh1Wave, pSamples, NumSamples);
  } else if (Channel == 2) {
    _ResampleWave(s_aCh2Wave, pSamples, NumSamples);
  } else {
    return;
  }

  _RequestScopeRedraw();
}

void SCREEN01_ScopePushSample(short Ch1Sample, short Ch2Sample) {
  s_DemoWaveEnabled = 0;
  _ShiftAppendWave(s_aCh1Wave, Ch1Sample);
  _ShiftAppendWave(s_aCh2Wave, Ch2Sample);
  _RequestScopeRedraw();
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
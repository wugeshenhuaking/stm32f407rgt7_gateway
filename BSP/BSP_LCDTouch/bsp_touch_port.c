/*
*********************************************************************************************************
*
*   Module : Touch panel BSP port layer
*   File   : bsp_touch_port.c
*   Notes  :
*       - Reads raw points from GT9xxx
*       - Converts them into logical screen coordinates
*       - Bridges single-touch events into emWin
*
*********************************************************************************************************
*/

#include <stdio.h>

#include "bsp_touch_port.h"
#include "bsp_timer.h"
#include "GUI.h"

#if BSP_TOUCH_EMWIN_DEBUG
  #define TOUCH_LOG(fmt, ...)  printf("[TOUCH] " fmt "\r\n", ##__VA_ARGS__)
#else
  #define TOUCH_LOG(...)       do { } while (0)
#endif

typedef enum
{
    TOUCH_DOWN = 0,
    TOUCH_MOVE,
    TOUCH_RELEASE
} TOUCH_EVENT_E;

static BSP_TouchStateTypeDef s_touch_state;
static uint16_t              s_raw_x_debug = 0;
static uint16_t              s_raw_y_debug = 0;
static GUI_PID_STATE         s_gui_pid_state;
static uint8_t               s_touch_ready = 0;
static uint8_t               s_touch_down  = 0;
static uint16_t              s_last_x      = 0;
static uint16_t              s_last_y      = 0;
static uint16_t              s_measure_x   = 0;
static uint16_t              s_measure_y   = 0;
static uint8_t               s_measure_ok  = 0;
static uint32_t              s_measure_tick = 0xFFFFFFFFUL;
static uint8_t               s_touch_task_timer_started = 0;

static uint16_t scale_raw_x_to_gui_width(uint16_t raw_x)
{
    if (BSP_TOUCH_RAW_WIDTH <= 1U)
    {
        return 0;
    }
    if (raw_x >= BSP_TOUCH_RAW_WIDTH)
    {
        raw_x = (uint16_t)(BSP_TOUCH_RAW_WIDTH - 1U);
    }
    return (uint16_t)(((uint32_t)raw_x * (uint32_t)(BSP_TOUCH_GUI_WIDTH - 1U)) /
                      (uint32_t)(BSP_TOUCH_RAW_WIDTH - 1U));
}

static uint16_t scale_raw_y_to_gui_height(uint16_t raw_y)
{
    if (BSP_TOUCH_RAW_HEIGHT <= 1U)
    {
        return 0;
    }
    if (raw_y >= BSP_TOUCH_RAW_HEIGHT)
    {
        raw_y = (uint16_t)(BSP_TOUCH_RAW_HEIGHT - 1U);
    }
    return (uint16_t)(((uint32_t)raw_y * (uint32_t)(BSP_TOUCH_GUI_HEIGHT - 1U)) /
                      (uint32_t)(BSP_TOUCH_RAW_HEIGHT - 1U));
}

static uint16_t scale_raw_x_to_gui_height(uint16_t raw_x)
{
    if (BSP_TOUCH_RAW_WIDTH <= 1U)
    {
        return 0;
    }
    if (raw_x >= BSP_TOUCH_RAW_WIDTH)
    {
        raw_x = (uint16_t)(BSP_TOUCH_RAW_WIDTH - 1U);
    }
    return (uint16_t)(((uint32_t)raw_x * (uint32_t)(BSP_TOUCH_GUI_HEIGHT - 1U)) /
                      (uint32_t)(BSP_TOUCH_RAW_WIDTH - 1U));
}

static uint16_t scale_raw_y_to_gui_width(uint16_t raw_y)
{
    if (BSP_TOUCH_RAW_HEIGHT <= 1U)
    {
        return 0;
    }
    if (raw_y >= BSP_TOUCH_RAW_HEIGHT)
    {
        raw_y = (uint16_t)(BSP_TOUCH_RAW_HEIGHT - 1U);
    }
    return (uint16_t)(((uint32_t)raw_y * (uint32_t)(BSP_TOUCH_GUI_WIDTH - 1U)) /
                      (uint32_t)(BSP_TOUCH_RAW_HEIGHT - 1U));
}

static void coord_convert(uint16_t raw_x, uint16_t raw_y,
                          uint16_t *out_x, uint16_t *out_y)
{
    uint16_t x;
    uint16_t y;

    /*
     * Touch raw coordinates always come from the physical panel space
     * (approximately 480 x 800, top-left origin).
     *
     * emWin logical coordinates depend on the currently selected LCD
     * orientation. The display direction and the touch direction must
     * therefore use the same mirror / reverse rule.
     */
    if ((out_x == NULL) || (out_y == NULL))
    {
        return;
    }

    if ((BSP_TOUCH_GUI_WIDTH <= 1U) || (BSP_TOUCH_GUI_HEIGHT <= 1U))
    {
        *out_x = 0;
        *out_y = 0;
        return;
    }

#if (NT35510_DIRECTION == NT35510_DIR_PORTRAIT)
    x = scale_raw_x_to_gui_width(raw_x);
    y = scale_raw_y_to_gui_height(raw_y);
    *out_x = x;
    *out_y = y;
#elif (NT35510_DIRECTION == NT35510_DIR_PORTRAIT_REV)
    x = scale_raw_x_to_gui_width(raw_x);
    y = scale_raw_y_to_gui_height(raw_y);
    *out_x = (uint16_t)((BSP_TOUCH_GUI_WIDTH  - 1U) - x);
    *out_y = (uint16_t)((BSP_TOUCH_GUI_HEIGHT - 1U) - y);
#elif (NT35510_DIRECTION == NT35510_DIR_LANDSCAPE)
    /*
     * MADCTL = 0xA0:
     *   LCD logical x maps to panel long edge (raw_y)
     *   LCD logical y maps to panel short edge (raw_x), mirrored vertically
     */
    x = scale_raw_y_to_gui_width(raw_y);
    y = scale_raw_x_to_gui_height(raw_x);
    *out_x = (uint16_t)((BSP_TOUCH_GUI_WIDTH  - 1U) - x);
    *out_y = y;
#elif (NT35510_DIRECTION == NT35510_DIR_LANDSCAPE_REV)
    /*
     * MADCTL = 0x60:
     *   LCD logical x maps to panel long edge (raw_y)
     *   LCD logical y maps to panel short edge (raw_x), mirrored.
     *
     * Inverse mapping for touch:
     *   gui_x <- raw_y
     *   gui_y <- mirrored raw_x
     */
    x = scale_raw_y_to_gui_width(raw_y);
    y = scale_raw_x_to_gui_height(raw_x);
    *out_x = x;
    *out_y = (uint16_t)((BSP_TOUCH_GUI_HEIGHT - 1U) - y);
#else
    x = scale_raw_x_to_gui_width(raw_x);
    y = scale_raw_y_to_gui_height(raw_y);
    *out_x = x;
    *out_y = y;
#endif
}

uint8_t BSP_Touch_Init(void)
{
    uint8_t ret;
    uint8_t i;

    for (i = 0; i < BSP_TOUCH_MAX_POINTS; i++)
    {
        s_touch_state.points[i].x       = 0;
        s_touch_state.points[i].y       = 0;
        s_touch_state.points[i].pressed = 0;
    }

    s_touch_state.touch_num      = 0;
    s_touch_state.is_pressed     = 0;
    s_gui_pid_state.x            = 0;
    s_gui_pid_state.y            = 0;
    s_gui_pid_state.Pressed      = 0;
    s_gui_pid_state.Layer        = 0;
    s_touch_ready                = 0;
    s_touch_down                 = 0;
    s_last_x                     = 0;
    s_last_y                     = 0;
    s_measure_x                  = 0;
    s_measure_y                  = 0;
    s_measure_ok                 = 0;
    s_measure_tick               = 0xFFFFFFFFUL;
    s_touch_task_timer_started   = 0;

    ret = GT9XXX_Init();
    if (ret == 0)
    {
        s_touch_ready = 1;
        BSP_Touch_TaskInit();
        TOUCH_LOG("init ok, dir=%s gui=%ux%u raw=%ux%u",
#if (NT35510_DIRECTION == NT35510_DIR_PORTRAIT)
                  "portrait",
#elif (NT35510_DIRECTION == NT35510_DIR_PORTRAIT_REV)
                  "portrait_rev",
#elif (NT35510_DIRECTION == NT35510_DIR_LANDSCAPE)
                  "landscape",
#elif (NT35510_DIRECTION == NT35510_DIR_LANDSCAPE_REV)
                  "landscape_rev",
#else
                  "unknown",
#endif
                  (unsigned)NT35510_WIDTH,
                  (unsigned)NT35510_HEIGHT,
                  (unsigned)BSP_TOUCH_RAW_WIDTH,
                  (unsigned)BSP_TOUCH_RAW_HEIGHT);
    }
    else
    {
        TOUCH_LOG("init failed, ret=%u", (unsigned)ret);
    }

    return ret;
}

uint8_t BSP_Touch_Scan(void)
{
    GT9XXX_PointTypeDef raw[GT9XXX_MAX_TOUCH];
    uint8_t num;
    uint8_t i;

    num = GT9XXX_Scan(raw, BSP_TOUCH_MAX_POINTS);

    for (i = 0; i < BSP_TOUCH_MAX_POINTS; i++)
    {
        s_touch_state.points[i].x       = 0;
        s_touch_state.points[i].y       = 0;
        s_touch_state.points[i].pressed = 0;
    }

    if (num == 0)
    {
        s_touch_state.touch_num  = 0;
        s_touch_state.is_pressed = 0;
        s_raw_x_debug            = 0;
        s_raw_y_debug            = 0;
        return 0;
    }

    for (i = 0; i < num && i < BSP_TOUCH_MAX_POINTS; i++)
    {
        if (raw[i].valid)
        {
            if (i == 0U)
            {
                s_raw_x_debug = raw[i].x;
                s_raw_y_debug = raw[i].y;
            }
            coord_convert(raw[i].x, raw[i].y,
                          &s_touch_state.points[i].x,
                          &s_touch_state.points[i].y);
            s_touch_state.points[i].pressed = 1;
        }
    }

    s_touch_state.touch_num  = num;
    s_touch_state.is_pressed = 1;

    return num;
}

void BSP_Touch_GetState(BSP_TouchStateTypeDef *state)
{
    if (state == NULL)
    {
        return;
    }

    *state = s_touch_state;
}

uint8_t BSP_Touch_GetPoint(uint8_t index, uint16_t *x, uint16_t *y)
{
    if (index >= BSP_TOUCH_MAX_POINTS)
    {
        return 0;
    }

    if (!s_touch_state.points[index].pressed)
    {
        return 0;
    }

    if (x != NULL)
    {
        *x = s_touch_state.points[index].x;
    }
    if (y != NULL)
    {
        *y = s_touch_state.points[index].y;
    }

    return 1;
}

uint8_t BSP_Touch_IsPressed(void)
{
    return s_touch_state.is_pressed;
}

uint8_t BSP_Touch_GetTouchNum(void)
{
    return s_touch_state.touch_num;
}

static void TOUCH_PutKey(TOUCH_EVENT_E event, uint16_t x, uint16_t y)
{
    if (GUI_IsInitialized() == 0U)
    {
        return;
    }

    switch (event)
    {
    case TOUCH_DOWN:
    case TOUCH_MOVE:
        s_gui_pid_state.x       = x;
        s_gui_pid_state.y       = y;
        s_gui_pid_state.Pressed = 1;
        s_gui_pid_state.Layer   = 0;
        GUI_TOUCH_StoreStateEx(&s_gui_pid_state);
#if BSP_TOUCH_EMWIN_DEBUG
        if (event == TOUCH_DOWN)
        {
            TOUCH_LOG("DOWN raw=(%u,%u) gui=(%u,%u)",
                      (unsigned)s_raw_x_debug,
                      (unsigned)s_raw_y_debug,
                      (unsigned)x,
                      (unsigned)y);
        }
#endif
        break;

    case TOUCH_RELEASE:
        s_gui_pid_state.Pressed = 0;
        s_gui_pid_state.Layer   = 0;
        GUI_TOUCH_StoreStateEx(&s_gui_pid_state);
#if BSP_TOUCH_EMWIN_DEBUG
        TOUCH_LOG("RELEASE");
#endif
        break;

    default:
        return;
    }
}

static void TOUCH_CapScan(void)
{
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t  touched;

    if (s_touch_ready == 0U)
    {
        return;
    }

    BSP_Touch_Scan();
    touched = BSP_Touch_GetPoint(0, &x, &y);

    if (touched != 0U)
    {
        if (s_touch_down == 0U)
        {
            s_touch_down = 1U;
            s_last_x = x;
            s_last_y = y;
            TOUCH_PutKey(TOUCH_DOWN, x, y);
        }
        else
        {
            s_last_x = x;
            s_last_y = y;
            TOUCH_PutKey(TOUCH_MOVE, x, y);
        }
    }
    else if (s_touch_down != 0U)
    {
        s_touch_down = 0U;
        TOUCH_PutKey(TOUCH_RELEASE, s_last_x, s_last_y);
    }
}

void BSP_Touch_EmWinExec(void)
{
    /* Legacy compatibility hook, no longer used */
}

void BSP_Touch_EmWinSchedule(void)
{
    /* Legacy compatibility hook, no longer used */
}

void BSP_Touch_EmWinFlush(void)
{
    /* Legacy compatibility hook, no longer used */
}

void BSP_Touch_TaskInit(void)
{
    if (s_touch_ready == 0U)
    {
        return;
    }

    bsp_StartAutoTimer(BSP_TOUCH_TIMER_ID, BSP_TOUCH_SCAN_PERIOD_MS);
    s_touch_task_timer_started = 1U;
}

void BSP_Touch_Task20ms(void)
{
    if (s_touch_ready == 0U)
    {
        return;
    }

    if (s_touch_task_timer_started == 0U)
    {
        BSP_Touch_TaskInit();
        if (s_touch_task_timer_started == 0U)
        {
            return;
        }
    }

    if (bsp_CheckTimer(BSP_TOUCH_TIMER_ID) == 0U)
    {
        return;
    }

    TOUCH_CapScan();
}

void PID_X_Exec(void)
{
    BSP_Touch_Task20ms();
}

static void touch_measure_update(void)
{
    uint16_t x = 0;
    uint16_t y = 0;
    uint32_t tick;

    if (s_touch_ready == 0U)
    {
        s_measure_x    = 0U;
        s_measure_y    = 0U;
        s_measure_ok   = 0U;
        return;
    }

    tick = HAL_GetTick();
    if (s_measure_tick == tick)
    {
        return;
    }
    s_measure_tick = tick;

    BSP_Touch_Scan();
    if (BSP_Touch_GetPoint(0, &x, &y) != 0U)
    {
        s_measure_x  = x;
        s_measure_y  = y;
        s_measure_ok = 1U;
    }
    else
    {
        s_measure_x  = 0U;
        s_measure_y  = 0U;
        s_measure_ok = 0U;
    }
}

/*
 * emWin compatibility hooks
 *
 * The linked touch driver module expects these low-level analog-touch
 * symbols to exist, even though this project uses a controller-driven
 * capacitive panel and feeds events through GUI_TOUCH_StoreState().
 *
 * We provide lightweight shims here so the linker is satisfied and, if
 * emWin ever polls them, it still sees the latest cached raw coordinates.
 */
void GUI_TOUCH_X_ActivateX(void)
{
}

void GUI_TOUCH_X_ActivateY(void)
{
}

void GUI_TOUCH_X_Disable(void)
{
}

int GUI_TOUCH_X_MeasureX(void)
{
    touch_measure_update();

    if (s_measure_ok != 0U)
    {
        return (int)s_measure_x;
    }
    return 0;
}

int GUI_TOUCH_X_MeasureY(void)
{
    touch_measure_update();

    if (s_measure_ok != 0U)
    {
        return (int)s_measure_y;
    }
    return 0;
}


/*********************************************************************
*
*       PID_X_Exec
*/
//static void PID_X_Exec(void) {
//  static TOUCH_STATE   TouchState;
//  static GUI_PID_STATE StatePID;
//  static int           IsTouched;

//  if (_IsInitialized) {
//    StatePID.Layer = _LayerIndex;
//    _GetTouchState(&TouchState);


//    if (TouchState.NumTouch > 0) {
//      IsTouched        = 1;
//      StatePID.Pressed = 1;
//      StatePID.x       = TouchState.x;
//      StatePID.y       = TouchState.y;
//      //
//      // Pass information to emWin
//      //
//      GUI_TOUCH_StoreStateEx(&StatePID);
//    } else {
//      if (IsTouched == 1) {
//        //
//        // Since StatePID is declared as static we use the x/y coordinate
//        // from the down event to create an up event.
//        //
//        IsTouched        = 0;
//        StatePID.Pressed = 0;
//        GUI_TOUCH_StoreStateEx(&StatePID);
//      }
//    }
//  }
//}

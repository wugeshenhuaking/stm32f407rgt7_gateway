/*********************************************************************
*          Portions COPYRIGHT 2016 STMicroelectronics                *
*          Portions SEGGER Microcontroller GmbH & Co. KG             *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 1996 - 2015  SEGGER Microcontroller GmbH & Co. KG       *
*                                                                    *
*        Internet: www.segger.com    Support:  support@segger.com    *
*                                                                    *
**********************************************************************

** emWin V5.32 - Graphical user interface for embedded applications **
All  Intellectual Property rights  in the Software belongs to  SEGGER.
emWin is protected by  international copyright laws.  Knowledge of the
source code may not be used to write a similar product.  This file may
only be used in accordance with the following terms:

The  software has  been licensed  to STMicroelectronics International
N.V. a Dutch company with a Swiss branch and its headquarters in Plan-
les-Ouates, Geneva, 39 Chemin du Champ des Filles, Switzerland for the
purposes of creating libraries for ARM Cortex-M-based 32-bit microcon_
troller products commercialized by Licensee only, sublicensed and dis_
tributed under the terms and conditions of the End User License Agree_
ment supplied by STMicroelectronics International N.V.
Full source code is available at: www.segger.com

We appreciate your understanding and fairness.
----------------------------------------------------------------------
File        : GUIConf.c
Purpose     : Display controller initialization
---------------------------END-OF-HEADER------------------------------
*/

/**
  ******************************************************************************
  * @attention
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

#include <stdint.h>
#include <stdio.h>

#include "GUI.h"
#include "bsp_sram.h"
#include "malloc.h"
/*********************************************************************
*
*       Defines
*
**********************************************************************
*/
//
// Define the available number of bytes available for the GUI
//
#define EX_SRAM   1/*1 used extern sram, 0 used internal sram */

#if EX_SRAM
#define GUI_NUMBYTES  (1000*1024)
#else
#define GUI_NUMBYTES  (20*1024)
#endif

/* Define the average block size */
#define GUI_BLOCKSIZE 0x80

/*
 * Put the emWin pool near the top of external SRAM instead of directly at
 * 0x68000000. This mimics the original reference project more closely and
 * helps us verify whether the low-address SRAM region is the unstable area.
 */
//#define EMWIN_EXT_SRAM_RESERVED_TAIL  (64UL * 1024UL)
//#define EMWIN_EXT_SRAM_POOL_BASE      (EXT_SRAM_ADDR + SRAM_SIZE - EMWIN_EXT_SRAM_RESERVED_TAIL - GUI_NUMBYTES)

//#if EX_SRAM && ((GUI_NUMBYTES + EMWIN_EXT_SRAM_RESERVED_TAIL) > SRAM_SIZE)
//#error "emWin external SRAM pool exceeds available SRAM size"
//#endif

#if !EX_SRAM
static U32 s_aMemory[GUI_NUMBYTES / 4];
#endif

static void _EmWinOnShortOfRAM(void)
{
    printf("[emWin] short of RAM: used=%ld free=%ld peak=%ld\r\n",
           (long)GUI_ALLOC_GetNumUsedBytes(),
           (long)GUI_ALLOC_GetNumFreeBytes(),
           (long)GUI_ALLOC_GetMaxUsedBytes());
}

static void _EmWinOnError(const char *s)
{
    printf("[emWin] error: %s\r\n", (s != NULL) ? s : "(null)");
}


/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       GUI_X_Config
*
* Purpose:
*   Called during the initialization process in order to set up the
*   available memory for the GUI.
*/
void GUI_X_Config(void)
{
#if EX_SRAM
//    static void *aMemory;
//    aMemory = (uint16_t *)EXT_SRAM_ADDR;
//    void *aMemory = mymalloc(SRAMEX, GUI_NUMBYTES);

static U32 aMemory[GUI_NUMBYTES / 4] 
    __attribute__((section(".bss.ARM.__at_0x68000000")));

    /*  Assign memory to emWin */
    GUI_ALLOC_AssignMemory(aMemory, GUI_NUMBYTES);
//    GUI_ALLOC_SetAvBlockSize(GUI_BLOCKSIZE);
    GUI_SetDefaultFont(GUI_FONT_6X8);
//  
//    GUI_SetOnErrorFunc(_EmWinOnError);
//    printf("[emWin] alloc pool ext-sram base=0x%08lX end=0x%08lX size=%lu block=0x%02X tail=%lu\r\n",
//           (unsigned long)EMWIN_EXT_SRAM_POOL_BASE,
//           (unsigned long)(EMWIN_EXT_SRAM_POOL_BASE + GUI_NUMBYTES - 1UL),
//           (unsigned long)GUI_NUMBYTES,
//           (unsigned)GUI_BLOCKSIZE,
//           (unsigned long)EMWIN_EXT_SRAM_RESERVED_TAIL);
#else
    /*  Assign memory to emWin */
    GUI_ALLOC_AssignMemory(s_aMemory, GUI_NUMBYTES);
    GUI_SetDefaultFont(GUI_FONT_6X8);
    GUI_ALLOC_SetAvBlockSize(GUI_BLOCKSIZE);
    GUI_SetOnErrorFunc(_EmWinOnError);
    printf("[emWin] alloc pool internal base=%p size=%lu block=0x%02X\r\n",
           (void *)s_aMemory,
           (unsigned long)GUI_NUMBYTES,
           (unsigned)GUI_BLOCKSIZE);
#endif
}

//uintptr_t BSP_EmWin_GetAllocPoolBase(void)
//{
//#if EX_SRAM
//    return (uintptr_t)EMWIN_EXT_SRAM_POOL_BASE;
//#else
//    return (uintptr_t)s_aMemory;
//#endif
//}

//U32 BSP_EmWin_GetAllocPoolSize(void)
//{
//    return GUI_NUMBYTES;
//}

/*************************** End of file ****************************/

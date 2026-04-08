/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "lwip.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
// GCC lib header file
#include <stdio.h>
#include <stdint.h>

// NT35510 lcd ic header file
#include "bsp_lcd.h"
#include "bsp_lcd_test.h"
#include "lcd_nt35510.h"

// BSP SRAM header file
#include "bsp_sram.h"
#include "bsp_sram_test.h"
#include "malloc.h"

// BSP TOUCH header file
#include "bsp_touch_port.h"
#include "test_bsp_touch.h"
#include "bsp_timer.h"

// emWin header file
#include "GUI.h"
#ifdef USE_AC5
#include "GUIDEMO.h"
#include "WM.h"
#endif

// BSP CAN header file
#include "bsp_can_port.h"
#include "bsp_can_test.h"

// CANopen header file
#include "CO_app_STM32.h"

// app CAN init
#include "app_CAN.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
CANopenNodeSTM32 canOpenNodeSTM32;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EMWIN_SRAM_DIAG_ENABLE     0
#define EMWIN_SRAM_DIAG_LOOPS      32UL

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
extern uintptr_t BSP_EmWin_GetAllocPoolBase(void);
extern U32       BSP_EmWin_GetAllocPoolSize(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int fputc(int ch, FILE *f)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 1000);
  return ch;
}
//#include "Generated/Resource.h"
//#include "bsp_touch_port.h"
//    PID_X_Exec();

void user_app(void)
{
    // CANopen polling
    canopen_app_process(); // 1ms polling
    APP_CAN_Process();
  
    // LWIP polling
    MX_LWIP_Process();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SCB->SHCSR |= (SCB_SHCSR_MEMFAULTENA_Msk |
                 SCB_SHCSR_BUSFAULTENA_Msk |
                 SCB_SHCSR_USGFAULTENA_Msk);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FSMC_Init();
  MX_USART1_UART_Init();
  MX_LWIP_Init();
  MX_USART3_UART_Init();
  MX_CAN1_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  
  /*LCD Initialize*/
  NT35510_Init();
//  NT35510_RunAllTests();  // 运行NT35510测试，验证LCD接口和NT35510功能

  /* SRAM Initialize */
//  SRAM_RunAllTests();   // 运行SRAM测试，验证FSMC配置和SRAM芯片功能
//  my_mem_init(SRAMIN);                /* 初始化内部SRAM内存池 */
//  my_mem_init(SRAMEX);                /* 初始化外部SRAM内存池 */
//  my_mem_init(SRAMCCM);               /* 初始化内部CCM内存池 */
  
  /* soft timer Initialize */
  bsp_InitTimer();
  __HAL_RCC_CRC_CLK_ENABLE();

  
  /* TOUCH Initialize */
  #if EMWIN_SRAM_DIAG_ENABLE
  {
    uintptr_t emwin_pool_base = BSP_EmWin_GetAllocPoolBase();
    U32       emwin_pool_size = BSP_EmWin_GetAllocPoolSize();

    if ((emwin_pool_base >= SRAM_BASE_ADDR) &&
        ((emwin_pool_base + emwin_pool_size) <= (SRAM_BASE_ADDR + SRAM_SIZE)))
    {
      SRAM_Test_EmWinPoolStress((uint32_t)(emwin_pool_base - SRAM_BASE_ADDR),
                                (uint32_t)emwin_pool_size,
                                EMWIN_SRAM_DIAG_LOOPS);
    }
    else
    {
      printf("[SRAM] emWin pool is not inside external SRAM, diag skipped\r\n");
    }
  }
  #endif
  BSP_Touch_Init();
  //  Touch_RunAllTests();  // 运行触摸芯片测试，验证触摸屏功能和触摸控制器接口

  /* BSP CAN Initialize*/
#if USE_CANOPEN == DISABLE
  #define BSP_CAN_TXRX_TEST
  BSP_CAN_Test_RunAll();
  BSP_CAN_Init(BSP_CAN_MODE_NORMAL);
#else
  // CANopen init
  canOpenNodeSTM32.CANHandle = &hcan1;
  canOpenNodeSTM32.HWInitFunction = MX_CAN1_Init; // CAN已由CubeMX初始化
  canOpenNodeSTM32.timerHandle = NULL;            // canopen心跳定时器,当前选择由systick接管。所以不需要传入定时器示例
  canOpenNodeSTM32.desiredNodeID = 1;             // 设置当前设备节点ID
  canOpenNodeSTM32.baudrate = 500;                // 500kbps
  canopen_app_init(&canOpenNodeSTM32);
  APP_CAN_Init();
#endif

  /* emWin Initialize*/
  #ifdef USE_AC5
  GUI_Init();
  GUIDEMO_Main();                     /* 运行emwin例程 */
  #else
  MainTask();
  #endif
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if USE_CANOPEN == ENABLE
    // CANopen polling
//    canopen_app_process(); // 1ms polling
//    APP_CAN_Process();
#endif

    // LWIP polling
    //    MX_LWIP_Process();

#ifdef BSP_CAN_TXRX_TEST
    // BSP can polling test
    /* 发送 */
    BSP_CAN_Msg_t tx = {.id = 0x123, .len = 8, .data = {1, 2, 3, 4, 5, 6, 7, 8}};
    BSP_CAN_Send(&tx);

    /* 接收 — 用 BufferHasData 轮询，不阻塞 */
    BSP_CAN_Msg_t rx;
    while (BSP_CAN_BufferHasData())
    {
      if (BSP_CAN_RecvFromBuffer(&rx) == BSP_CAN_OK)
      {
        printf("RX id=0x%03X len=%d data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
               (unsigned)rx.id, rx.len,
               rx.data[0], rx.data[1], rx.data[2], rx.data[3],
               rx.data[4], rx.data[5], rx.data[6], rx.data[7]);
      }
    }
#endif

    // HAL_Delay(500);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

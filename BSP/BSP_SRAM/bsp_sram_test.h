#ifndef __BSP_SRAM_TEST_H
#define __BSP_SRAM_TEST_H

/*
*********************************************************************************************************
*
*   模块名称 : SRAM 测试层
*   文件名称 : bsp_sram_test.h
*   说    明 : 基于 bsp_sram_port 接口的硬件在环测试。
*              所有测试结果通过 printf -> USART1 输出。
*
*   测试列表 :
*       Test1  SingleByte   单字节读写，步长 1024 遍历整个地址空间
*       Test2  Block        256 字节块读写，验证连续访问正确性
*       Test3  AllZeroOne   全 0x00 / 全 0xFF 写入，检测数据线短路
*       Test4  Boundary     首末地址边界读写
*       Test5  Word16Range  连续 16 位访问测试
*       Test6  Word32Range  连续 32 位访问测试
*       Test7  ByteLane     低/高字节选通信号测试
*       Test8  MixedWidth   8/16/32 位混合访问一致性测试
*
*   使用方法 :
*       MX_FSMC_Init();        // CubeMX 生成，main() 中已调用
*       SRAM_RunAllTests();    // 一键运行全部测试
*
*********************************************************************************************************
*/

#include "bsp_sram_port.h"

uint8_t SRAM_Test_SingleByte(void);
uint8_t SRAM_Test_Block(void);
uint8_t SRAM_Test_AllZeroAllOne(void);
uint8_t SRAM_Test_Boundary(void);
uint8_t SRAM_Test_Word16Range(uint32_t addr, uint32_t len);
uint8_t SRAM_Test_Word32Range(uint32_t addr, uint32_t len);
uint8_t SRAM_Test_ByteLaneRange(uint32_t addr, uint32_t len);
uint8_t SRAM_Test_MixedWidthAccess(uint32_t addr, uint32_t len);
uint8_t SRAM_Test_PointerPatternRange(uint32_t addr, uint32_t len, uint32_t loops);
void    SRAM_Test_EmWinPoolRange(uint32_t len);
void    SRAM_Test_EmWinPoolStress(uint32_t addr, uint32_t len, uint32_t loops);
void    SRAM_RunAllTests(void);

#endif /* __BSP_SRAM_TEST_H */

/*
*********************************************************************************************************
*
*   模块名称 : SRAM 测试层
*   文件名称 : bsp_sram_test.c
*   说    明 : 基于 bsp_sram_port 接口的硬件在环测试。
*              本文件不包含任何硬件相关代码，可直接复用到其他平台。
*
*********************************************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include "bsp_sram_test.h"

/*
*********************************************************************************************************
*   函 数 名: SRAM_Test_SingleByte
*   功能说明: 单字节读写测试
*             步长 1024 遍历整个地址空间，写入地址低字节，回读验证
*   返 回 值: 0 = PASS，1 = FAIL
*********************************************************************************************************
*/
uint8_t SRAM_Test_SingleByte(void)
{
    uint8_t  write_val, read_val;
    uint32_t err_count = 0;
    uint32_t addr;
    uint32_t size = BSP_SRAM_GetSize();

    printf("\r\n===== Test1: Single Byte R/W =====\r\n");

    for (addr = 0; addr < size; addr += 1024)
    {
        write_val = (uint8_t)(addr & 0xFF);
        BSP_SRAM_WriteByte(addr, write_val);
    }

    for (addr = 0; addr < size; addr += 1024)
    {
        write_val = (uint8_t)(addr & 0xFF);
        read_val  = BSP_SRAM_ReadByte(addr);

        if (read_val != write_val)
        {
            printf("FAIL addr=0x%08X write=0x%02X read=0x%02X\r\n",
                   (unsigned int)addr, write_val, read_val);
            err_count++;
            if (err_count >= 10) break;
        }
    }

    if (err_count == 0)
    {
        printf("Test1 PASS!\r\n");
        return 0;
    }
    else
    {
        printf("Test1 FAIL! errors=%u\r\n", (unsigned int)err_count);
        return 1;
    }
}

/*
*********************************************************************************************************
*   函 数 名: SRAM_Test_Block
*   功能说明: 256 字节块读写测试
*             一次性写入 256 字节，整块读回后 memcmp 比对
*   返 回 值: 0 = PASS，1 = FAIL
*********************************************************************************************************
*/
uint8_t SRAM_Test_Block(void)
{
    uint8_t  write_buf[256];
    uint8_t  read_buf[256];
    uint16_t i;

    printf("\r\n===== Test2: Block R/W (256 bytes) =====\r\n");

    for (i = 0; i < 256; i++)
    {
        write_buf[i] = (uint8_t)i;
    }
    memset(read_buf, 0, sizeof(read_buf));

    BSP_SRAM_Write(0, write_buf, 256);
    BSP_SRAM_Read(0, read_buf, 256);

    if (memcmp(write_buf, read_buf, 256) == 0)
    {
        printf("Test2 PASS! Block write/read OK\r\n");
        return 0;
    }
    else
    {
        printf("Test2 FAIL! Data mismatch:\r\n");
        for (i = 0; i < 256; i++)
        {
            if (write_buf[i] != read_buf[i])
            {
                printf("  [%d] write=0x%02X read=0x%02X\r\n",
                       i, write_buf[i], read_buf[i]);
            }
        }
        return 1;
    }
}

/*
*********************************************************************************************************
*   函 数 名: SRAM_Test_AllZeroAllOne
*   功能说明: 全 0x00 / 全 0xFF 写入测试
*             步长 512 遍历地址空间，先写全 0 验证，再写全 1 验证
*             主要用于检测数据总线是否有短路
*   返 回 值: 0 = PASS，1 = FAIL
*********************************************************************************************************
*/
uint8_t SRAM_Test_AllZeroAllOne(void)
{
    uint8_t  read_val;
    uint32_t err_count = 0;
    uint32_t addr;
    uint32_t size = BSP_SRAM_GetSize();

    printf("\r\n===== Test3: All-0x00 / All-0xFF =====\r\n");

    /* 写全 0 */
    for (addr = 0; addr < size; addr += 512)
    {
        BSP_SRAM_WriteByte(addr, 0x00);
    }
    for (addr = 0; addr < size; addr += 512)
    {
        read_val = BSP_SRAM_ReadByte(addr);
        if (read_val != 0x00)
        {
            printf("All-0 FAIL addr=0x%08X read=0x%02X\r\n",
                   (unsigned int)addr, read_val);
            if (++err_count >= 5) break;
        }
    }
    if (err_count == 0) printf("All-0x00 PASS\r\n");

    err_count = 0;

    /* 写全 1 */
    for (addr = 0; addr < size; addr += 512)
    {
        BSP_SRAM_WriteByte(addr, 0xFF);
    }
    for (addr = 0; addr < size; addr += 512)
    {
        read_val = BSP_SRAM_ReadByte(addr);
        if (read_val != 0xFF)
        {
            printf("All-F FAIL addr=0x%08X read=0x%02X\r\n",
                   (unsigned int)addr, read_val);
            if (++err_count >= 5) break;
        }
    }

    if (err_count == 0)
    {
        printf("All-0xFF PASS\r\nTest3 PASS!\r\n");
        return 0;
    }
    else
    {
        printf("Test3 FAIL!\r\n");
        return 1;
    }
}

/*
*********************************************************************************************************
*   函 数 名: SRAM_Test_Boundary
*   功能说明: 首末地址边界测试
*             写入首地址 0xAA、末地址 0x55，回读验证
*   返 回 值: 0 = PASS，1 = FAIL
*********************************************************************************************************
*/
uint8_t SRAM_Test_Boundary(void)
{
    uint8_t  r1, r2;
    uint32_t last = BSP_SRAM_GetSize() - 1;

    printf("\r\n===== Test4: Boundary Test =====\r\n");

    BSP_SRAM_WriteByte(0,    0xAA);
    BSP_SRAM_WriteByte(last, 0x55);

    r1 = BSP_SRAM_ReadByte(0);
    r2 = BSP_SRAM_ReadByte(last);

    printf("First byte: write=0xAA read=0x%02X %s\r\n",
           r1, r1 == 0xAA ? "PASS" : "FAIL");
    printf("Last  byte: write=0x55 read=0x%02X %s\r\n",
           r2, r2 == 0x55 ? "PASS" : "FAIL");

    if (r1 == 0xAA && r2 == 0x55)
    {
        printf("Test4 PASS!\r\n");
        return 0;
    }
    else
    {
        printf("Test4 FAIL!\r\n");
        return 1;
    }
}

/*
*********************************************************************************************************
*   函 数 名: SRAM_RunAllTests
*   功能说明: 一键运行全部 SRAM 测试，在 main() 中 MX_FSMC_Init() 之后调用
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
void SRAM_RunAllTests(void)
{
    uint8_t t1, t2, t3, t4;

    printf("\r\n========================================\r\n");
    printf("  SRAM IS62WV51216 Test Start\r\n");
    printf("  Base: 0x%08X  Size: %u bytes\r\n",
           (unsigned int)BSP_SRAM_GetBaseAddr(),
           (unsigned int)BSP_SRAM_GetSize());
    printf("========================================\r\n");

    t1 = SRAM_Test_SingleByte();
    t2 = SRAM_Test_Block();
    t3 = SRAM_Test_AllZeroAllOne();
    t4 = SRAM_Test_Boundary();

    printf("\r\n===== Summary =====\r\n");
    printf("Test1 SingleByte : %s\r\n", t1 == 0 ? "PASS" : "FAIL");
    printf("Test2 Block      : %s\r\n", t2 == 0 ? "PASS" : "FAIL");
    printf("Test3 AllZeroOne : %s\r\n", t3 == 0 ? "PASS" : "FAIL");
    printf("Test4 Boundary   : %s\r\n", t4 == 0 ? "PASS" : "FAIL");

    if ((t1 + t2 + t3 + t4) == 0)
        printf("\nAll Tests PASSED! SRAM is working correctly.\r\n");
    else
        printf("\nSome Tests FAILED! Check wiring or FSMC timing.\r\n");

    printf("========================================\r\n");
}
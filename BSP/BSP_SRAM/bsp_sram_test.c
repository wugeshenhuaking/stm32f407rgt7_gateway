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

static void SRAM_Test_Write16(uint32_t addr, uint16_t data)
{
    SRAM_WRITE16(addr, data);
}

static uint16_t SRAM_Test_Read16(uint32_t addr)
{
    return SRAM_READ16(addr);
}

static void SRAM_Test_Write32(uint32_t addr, uint32_t data)
{
    *(volatile uint32_t *)(SRAM_BASE_ADDR + addr) = data;
}

static uint32_t SRAM_Test_Read32(uint32_t addr)
{
    return *(volatile uint32_t *)(SRAM_BASE_ADDR + addr);
}

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
*   函 数 名: SRAM_Test_Word16Range
*   功能说明: 连续 16 位访问测试
*             用于验证 16 位总线本身在半字读写下是否稳定
*   形    参: addr - 起始偏移地址，必须 2 字节对齐
*             len  - 测试长度，按 2 字节对齐取整
*   返 回 值: 0 = PASS，1 = FAIL
*********************************************************************************************************
*/
uint8_t SRAM_Test_Word16Range(uint32_t addr, uint32_t len)
{
    uint32_t halves;
    uint32_t i;
    uint16_t write_val;
    uint16_t read_val;

    if ((addr & 0x1U) != 0U)
    {
        printf("SRAM_Test_Word16Range FAIL: addr=0x%08X not halfword aligned\r\n",
               (unsigned int)(SRAM_BASE_ADDR + addr));
        return 1;
    }

    halves = len / 2U;
    if (halves == 0U)
    {
        printf("SRAM_Test_Word16Range skipped: len=%u\r\n", (unsigned int)len);
        return 0;
    }

    printf("\r\n===== Test5: 16-bit Range R/W =====\r\n");
    printf("Range: 0x%08X ~ 0x%08X (%u bytes)\r\n",
           (unsigned int)(SRAM_BASE_ADDR + addr),
           (unsigned int)(SRAM_BASE_ADDR + addr + halves * 2U - 1U),
           (unsigned int)(halves * 2U));

    for (i = 0; i < halves; i++)
    {
        write_val = (uint16_t)(0x5A00U ^ (uint16_t)(i * 0x1357U));
        SRAM_Test_Write16(addr + i * 2U, write_val);
    }

    for (i = 0; i < halves; i++)
    {
        write_val = (uint16_t)(0x5A00U ^ (uint16_t)(i * 0x1357U));
        read_val  = SRAM_Test_Read16(addr + i * 2U);
        if (read_val != write_val)
        {
            printf("FAIL pass1 addr=0x%08X write=0x%04X read=0x%04X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i * 2U),
                   (unsigned int)write_val,
                   (unsigned int)read_val);
            return 1;
        }
    }

    for (i = 0; i < halves; i++)
    {
        write_val = (uint16_t)(0xA5FFU ^ (uint16_t)(i * 0x0101U));
        SRAM_Test_Write16(addr + i * 2U, write_val);
    }

    for (i = 0; i < halves; i++)
    {
        write_val = (uint16_t)(0xA5FFU ^ (uint16_t)(i * 0x0101U));
        read_val  = SRAM_Test_Read16(addr + i * 2U);
        if (read_val != write_val)
        {
            printf("FAIL pass2 addr=0x%08X write=0x%04X read=0x%04X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i * 2U),
                   (unsigned int)write_val,
                   (unsigned int)read_val);
            return 1;
        }
    }

    printf("Test5 PASS! 16-bit continuous access OK\r\n");
    return 0;
}

/*
*********************************************************************************************************
*   函 数 名: SRAM_Test_Word32Range
*   功能说明: 连续 32 位访问测试
*             用于验证 CPU 的 32 位逻辑访问经过 FSMC 拆分后是否稳定
*   形    参: addr - 起始偏移地址，必须 4 字节对齐
*             len  - 测试长度，按 4 字节对齐取整
*   返 回 值: 0 = PASS，1 = FAIL
*********************************************************************************************************
*/
uint8_t SRAM_Test_Word32Range(uint32_t addr, uint32_t len)
{
    uint32_t words;
    uint32_t i;
    uint32_t write_val;
    uint32_t read_val;

    if ((addr & 0x3U) != 0U)
    {
        printf("SRAM_Test_Word32Range FAIL: addr=0x%08X not word aligned\r\n",
               (unsigned int)(SRAM_BASE_ADDR + addr));
        return 1;
    }

    words = len / 4U;
    if (words == 0U)
    {
        printf("SRAM_Test_Word32Range skipped: len=%u\r\n", (unsigned int)len);
        return 0;
    }

    printf("\r\n===== Test6: 32-bit Range R/W =====\r\n");
    printf("Range: 0x%08X ~ 0x%08X (%u bytes)\r\n",
           (unsigned int)(SRAM_BASE_ADDR + addr),
           (unsigned int)(SRAM_BASE_ADDR + addr + words * 4U - 1U),
           (unsigned int)(words * 4U));

    for (i = 0; i < words; i++)
    {
        write_val = 0x5AA50000UL ^ (i * 0x13579BUL);
        SRAM_Test_Write32(addr + i * 4U, write_val);
    }

    for (i = 0; i < words; i++)
    {
        write_val = 0x5AA50000UL ^ (i * 0x13579BUL);
        read_val  = SRAM_Test_Read32(addr + i * 4U);
        if (read_val != write_val)
        {
            printf("FAIL pass1 addr=0x%08X write=0x%08X read=0x%08X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i * 4U),
                   (unsigned int)write_val,
                   (unsigned int)read_val);
            return 1;
        }
    }

    for (i = 0; i < words; i++)
    {
        write_val = 0xA55AFFFFUL ^ (i * 0x01010101UL);
        SRAM_Test_Write32(addr + i * 4U, write_val);
    }

    for (i = 0; i < words; i++)
    {
        write_val = 0xA55AFFFFUL ^ (i * 0x01010101UL);
        read_val  = SRAM_Test_Read32(addr + i * 4U);
        if (read_val != write_val)
        {
            printf("FAIL pass2 addr=0x%08X write=0x%08X read=0x%08X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i * 4U),
                   (unsigned int)write_val,
                   (unsigned int)read_val);
            return 1;
        }
    }

    printf("Test6 PASS! 32-bit continuous access OK\r\n");
    return 0;
}

/*
*********************************************************************************************************
*   函 数 名: SRAM_Test_ByteLaneRange
*   功能说明: 验证 16 位总线的低/高字节选通信号是否正常
*             先按 16 位写入，再分别按字节改写低字节和高字节
*   形    参: addr - 起始偏移地址，必须 2 字节对齐
*             len  - 测试长度，按 2 字节对齐取整
*   返 回 值: 0 = PASS，1 = FAIL
*********************************************************************************************************
*/
uint8_t SRAM_Test_ByteLaneRange(uint32_t addr, uint32_t len)
{
    uint32_t halves;
    uint32_t i;
    uint16_t read16;
    uint8_t low_byte;
    uint8_t high_byte;

    if ((addr & 0x1U) != 0U)
    {
        printf("SRAM_Test_ByteLaneRange FAIL: addr=0x%08X not halfword aligned\r\n",
               (unsigned int)(SRAM_BASE_ADDR + addr));
        return 1;
    }

    halves = len / 2U;
    if (halves == 0U)
    {
        printf("SRAM_Test_ByteLaneRange skipped: len=%u\r\n", (unsigned int)len);
        return 0;
    }

    printf("\r\n===== Test7: Byte-Lane R/W =====\r\n");
    printf("Range: 0x%08X ~ 0x%08X (%u bytes)\r\n",
           (unsigned int)(SRAM_BASE_ADDR + addr),
           (unsigned int)(SRAM_BASE_ADDR + addr + halves * 2U - 1U),
           (unsigned int)(halves * 2U));

    for (i = 0; i < halves; i++)
    {
        uint32_t test_addr = addr + i * 2U;

        SRAM_Test_Write16(test_addr, 0xFFFFU);

        BSP_SRAM_WriteByte(test_addr, 0x12U);
        read16 = SRAM_Test_Read16(test_addr);
        if (read16 != 0xFF12U)
        {
            printf("FAIL low-byte addr=0x%08X expect=0xFF12 read=0x%04X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + test_addr),
                   (unsigned int)read16);
            return 1;
        }

        BSP_SRAM_WriteByte(test_addr + 1U, 0x34U);
        read16 = SRAM_Test_Read16(test_addr);
        if (read16 != 0x3412U)
        {
            printf("FAIL high-byte addr=0x%08X expect=0x3412 read=0x%04X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + test_addr),
                   (unsigned int)read16);
            return 1;
        }

        SRAM_Test_Write16(test_addr, 0xA55AU);
        low_byte  = BSP_SRAM_ReadByte(test_addr);
        high_byte = BSP_SRAM_ReadByte(test_addr + 1U);
        if ((low_byte != 0x5AU) || (high_byte != 0xA5U))
        {
            printf("FAIL 16->8 addr=0x%08X expect=[5A A5] read=[%02X %02X]\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + test_addr),
                   (unsigned int)low_byte,
                   (unsigned int)high_byte);
            return 1;
        }
    }

    printf("Test7 PASS! byte-lane access OK\r\n");
    return 0;
}

/*
*********************************************************************************************************
*   函 数 名: SRAM_Test_MixedWidthAccess
*   功能说明: 验证 8/16/32 位交叉读写时数据映射是否一致
*             重点模拟 emWin / C 运行库常见的混合访问模式
*   形    参: addr - 起始偏移地址，必须 4 字节对齐
*             len  - 测试长度，按 4 字节对齐取整
*   返 回 值: 0 = PASS，1 = FAIL
*********************************************************************************************************
*/
uint8_t SRAM_Test_MixedWidthAccess(uint32_t addr, uint32_t len)
{
    uint32_t bytes;
    uint32_t i;
    uint8_t b0, b1, b2, b3;
    uint16_t h0, h1;
    uint32_t w;

    if ((addr & 0x3U) != 0U)
    {
        printf("SRAM_Test_MixedWidthAccess FAIL: addr=0x%08X not word aligned\r\n",
               (unsigned int)(SRAM_BASE_ADDR + addr));
        return 1;
    }

    bytes = len & ~0x3U;
    if (bytes < 4U)
    {
        printf("SRAM_Test_MixedWidthAccess skipped: len=%u\r\n", (unsigned int)len);
        return 0;
    }

    printf("\r\n===== Test8: Mixed Width Access =====\r\n");
    printf("Range: 0x%08X ~ 0x%08X (%u bytes)\r\n",
           (unsigned int)(SRAM_BASE_ADDR + addr),
           (unsigned int)(SRAM_BASE_ADDR + addr + bytes - 1U),
           (unsigned int)bytes);

    for (i = 0; i < bytes; i++)
    {
        BSP_SRAM_WriteByte(addr + i, (uint8_t)(0x20U + (uint8_t)i));
    }

    for (i = 0; i < bytes; i += 4U)
    {
        b0 = (uint8_t)(0x20U + (uint8_t)(i + 0U));
        b1 = (uint8_t)(0x20U + (uint8_t)(i + 1U));
        b2 = (uint8_t)(0x20U + (uint8_t)(i + 2U));
        b3 = (uint8_t)(0x20U + (uint8_t)(i + 3U));

        h0 = SRAM_Test_Read16(addr + i);
        h1 = SRAM_Test_Read16(addr + i + 2U);
        w  = SRAM_Test_Read32(addr + i);

        if (h0 != (uint16_t)(b0 | ((uint16_t)b1 << 8)))
        {
            printf("FAIL 8->16 addr=0x%08X expect=0x%04X read=0x%04X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i),
                   (unsigned int)(b0 | ((uint16_t)b1 << 8)),
                   (unsigned int)h0);
            return 1;
        }

        if (h1 != (uint16_t)(b2 | ((uint16_t)b3 << 8)))
        {
            printf("FAIL 8->16 addr=0x%08X expect=0x%04X read=0x%04X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i + 2U),
                   (unsigned int)(b2 | ((uint16_t)b3 << 8)),
                   (unsigned int)h1);
            return 1;
        }

        if (w != ((uint32_t)b0 |
                  ((uint32_t)b1 << 8) |
                  ((uint32_t)b2 << 16) |
                  ((uint32_t)b3 << 24)))
        {
            printf("FAIL 8->32 addr=0x%08X expect=0x%08X read=0x%08X\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i),
                   (unsigned int)((uint32_t)b0 |
                                  ((uint32_t)b1 << 8) |
                                  ((uint32_t)b2 << 16) |
                                  ((uint32_t)b3 << 24)),
                   (unsigned int)w);
            return 1;
        }
    }

    for (i = 0; i < bytes; i += 2U)
    {
        h0 = (uint16_t)(0xA100U ^ (uint16_t)(i * 0x31U));
        SRAM_Test_Write16(addr + i, h0);
    }

    for (i = 0; i < bytes; i += 2U)
    {
        h0 = (uint16_t)(0xA100U ^ (uint16_t)(i * 0x31U));
        b0 = BSP_SRAM_ReadByte(addr + i);
        b1 = BSP_SRAM_ReadByte(addr + i + 1U);
        if ((b0 != (uint8_t)(h0 & 0xFFU)) || (b1 != (uint8_t)(h0 >> 8)))
        {
            printf("FAIL 16->8 addr=0x%08X expect=[%02X %02X] read=[%02X %02X]\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i),
                   (unsigned int)(h0 & 0xFFU),
                   (unsigned int)(h0 >> 8),
                   (unsigned int)b0,
                   (unsigned int)b1);
            return 1;
        }
    }

    for (i = 0; i < bytes; i += 4U)
    {
        w = 0x11223344UL ^ (i * 0x01020408UL);
        SRAM_Test_Write32(addr + i, w);
    }

    for (i = 0; i < bytes; i += 4U)
    {
        w  = 0x11223344UL ^ (i * 0x01020408UL);
        b0 = BSP_SRAM_ReadByte(addr + i);
        b1 = BSP_SRAM_ReadByte(addr + i + 1U);
        b2 = BSP_SRAM_ReadByte(addr + i + 2U);
        b3 = BSP_SRAM_ReadByte(addr + i + 3U);
        h0 = SRAM_Test_Read16(addr + i);
        h1 = SRAM_Test_Read16(addr + i + 2U);

        if ((b0 != (uint8_t)(w & 0xFFU)) ||
            (b1 != (uint8_t)((w >> 8) & 0xFFU)) ||
            (b2 != (uint8_t)((w >> 16) & 0xFFU)) ||
            (b3 != (uint8_t)((w >> 24) & 0xFFU)))
        {
            printf("FAIL 32->8 addr=0x%08X expect=[%02X %02X %02X %02X] read=[%02X %02X %02X %02X]\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i),
                   (unsigned int)(w & 0xFFU),
                   (unsigned int)((w >> 8) & 0xFFU),
                   (unsigned int)((w >> 16) & 0xFFU),
                   (unsigned int)((w >> 24) & 0xFFU),
                   (unsigned int)b0,
                   (unsigned int)b1,
                   (unsigned int)b2,
                   (unsigned int)b3);
            return 1;
        }

        if ((h0 != (uint16_t)(w & 0xFFFFU)) ||
            (h1 != (uint16_t)((w >> 16) & 0xFFFFU)))
        {
            printf("FAIL 32->16 addr=0x%08X expect=[0x%04X 0x%04X] read=[0x%04X 0x%04X]\r\n",
                   (unsigned int)(SRAM_BASE_ADDR + addr + i),
                   (unsigned int)(w & 0xFFFFU),
                   (unsigned int)((w >> 16) & 0xFFFFU),
                   (unsigned int)h0,
                   (unsigned int)h1);
            return 1;
        }
    }

    printf("Test8 PASS! mixed 8/16/32-bit access is consistent\r\n");
    return 0;
}

uint8_t SRAM_Test_PointerPatternRange(uint32_t addr, uint32_t len, uint32_t loops)
{
    static const uint32_t s_masks[] =
    {
        0x00000000UL,
        0xFFFFFFFFUL,
        0xA5A55A5AUL,
        0x5A5AA5A5UL
    };
    uint32_t words;
    uint32_t loop;
    uint32_t phase;
    uint32_t i;
    uint32_t expected;
    uint32_t actual;

    if ((addr & 0x3U) != 0U)
    {
        printf("SRAM_Test_PointerPatternRange FAIL: addr=0x%08X not word aligned\r\n",
               (unsigned int)(SRAM_BASE_ADDR + addr));
        return 1;
    }

    words = len / 4U;
    if ((words == 0U) || (loops == 0U))
    {
        printf("SRAM_Test_PointerPatternRange skipped: len=%u loops=%u\r\n",
               (unsigned int)len,
               (unsigned int)loops);
        return 0;
    }

    printf("\r\n===== Test9: Pointer Pattern Stress =====\r\n");
    printf("Range: 0x%08X ~ 0x%08X (%u bytes), loops=%u\r\n",
           (unsigned int)(SRAM_BASE_ADDR + addr),
           (unsigned int)(SRAM_BASE_ADDR + addr + words * 4U - 1U),
           (unsigned int)(words * 4U),
           (unsigned int)loops);

    for (loop = 0; loop < loops; loop++)
    {
        for (phase = 0; phase < (sizeof(s_masks) / sizeof(s_masks[0])); phase++)
        {
            for (i = 0; i < words; i++)
            {
                expected = (SRAM_BASE_ADDR + addr + i * 4U) ^ s_masks[phase];
                SRAM_Test_Write32(addr + i * 4U, expected);
            }

            for (i = 0; i < words; i++)
            {
                expected = (SRAM_BASE_ADDR + addr + i * 4U) ^ s_masks[phase];
                actual   = SRAM_Test_Read32(addr + i * 4U);
                if (actual != expected)
                {
                    printf("FAIL loop=%u phase=%u addr=0x%08X write=0x%08X read=0x%08X xor=0x%08X\r\n",
                           (unsigned int)loop,
                           (unsigned int)phase,
                           (unsigned int)(SRAM_BASE_ADDR + addr + i * 4U),
                           (unsigned int)expected,
                           (unsigned int)actual,
                           (unsigned int)(expected ^ actual));
                    return 1;
                }
            }
        }
    }

    printf("Test9 PASS! pointer-like 32-bit stress OK\r\n");
    return 0;
}

void SRAM_Test_EmWinPoolRange(uint32_t len)
{
    uint8_t t16;
    uint8_t t32;
    uint8_t tbl;
    uint8_t tmx;

    printf("\r\n===== emWin SRAM Pool Diagnostic =====\r\n");
    printf("Pool base : 0x%08X\r\n", (unsigned int)SRAM_BASE_ADDR);
    printf("Pool size : %u bytes\r\n", (unsigned int)len);

    t16 = SRAM_Test_Word16Range(0, len);
    t32 = SRAM_Test_Word32Range(0, len);
    tbl = SRAM_Test_ByteLaneRange(0, len);
    tmx = SRAM_Test_MixedWidthAccess(0, (len > 256U) ? 256U : len);

    printf("\r\n===== emWin SRAM Pool Summary =====\r\n");
    printf("16-bit range : %s\r\n", t16 == 0U ? "PASS" : "FAIL");
    printf("32-bit range : %s\r\n", t32 == 0U ? "PASS" : "FAIL");
    printf("Byte-lane    : %s\r\n", tbl == 0U ? "PASS" : "FAIL");
    printf("Mixed width  : %s\r\n", tmx == 0U ? "PASS" : "FAIL");

    if ((t16 | t32 | tbl | tmx) == 0U)
    {
        printf("emWin pool area looks stable for 8/16/32-bit accesses.\r\n");
    }
    else
    {
        printf("emWin pool area FAILED at least one access-width diagnostic.\r\n");
    }
}

void SRAM_Test_EmWinPoolStress(uint32_t addr, uint32_t len, uint32_t loops)
{
    uint8_t t16;
    uint8_t t32;
    uint8_t tbl;
    uint8_t tmx;
    uint8_t tpt;
    uint32_t mixed_len;

    mixed_len = (len > 256U) ? 256U : len;

    printf("\r\n===== emWin SRAM Exact Region Diagnostic =====\r\n");
    printf("Pool base : 0x%08X\r\n", (unsigned int)(SRAM_BASE_ADDR + addr));
    printf("Pool size : %u bytes\r\n", (unsigned int)len);
    printf("Loops     : %u\r\n", (unsigned int)loops);

    t16 = SRAM_Test_Word16Range(addr, len);
    t32 = SRAM_Test_Word32Range(addr, len);
    tbl = SRAM_Test_ByteLaneRange(addr, len);
    tmx = SRAM_Test_MixedWidthAccess(addr, mixed_len);
    tpt = SRAM_Test_PointerPatternRange(addr, len, loops);

    printf("\r\n===== emWin Exact Region Summary =====\r\n");
    printf("16-bit range : %s\r\n", t16 == 0U ? "PASS" : "FAIL");
    printf("32-bit range : %s\r\n", t32 == 0U ? "PASS" : "FAIL");
    printf("Byte-lane    : %s\r\n", tbl == 0U ? "PASS" : "FAIL");
    printf("Mixed width  : %s\r\n", tmx == 0U ? "PASS" : "FAIL");
    printf("Ptr pattern  : %s\r\n", tpt == 0U ? "PASS" : "FAIL");

    if ((t16 | t32 | tbl | tmx | tpt) == 0U)
    {
        printf("Exact emWin pool region looks stable.\r\n");
    }
    else
    {
        printf("Exact emWin pool region FAILED at least one diagnostic.\r\n");
    }
}

void SRAM_RunAllTests(void)
{
    uint8_t t1, t2, t3, t4, t5, t6, t7, t8;

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
    t5 = SRAM_Test_Word16Range(0, BSP_SRAM_GetSize());
    t6 = SRAM_Test_Word32Range(0, BSP_SRAM_GetSize());
    t7 = SRAM_Test_ByteLaneRange(0, BSP_SRAM_GetSize());
    t8 = SRAM_Test_MixedWidthAccess(0, 256U);

    printf("\r\n===== Summary =====\r\n");
    printf("Test1 SingleByte : %s\r\n", t1 == 0 ? "PASS" : "FAIL");
    printf("Test2 Block      : %s\r\n", t2 == 0 ? "PASS" : "FAIL");
    printf("Test3 AllZeroOne : %s\r\n", t3 == 0 ? "PASS" : "FAIL");
    printf("Test4 Boundary   : %s\r\n", t4 == 0 ? "PASS" : "FAIL");
    printf("Test5 Word16     : %s\r\n", t5 == 0 ? "PASS" : "FAIL");
    printf("Test6 Word32     : %s\r\n", t6 == 0 ? "PASS" : "FAIL");
    printf("Test7 ByteLane   : %s\r\n", t7 == 0 ? "PASS" : "FAIL");
    printf("Test8 MixedWidth : %s\r\n", t8 == 0 ? "PASS" : "FAIL");

    if ((t1 + t2 + t3 + t4 + t5 + t6 + t7 + t8) == 0)
        printf("\nAll Tests PASSED! SRAM is working correctly.\r\n");
    else
        printf("\nSome Tests FAILED! Check wiring or FSMC timing.\r\n");

    printf("========================================\r\n");
}

#include "bsp_sram.h"
void FSMC_SRAM_Init(void);
void FSMC_SRAM_WriteBuffer(uint8_t* pBuffer,uint32_t WriteAddr,uint32_t NumHalfwordToWrite);
void FSMC_SRAM_ReadBuffer(uint8_t* pBuffer,uint32_t ReadAddr,uint32_t NumHalfwordToRead);


void fsmc_sram_test_write(uint32_t addr,uint8_t data);
uint8_t fsmc_sram_test_read(uint32_t addr);

int XmRamInit(void);

#pragma once
#pragma once
#ifndef __SDRAM_H
#define __SDRAM_H

#include "stm32f4xx_hal.h"

#define SDRAM_DEVICE_ADDR         ((uint32_t)0xD0000000)
#define SDRAM_DEVICE_SIZE         ((uint32_t)0x800000)  /* SDRAM device size in Bytes */

#define SDRAM_MEMORY_WIDTH      FMC_SDRAM_MEM_BUS_WIDTH_16

#define SDRAM_CAS_LATENCY       FMC_SDRAM_CAS_LATENCY_3 
#define SDCLOCK_PERIOD          FMC_SDRAM_CLOCK_PERIOD_2
#define SDRAM_READBURST         FMC_SDRAM_RBURST_DISABLE

/* Set the refresh rate counter */
/* (15.62 us x Freq) - 20 */
#define REFRESH_COUNT           ((uint32_t)1386)   /* SDRAM refresh counter */
#define SDRAM_TIMEOUT           ((uint32_t)0xFFFF)

/* DMA definitions for SDRAM DMA transfer */
#define __DMAx_CLK_ENABLE       __HAL_RCC_DMA2_CLK_ENABLE
#define SDRAM_DMAx_CHANNEL      DMA_CHANNEL_0
#define SDRAM_DMAx_STREAM       DMA2_Stream0
#define SDRAM_DMAx_IRQn         DMA2_Stream0_IRQn
#define SDRAM_DMAx_IRQHandler   DMA2_Stream0_IRQHandler

#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

#define   SDRAM_OK         ((uint8_t)0x00)
#define   SDRAM_ERROR      ((uint8_t)0x01)

class SDRAM {
public:
	SDRAM();
	~SDRAM();
	uint8_t init();
	uint8_t writeData(uint32_t uwStartAddress, uint32_t *pData, uint32_t uwDataSize);
	uint8_t readData(uint32_t uwStartAddress, uint32_t *pData, uint32_t uwDataSize);
private:
	void setupHw(SDRAM_HandleTypeDef  *hsdram, void *Params);
	void initSequence(uint32_t refreshCount);
	SDRAM_HandleTypeDef sdramHandle;
	FMC_SDRAM_TimingTypeDef timing;
	FMC_SDRAM_CommandTypeDef command;
	DMA_HandleTypeDef dmaHandle;
};

#endif /* __SDRAM_H */
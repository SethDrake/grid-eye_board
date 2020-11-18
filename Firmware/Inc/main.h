#pragma once
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include <stm32f4xx_hal.h>
#include "Support/STM32Cube_FW_F4_V1.19.0/Drivers/BSP/STM32F429I-Discovery/stm32f429i_discovery_io.h"
#include "Support/STM32Cube_FW_F4_V1.19.0/Drivers/BSP/STM32F429I-Discovery/stm32f429i_discovery_sdram.h"
#include "Support/STM32Cube_FW_F4_V1.19.0/Drivers/BSP/components/ili9341/ili9341.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/ 
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#define FRAMEBUFFER_ADDR  SDRAM_DEVICE_ADDR
#define FRAMEBUFFER_SIZE  320 * 240 * 2
#define FRAMEBUFFER2_ADDR FRAMEBUFFER_ADDR + FRAMEBUFFER_SIZE
#define FRAMEBUFFER2_SIZE  320 * 240 * 2
#define CUSTOM_DATA_ADDR  FRAMEBUFFER2_ADDR + FRAMEBUFFER2_SIZE

#define THERMAL_RESOLUTION 240

extern void Error_Handler(const uint8_t source);
extern DMA2D_HandleTypeDef dma2dHandle;

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */



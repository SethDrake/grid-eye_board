#pragma once
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "stm32f429i_discovery.h"
#include "stm32f429i_discovery_io.h"
#include "stm32f429i_discovery_sdram.h"
//#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ts.h"
#include "stm32f429i_discovery_gyroscope.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/ 
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#define FRAMEBUFFER_ADDR  SDRAM_DEVICE_ADDR
#define FRAMEBUFFER_SIZE  320 * 240 * 2
#define FRAMEBUFFER_ADDR2 FRAMEBUFFER_ADDR + FRAMEBUFFER_SIZE
#define CUSTOM_DATA_ADDR  FRAMEBUFFER_ADDR2 + FRAMEBUFFER_SIZE

extern void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */



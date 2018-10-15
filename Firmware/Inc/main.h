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
#include "cpu_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/ 
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#define FRAMEBUFFER_ADDR  SDRAM_DEVICE_ADDR
#define FRAMEBUFFER_SIZE  240 * 240 * 2
#define FRAMEBUFFER_ADDR2 FRAMEBUFFER_ADDR + FRAMEBUFFER_SIZE
#define FRAMEBUFFER2_SIZE  240 * 80 * 2
#define CUSTOM_DATA_ADDR  FRAMEBUFFER_ADDR2 + FRAMEBUFFER2_SIZE

#define THERMAL_RESOLUTION 240

extern void Error_Handler();

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */



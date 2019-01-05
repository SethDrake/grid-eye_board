#pragma once
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include <../CMSIS_RTOS/cmsis_os.h>
#include <stm32f4xx_hal.h>
#include "ili9341.h"
#include "framebuffer.h"
#include "cpu_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/ 
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#define THERMAL_FB_ADDR  SDRAM_DEVICE_ADDR
#define THERMAL_FB_SIZE  240 * 240 * 2
#define INFO_FB_ADDR	 THERMAL_FB_ADDR + THERMAL_FB_SIZE
#define INFO_FB_SIZE	 80 * 240 * 2

#define CUSTOM_DATA_ADDR INFO_FB_ADDR + INFO_FB_SIZE

#define THERMAL_RESOLUTION 240

#define DISPLAY_SPI		SPI5
#define THERMAL_I2C		I2C3

#define BTN_PORT		GPIOA
#define BTN_PIN			GPIO_PIN_0

#define LED_PORT		GPIOG
#define LED_1_PIN		GPIO_PIN_13
#define LED_2_PIN		GPIO_PIN_14

extern ILI9341 display;
extern Framebuffer fbMain;
extern Framebuffer fbInfo;

extern void Error_Handler(const uint8_t source);
extern void vApplicationStackOverflowHook(TaskHandle_t xTask, const char* taskName);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */



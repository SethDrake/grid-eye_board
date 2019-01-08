#pragma once
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include <../CMSIS_RTOS/cmsis_os.h>
#include <stm32f4xx_hal.h>
#include "ili9341.h"
#include "framebuffer.h"
#include "cpu_utils.h"
#include "ov7670.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/ 
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#define THERMAL_FB_ADDR    SDRAM_DEVICE_ADDR
#define THERMAL_FB_SIZE	   240 * 240 * 2
#define INFO_FB_ADDR	   THERMAL_FB_ADDR + THERMAL_FB_SIZE
#define INFO_FB_SIZE	   80 * 240 * 2
#define CAMERA_FB_ADDR	   INFO_FB_ADDR + INFO_FB_SIZE
#define CAMERA_FB_SIZE	   CAM_FRAME_WIDTH * CAM_FRAME_HEIGHT * 3

#define CUSTOM_DATA_ADDR   CAMERA_FB_ADDR + CAMERA_FB_SIZE

#define THERMAL_RESOLUTION 240

#define DISPLAY_SPI		   SPI5
#define THERMAL_I2C		   I2C3
#define CAMERA_I2C		   I2C1

#define BTN_PORT		   GPIOA
#define BTN_PIN			   GPIO_PIN_0

#define LED_PORT		   GPIOG
#define GREEN_LED_PIN	   GPIO_PIN_13
#define RED_LED_PIN		   GPIO_PIN_14

#define CAM_XCLK_PIN	   GPIO_PIN_6
#define CAM_XCLK_PORT	   GPIOF

#define CAM_PCLK_PIN	   GPIO_PIN_3
#define CAM_PCLK_PORT	   GPIOC

#define CAM_RESET_PIN	   GPIO_PIN_9
#define CAM_RESET_PORT	   GPIOB

#define CAM_HREF_PIN	   GPIO_PIN_10
#define CAM_HREF_PORT	   GPIOF

extern SPI_HandleTypeDef lcdSpiHandle;

extern OV7670 camera;
extern ILI9341 display;
extern Framebuffer fbMain;
extern Framebuffer fbInfo;
extern Framebuffer fbCamera;

extern void Error_Handler(uint8_t reason, unsigned int * hardfault_args);
extern void vApplicationStackOverflowHook(TaskHandle_t xTask, const char* taskName);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */



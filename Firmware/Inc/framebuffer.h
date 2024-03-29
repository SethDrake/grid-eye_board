#pragma once
#ifndef __FRAMEBUFFER_H
#define __FRAMEBUFFER_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* ARGB1555 */
#define ARGB_COLOR_RED		  0x7C00
#define ARGB_COLOR_PURPLE	  0x7990
#define ARGB_COLOR_GREEN	  0x03e0
#define ARGB_COLOR_BLUE		  0x001f
#define ARGB_COLOR_BLACK	  0x0000
#define ARGB_COLOR_YELLOW	  0x7FE0
#define ARGB_COLOR_ORANGE	  0x7E00
#define ARGB_COLOR_WHITE	  0x7FFF
#define ARGB_COLOR_CYAN		  0x03FF
#define ARGB_COLOR_BRIGHT_RED 0x7C10
#define ARGB_COLOR_GRAY1	  0x4210
#define ARGB_COLOR_GRAY2	  0x2108

/* RGB565 */
#define COLOR_RED		  0xf800
#define COLOR_PURPLE	  0xf310
#define COLOR_GREEN		  0x07e0
#define COLOR_BLUE		  0x001f
#define COLOR_BLACK		  0x0000
#define COLOR_YELLOW	  0xffe0
#define COLOR_ORANGE	  0xfc00
#define COLOR_WHITE		  0xffff
#define COLOR_CYAN		  0x07ff
#define COLOR_BRIGHT_RED  0xf810
#define COLOR_GRAY1		  0x8410
#define COLOR_GRAY2		  0x4208

typedef enum
{ 
	PORTRAIT = 0,
	LANDSCAPE
} FB_ORIENTATION;

class Framebuffer {
public:
	Framebuffer();
	~Framebuffer();
	void init(DMA2D_HandleTypeDef* dma2dHandler, uint8_t layer, const uint32_t fb_addr, const uint16_t fb_sizeX, const uint16_t fb_sizeY, const uint16_t color, const uint16_t bg_color);
	void setFbAddr(const uint32_t fb_addr);
	void setTextColor(const uint16_t color, const uint16_t bg_color);
	void setOrientation(const FB_ORIENTATION orientation);
	void clear(const uint32_t color);
	uint16_t getFBSizeX();
	uint16_t getFBSizeY();
	void printf(const uint16_t x, const uint16_t y, const uint16_t charColor, const uint16_t bkgColor, const char *format, ...);
	void printf(const uint16_t x, const uint16_t y, const char *format, ...);
	void putString(const char str[], uint16_t x, const uint16_t y, const uint16_t charColor, const uint16_t bkgColor);
	void pixelDraw(const uint16_t xpos, const uint16_t ypos, const uint16_t color);
protected:
	void putChar(const uint16_t x, uint16_t y, const uint8_t chr, const uint16_t charColor, const uint16_t bkgColor);
private:
	DMA2D_HandleTypeDef* dma2dHandler;
	uint16_t fb_sizeX;
	uint16_t fb_sizeY;
	uint32_t fb_addr;
	uint16_t color;
	uint16_t bg_color;
	const uint8_t *font;
	FB_ORIENTATION orientation;
	uint8_t layer;
};

#endif /* __THERMAL_H */
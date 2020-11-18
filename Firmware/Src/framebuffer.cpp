#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <framebuffer.h>
#include <mini_fonts.h>
#include "stm32f4xx_hal.h"

Framebuffer::Framebuffer()
{
	this->fb_sizeX = 0;
	this->fb_sizeY = 0;
	this->fb_addr = 0;
	this->color = 0xffff;
	this->bg_color = 0x0000;
	this->font = Consolas8x14;
	this->orientation = PORTRAIT;
	this->dma2dHandler = NULL;
	this->layer = 0;
}

Framebuffer::~Framebuffer()
{
	if (this->fb_addr != 0)
	{
		this->fb_addr = 0;	
	}
	this->fb_sizeX = 0;
	this->fb_sizeY = 0;
}

void Framebuffer::init(DMA2D_HandleTypeDef* dma2dHandler, uint8_t layer, const uint32_t fb_addr, const uint16_t fb_sizeX, const uint16_t fb_sizeY, const uint16_t color, const uint16_t bg_color)
{
	this->dma2dHandler = dma2dHandler;
	this->fb_sizeX = fb_sizeX;
	this->fb_sizeY = fb_sizeY;
	setFbAddr(fb_addr);
	this->layer = layer;
	this->setTextColor(color, bg_color);
}


void Framebuffer::setFbAddr(const uint32_t fb_addr)
{
	this->fb_addr = fb_addr;
}

void Framebuffer::setTextColor(const uint16_t color, const uint16_t bg_color)
{
	this->color = color;
	this->bg_color = bg_color;
}

void Framebuffer::setOrientation(const FB_ORIENTATION orientation)
{
	this->orientation = orientation;
}

void Framebuffer::clear(const uint32_t color)
{
	(*this->dma2dHandler).Init.Mode         = DMA2D_R2M;
	(*this->dma2dHandler).Init.ColorMode    = DMA2D_RGB565;
	(*this->dma2dHandler).Init.OutputOffset = 0;
	(*this->dma2dHandler).Instance = DMA2D;
	
	if (HAL_DMA2D_Init(this->dma2dHandler) == HAL_OK) 
	{
		if (HAL_DMA2D_ConfigLayer(this->dma2dHandler, layer) == HAL_OK) 
		{
			HAL_DMA2D_Start(this->dma2dHandler, color, this->fb_addr, this->fb_sizeX, this->fb_sizeY);
			HAL_DMA2D_PollForTransfer(this->dma2dHandler, 10);
		}
	} 

	/*volatile uint16_t *pSdramAddress = (uint16_t *)this->fb_addr;
	uint32_t buf_size = (this->fb_sizeX * this->fb_sizeY); 
	for (; buf_size != 0U; buf_size--)
	{
		*(volatile uint16_t *)pSdramAddress = color;
		pSdramAddress++;
	}*/
}

uint16_t Framebuffer::getFBSizeX()
{
	return this->fb_sizeX;
}

uint16_t Framebuffer::getFBSizeY()
{
	return this->fb_sizeY;
}

void Framebuffer::pixelDraw(const uint16_t xpos, const uint16_t ypos, const uint16_t color)
{
	volatile uint16_t *pSdramAddress = (uint16_t *)this->fb_addr;
	if (orientation == PORTRAIT)
	{
		pSdramAddress += ypos * fb_sizeX + xpos;	
	}
	else
	{
		pSdramAddress += xpos * fb_sizeY + ypos;	
	}
	*(volatile uint16_t *)pSdramAddress = color;
}

void Framebuffer::putChar(const uint16_t x, uint16_t y, const uint8_t chr, const uint16_t charColor, const uint16_t bkgColor)
{
	const uint8_t f_width = font[0];	
	const uint8_t f_height = font[1];
	const uint16_t f_bytes = (f_width * f_height / 8);

	volatile uint16_t *pSdramAddress = (uint16_t *)this->fb_addr;
	if (orientation == PORTRAIT)
	{
		pSdramAddress += y * fb_sizeX + x;
		for (uint8_t i = 0; i < f_height; i++)
		{
			for (uint8_t j = 0; j < f_width; j++) {
				const uint16_t bitNumberGlobal = f_width * i + (f_width - j);
				const uint16_t byteNumberLocal = (bitNumberGlobal / 8);
				const uint8_t bitNumberInByte = bitNumberGlobal - byteNumberLocal * 8;
				const uint8_t glyphByte = font[(chr - 0x20) * f_bytes + byteNumberLocal + 2];
				const uint8_t mask = 1 << bitNumberInByte;
				if (glyphByte & mask) {
					*(volatile uint16_t *)pSdramAddress = charColor;
				}
				else 
				{
					*(volatile uint16_t *)pSdramAddress = bkgColor;
				}
				pSdramAddress++;
			}
			//pSdramAddress++;
			//*(volatile uint16_t *)pSdramAddress = bkgColor; //vertical empty line right from symbol
			pSdramAddress = (uint16_t *)this->fb_addr;
			pSdramAddress += (y + i) * fb_sizeX + x;
		}
	}
	else
	{
		pSdramAddress += x * fb_sizeY + y;
		for (uint8_t i = 0; i < f_width; i++)
		{
			for (uint8_t j = 0; j < f_height; j++) {
				const uint16_t bitNumberGlobal = f_width * (f_height - j) + (f_width - i);
				const uint16_t byteNumberLocal = (bitNumberGlobal / 8);
				const uint8_t bitNumberInByte = bitNumberGlobal - byteNumberLocal * 8;
				const uint8_t glyphByte = font[(chr - 0x20) * f_bytes + byteNumberLocal + 2];
				const uint8_t mask = 1 << bitNumberInByte;
				if (glyphByte & mask) {
					*(volatile uint16_t *)pSdramAddress = charColor;
				}
				else 
				{
					*(volatile uint16_t *)pSdramAddress = bkgColor;
				}
				pSdramAddress++;
			}
			pSdramAddress = (uint16_t *)this->fb_addr;
			pSdramAddress += (x + i) * fb_sizeY + y;
		}
	}


}

void Framebuffer::putString(const char str[], uint16_t x, const uint16_t y, const uint16_t charColor, const uint16_t bkgColor)
{
	while (*str != 0) {
		putChar(x, y, *str, charColor, bkgColor);
		x += font[0]-1; //increment to font width
		str++;
	}
}

void Framebuffer::printf(const uint16_t x, const uint16_t y, const uint16_t charColor, const uint16_t bkgColor, const char* format, ...)
{
	char buf[40];
	va_list args;
	va_start(args, format);
	vsprintf(buf, format, args);
	putString(buf, x, y, charColor, bkgColor);
}

void Framebuffer::printf(const uint16_t x, const uint16_t y, const char* format, ...)
{
	char buf[40];
	va_list args;
	va_start(args, format);
	vsprintf(buf, format, args);
	putString(buf, x, y, color, bg_color);
}

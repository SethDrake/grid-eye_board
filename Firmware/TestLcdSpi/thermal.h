#pragma once
#ifndef __THERMAL_H
#define __THERMAL_H

#include <stm32f4xx_hal.h>
#include <framebuffer.h>

#define GRID_EYE_ADDR 0xD0
#define THERM_COEFF 0.0625
#define TEMP_COEFF 0.25

#define I2C_TIMEOUT_MAX  0x3000

class IRSensor {
public:
	IRSensor();
	~IRSensor();
	void init(I2C_HandleTypeDef* i2cHandle, const uint32_t fb_addr, const uint16_t fbSizeX, const uint16_t fbSizeY, const uint8_t* colorScheme);
	void setColorScheme(const uint8_t* colorScheme);
	void setFbAddress(const uint32_t fb_addr);
	void setFbSize(const uint16_t fbSizeX, const uint16_t fbSizeY);
	float readThermistor();
	void readImage();
	float* getTempMap();
	float getMaxTemp();
	float getMinTemp();
	uint8_t getHotDotIndex();
	uint8_t getColdDotIndex();
	void findMinAndMaxTemp();
	void visualizeImage(uint8_t resX, uint8_t resY, const uint8_t method);
	void drawGradient(Framebuffer* fb, uint8_t startX, uint8_t startY, uint8_t stopX, uint8_t stopY);
protected:
	uint16_t temperatureToRGB565(float temperature, float minTemp, float maxTemp);
	uint16_t rgb2color(uint8_t R, uint8_t G, uint8_t B);
	uint8_t calculateRGB(uint8_t rgb1, uint8_t rgb2, float t1, float step, float t);
	void writeData(uint8_t addr, uint8_t reg, uint8_t value);
	uint8_t readData(uint8_t addr, uint8_t reg);
private:
	I2C_HandleTypeDef* i2cHandle;
	uint32_t fb_addr;
	uint16_t fbSizeX;
	uint16_t fbSizeY;
	const uint8_t* colorScheme;
	float dots[64];
	uint16_t colors[64];
	uint8_t coldDotIndex;
	uint8_t hotDotIndex;
	float minTemp;
	float maxTemp;
	float rawHLtoTemp(uint8_t rawL, uint8_t rawH, float coeff);
	const float minTempCorr = -0.5;
	const float maxTempCorr = 0.5;
};


static const uint8_t DEFAULT_COLOR_SCHEME[] = {
	 28, 1, 108 ,
	 31, 17, 218 ,
	 50, 111, 238 ,
	 63, 196, 229 ,
	 64, 222, 135 ,
	 192, 240, 14 ,
	 223, 172, 18 ,
	 209, 111, 14 ,
	 210, 50, 28 ,
	 194, 26, 0 ,
	 132, 26, 0 
};

static const uint8_t ALTERNATE_COLOR_SCHEME[] = {
	 0, 0, 5 ,
	 7, 1, 97 ,
	 51, 1, 194 ,
	 110, 2, 212 ,
	 158, 6, 150 ,
	 197, 30, 58 ,
	 218, 66, 0 ,
	 237, 137, 0 ,
	 246, 199, 23 ,
	 251, 248, 117 ,
	 252, 254, 253 
};

#endif /* __THERMAL_H */



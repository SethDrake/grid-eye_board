#pragma once
#ifndef __THERMAL_H
#define __THERMAL_H

#include "stm32f429i_discovery_io.h"

#define GRID_EYE_ADDR 0xD0
#define THERM_COEFF 0.0625
#define TEMP_COEFF 0.25

class IRSensor {
public:
	IRSensor();
	~IRSensor();
	void init(DMA2D_HandleTypeDef* dma2dHandler, uint8_t layer, const uint32_t fb_addr, const uint8_t* colorScheme);
	void setColorScheme(const uint8_t* colorScheme);
	float readThermistor();
	void readImage();
	float* getTempMap();
	float getMaxTemp();
	float getMinTemp();
	void visualizeImage(uint8_t resX, uint8_t resY, const uint8_t method);
protected:
	void findMinAndMaxTemp();
	uint16_t rgb2color(uint8_t R, uint8_t G, uint8_t B);
	uint8_t calculateRGB(uint8_t rgb1, uint8_t rgb2, float t1, float step, float t);
	uint16_t temperatureToRGB565(float temperature, float minTemp, float maxTemp);
private:
	DMA2D_HandleTypeDef* dma2dHandler;
	uint8_t layer;
	uint32_t fb_addr;
	const uint8_t* colorScheme;
	float dots[64];
	float minTemp;
	float maxTemp;
	float rawHLtoTemp(uint8_t rawL, uint8_t rawH, float coeff);	
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



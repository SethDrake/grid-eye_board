#include <thermal.h>
#include "FreeRTOS.h"
#include "framebuffer.h"
#include <cstring>

IRSensor::IRSensor()
{
	this->dma2dHandler = NULL;
	this->fb_addr = 0;
	this->minTemp = 0;
	this->maxTemp = 0;
	this->layer = 0;
	this->fbSizeX = 0;
	this->fbSizeY = 0;
}

IRSensor::~IRSensor()
{
}

float IRSensor::rawHLtoTemp(const uint8_t rawL, const uint8_t rawH, const float coeff)
{
	const uint16_t therm = ((rawH & 0x07) << 8) | rawL;
	float temp = therm * coeff;
	if ((rawH >> 3) != 0)
	{
		temp *= -1;
	}
	return temp;
}

void IRSensor::init(DMA2D_HandleTypeDef* dma2dHandler, uint8_t layer, const uint32_t fb_addr, const uint16_t fbSizeX, const uint16_t fbSizeY, const uint8_t* colorScheme)
{
	//IOE_Write(GRID_EYE_ADDR, 0x00, 0x00); //set normal mode
	//IOE_Write(GRID_EYE_ADDR, 0x02, 0x00); //set 10 FPS mode
	IOE_Write(GRID_EYE_ADDR, 0x03, 0x00); //disable INT
	setFbAddress(fb_addr);
	this->dma2dHandler = dma2dHandler;
	this->layer = layer;
	this->setColorScheme(colorScheme);
	this->setFbSize(fbSizeX, fbSizeY);
}

void IRSensor::setColorScheme(const uint8_t* colorScheme)
{
	this->colorScheme = colorScheme;
}


void IRSensor::setFbAddress(const uint32_t fb_addr)
{
	this->fb_addr = fb_addr;
}

void IRSensor::setFbSize(const uint16_t fbSizeX, const uint16_t fbSizeY)
{
	this->fbSizeX = fbSizeX;
	this->fbSizeY = fbSizeY;
}

float IRSensor::readThermistor()
{
	const uint8_t thermL = IOE_Read(GRID_EYE_ADDR, 0x0E);
	const uint8_t thermH = IOE_Read(GRID_EYE_ADDR, 0x0F);
	return this->rawHLtoTemp(thermL, thermH, THERM_COEFF);
}

void IRSensor::readImage()
{
	uint8_t taddr = 0x80;
	for (uint8_t i = 0; i < 64; i++)
	{
		const uint8_t rawL = IOE_Read(GRID_EYE_ADDR, taddr); //low
		taddr++;
		const uint8_t rawH = IOE_Read(GRID_EYE_ADDR, taddr); //high
		taddr++;
		this->dots[i] = this->rawHLtoTemp(rawL, rawH, TEMP_COEFF);
	}
	this->findMinAndMaxTemp();
}

float* IRSensor::getTempMap()
{
	return this->dots;
}

float IRSensor::getMaxTemp()
{
	return this->maxTemp;
}

float IRSensor::getMinTemp()
{
	return this->minTemp;
}

uint8_t IRSensor::getHotDotIndex()
{
	return this->hotDotIndex;
}

uint8_t IRSensor::getColdDotIndex()
{
	return this->coldDotIndex;
}

void IRSensor::drawGradient(uint8_t startX, uint8_t startY, uint8_t stopX, uint8_t stopY)
{
	const uint8_t height = stopY - startY;
	const uint8_t width = stopX - startX;
	const float diff = (maxTemp + minTempCorr - minTemp + maxTempCorr) / height;
	uint16_t line[height];
	for (uint8_t j = 0; j < height; j++)
	{
		const float temp = minTemp + (diff * j);
		line[j] = temperatureToRGB565(temp, minTemp + minTempCorr, maxTemp + maxTempCorr);	
	}

	for (uint8_t i = 0; i < width; i++)
	{
		volatile uint16_t *pSdramAddress = (uint16_t *)(this->fb_addr + (240 * (startX + i) + startY) * 2);
		for (uint8_t j = 0; j < height; j++)
		{
			*(volatile uint16_t *)pSdramAddress = line[j];	
			pSdramAddress++;
		}
	}
}

void IRSensor::visualizeImage(const uint8_t resX, const uint8_t resY, const uint8_t method)
{
	uint8_t line = 0;
	uint8_t row = 0;

	volatile uint16_t* pSdramAddress = (uint16_t *)this->fb_addr;

	if (method == 0)
	{
		for (uint8_t i = 0; i < 64; i++)
		{
			colors[i] = this->temperatureToRGB565(dots[i], minTemp + minTempCorr, maxTemp + maxTempCorr);		
		}
		
		const uint8_t lrepeat = resX / 8;
		const uint8_t rrepeat = resY / 8;
		while (line < 8)
		{
			for (uint8_t t = 0; t < lrepeat; t++) //repeat
			{
				while (row < 8)
				{
					for (uint8_t k = 0; k < rrepeat; k++) //repeat
					{
						*(volatile uint16_t *)pSdramAddress = colors[line * 8 + row];
						pSdramAddress++;
					}
					row++;
				}
				row = 0;
			}
			line++;
		}
	}
	else if (method == 1)
	{
		float tmp, u, t, d1, d2, d3, d4;
		uint16_t p1, p2, p3, p4;
		
		for (uint8_t i = 0; i < 64; i++)
		{
			colors[i] = this->temperatureToRGB565(dots[i], minTemp + minTempCorr, maxTemp + maxTempCorr);		
		}
		
		for (uint16_t j = 0; j < resY; j++) {
			tmp = (float)(j) / (float)(resY - 1) * (8 - 1);
			int16_t h = (int16_t)tmp;
			if (h >= 8 - 1) {
				h = 8 - 2;
			}
			u = tmp - h;

			pSdramAddress = (uint16_t *)(this->fb_addr + (j * resX) * 2);

			for (uint16_t i = 0; i < resX; i++) {

				tmp = (float)(i) / (float)(resX - 1) * (8 - 1);
				int16_t w = (int16_t)tmp;
				if (w >= 8 - 1) {
					w = 8 - 2;
				}
				t = tmp - w;

				d1 = (1 - t) * (1 - u);
				d2 = t * (1 - u);
				d3 = t * u;
				d4 = (1 - t) * u;

				p1 = colors[h * 8 + w];
				p2 = colors[h * 8 + w + 1];
				p3 = colors[(h + 1) * 8 + w + 1];
				p4 = colors[(h + 1) * 8 + w];

				uint8_t blue = ((uint8_t)(p1 & 0x001f)*d1 + (uint8_t)(p2 & 0x001f)*d2 + (uint8_t)(p3 & 0x001f)*d3 + (uint8_t)(p4 & 0x001f)*d4);
				uint8_t green = (uint8_t)((p1 >> 5) & 0x003f) * d1 + (uint8_t)((p2 >> 5) & 0x003f) * d2 + (uint8_t)((p3 >> 5) & 0x003f) * d3 + (uint8_t)((p4 >> 5) & 0x003f) * d4;
				uint8_t red = (uint8_t)(p1 >> 11) * d1 + (uint8_t)(p2 >> 11) * d2 + (uint8_t)(p3 >> 11) * d3 + (uint8_t)(p4 >> 11) * d4;

				*(volatile uint16_t *)pSdramAddress = ((u_int16_t) red << 11) | ((u_int16_t) green << 5) | (blue);
				pSdramAddress++;
			}
		}
	}
	else if (method == 2)
	{
		float tmp, u, t, d1, d2, d3, d4;
		float p1, p2, p3, p4;
		
		for (uint16_t j = 0; j < resY; j++) {
			tmp = (float)(j) / (float)(resY - 1) * (8 - 1);
			int16_t h = (int16_t)tmp;
			if (h >= 8 - 1) {
				h = 8 - 2;
			}
			u = tmp - h;

			pSdramAddress = (uint16_t *)(this->fb_addr + (j * resX) * 2);

			for (uint16_t i = 0; i < resX; i++) {
				tmp = (float)(i) / (float)(resX - 1) * (8 - 1);
				int16_t w = (int16_t)tmp;
				if (w >= 8 - 1) {
					w = 8 - 2;
				}
				t = tmp - w;

				d1 = (1 - t) * (1 - u);
				d2 = t * (1 - u);
				d3 = t * u;
				d4 = (1 - t) * u;

				p1 = dots[h*8+w];
				p2 = dots[h * 8 + w + 1];
				p3 = dots[(h + 1) * 8 + w + 1];
				p4 = dots[(h + 1) * 8 + w];

				float temp = p1*d1 + p2*d2 + p3*d3 + p4*d4;

				*(volatile uint16_t *)pSdramAddress = this->temperatureToRGB565(temp, minTemp + minTempCorr, maxTemp + maxTempCorr);
				pSdramAddress++;
			}
		}
	}
}

void IRSensor::Dma2dXferCpltCallback(DMA2D_HandleTypeDef* hdma2d)
{

}

void IRSensor::findMinAndMaxTemp()
{
	this->minTemp = 1000;
	this->maxTemp = -100;
	for (uint8_t i = 0; i < 64; i++)
	{
		if (dots[i] < minTemp)
		{
			minTemp = dots[i];	
			coldDotIndex = i;
		}
		if (dots[i] > maxTemp)
		{
			maxTemp = dots[i];
			hotDotIndex = i;
		}
	}
}

uint16_t IRSensor::rgb2color(const uint8_t R, const uint8_t G, const uint8_t B)
{
	return ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3);
}

uint8_t IRSensor::calculateRGB(const uint8_t rgb1, const uint8_t rgb2, const float t1, const float step, const float t) {
	return (uint8_t)(rgb1 + (((t - t1) / step) * (rgb2 - rgb1)));
}

uint16_t IRSensor::temperatureToRGB565(const float temperature, const float minTemp, const float maxTemp) {
	uint16_t val;
	if (temperature < minTemp) {
		val = rgb2color(colorScheme[0], colorScheme[1], colorScheme[2]);
	}
	else if (temperature >= maxTemp) {
		const short colorSchemeSize = sizeof(DEFAULT_COLOR_SCHEME);
		val = rgb2color(colorScheme[(colorSchemeSize - 1) * 3 + 0], colorScheme[(colorSchemeSize - 1) * 3 + 1], colorScheme[(colorSchemeSize - 1) * 3 + 2]);
	}
	else {
		const float step = (maxTemp - minTemp) / 10.0;
		const uint8_t step1 = (uint8_t)((temperature - minTemp) / step);
		const uint8_t step2 = step1 + 1;
		const uint8_t red = calculateRGB(colorScheme[step1 * 3 + 0], colorScheme[step2 * 3 + 0], (minTemp + step1 * step), step, temperature);
		const uint8_t green = calculateRGB(colorScheme[step1 * 3 + 1], colorScheme[step2 * 3 + 1], (minTemp + step1 * step), step, temperature);
		const uint8_t blue = calculateRGB(colorScheme[step1 * 3 + 2], colorScheme[step2 * 3 + 2], (minTemp + step1 * step), step, temperature);
		val = rgb2color(red, green, blue);
	}
	return val;
}
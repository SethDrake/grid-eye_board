#include <thermal.h>

IRSensor::IRSensor()
{
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

void IRSensor::init(const uint32_t fb_addr)
{
	IOE_Write(GRID_EYE_ADDR, 0x00, 0x00); //set normal mode
	IOE_Write(GRID_EYE_ADDR, 0x02, 0x00); //set 10 FPS mode
	IOE_Write(GRID_EYE_ADDR, 0x03, 0x00); //disable INT
	this->fb_addr = fb_addr;
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

void IRSensor::visualizeImage(const uint8_t resX, const uint8_t resY)
{
	uint8_t line = 0;
	uint8_t row = 0;

	const uint8_t lrepeat = resX / 8;
	const uint8_t rrepeat = resY / 8;

	volatile uint16_t *pSdramAddress = (uint16_t *)this->fb_addr;

	while (line < 8)
	{
		for (uint8_t t = 0; t < lrepeat; t++) //repeat
		{
			while (row < 8)
			{
				const uint16_t color = this->temperatureToRGB565(dots[line * 8 + row], minTemp - 2, maxTemp + 3);
				for (uint8_t k = 0; k < rrepeat; k++) //repeat
				{
					*(volatile uint16_t *)pSdramAddress = color;
					pSdramAddress++;	
				}
				row++;
			}
			row = 0;
		}
		line++;				
	}
}


void IRSensor::findMinAndMaxTemp()
{
	this->minTemp = 1000;
	this->maxTemp = -100;
	for (uint8_t i = 0; i < 64; i++)
	{
		if (this->dots[i] < this->minTemp)
		{
			this->minTemp = dots[i];	
		}
		if (this->dots[i] > this->maxTemp)
		{
			this->maxTemp = dots[i];	
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
		val = rgb2color(DEFAULT_COLOR_SCHEME[0][0], DEFAULT_COLOR_SCHEME[0][1], DEFAULT_COLOR_SCHEME[0][2]);
	}
	else if (temperature >= maxTemp) {
		const short colorSchemeSize = sizeof(DEFAULT_COLOR_SCHEME);
		val = rgb2color(DEFAULT_COLOR_SCHEME[colorSchemeSize - 1][0], DEFAULT_COLOR_SCHEME[colorSchemeSize - 1][1], DEFAULT_COLOR_SCHEME[colorSchemeSize - 1][2]);
	}
	else {
		const float step = (maxTemp - minTemp) / 10.0;
		const uint8_t step1 = (uint8_t)((temperature - minTemp) / step);
		const uint8_t step2 = step1 + 1;
		const uint8_t red = calculateRGB(DEFAULT_COLOR_SCHEME[step1][0], DEFAULT_COLOR_SCHEME[step2][0], (minTemp + step1 * step), step, temperature);
		const uint8_t green = calculateRGB(DEFAULT_COLOR_SCHEME[step1][1], DEFAULT_COLOR_SCHEME[step2][1], (minTemp + step1 * step), step, temperature);
		const uint8_t blue = calculateRGB(DEFAULT_COLOR_SCHEME[step1][2], DEFAULT_COLOR_SCHEME[step2][2], (minTemp + step1 * step), step, temperature);
		val = rgb2color(red, green, blue);
	}
	return val;
}
#include "ov7670.h"
#include "delay.h"
#include <cstring>
#include "main.h"
#include "sdram.h"

OV7670::OV7670()
{
	this->fb_addr = 0;
	this->capturePtr = 0;
	this->i2cHandle = nullptr;
	this->hrefPort = nullptr;
	this->hrefPin = 0;
	this->resetPort = nullptr;
	this->resetPin = 0;
	this->isOk = false;
	this->isCaptureInProgress = false;
	this->isCameraFrameReady = true;
	this->isHrefHigh = false;
	this->bytesCount = 0;
	this->linesCount = 0;
	this->cameraID = 0x0000;
}

OV7670::~OV7670()
{
}

bool OV7670::init(I2C_HandleTypeDef* i2cHandle, GPIO_TypeDef* hrefPort, const uint16_t hrefPin, GPIO_TypeDef* resetPort, const uint16_t resetPin, const uint32_t fb_addr)
{
	this->i2cHandle = i2cHandle;
	this->hrefPort = hrefPort;
	this->hrefPin = hrefPin;
	this->resetPort = resetPort;
	this->resetPin = resetPin;
	this->fb_addr = fb_addr;

	//HW reset
	HAL_GPIO_WritePin(resetPort, resetPin, GPIO_PIN_RESET);
	DelayManager::DelayMs(10);
	HAL_GPIO_WritePin(resetPort, resetPin, GPIO_PIN_SET);

	writeData(REG_COM7, COM7_RESET); //reset
	DelayManager::DelayMs(30);
	//read PID & VID
	const uint8_t camPID = readData(REG_PID);
	if (camPID != 0x76)
	{
		return false;
	}
	cameraID = ((uint16_t)camPID << 8) | readData(REG_VER);
	writeData(REG_CLKRC, 0x80); //prescaler = 1
	//writeData(REG_COM7, COM7_FMT_QVGA  | COM7_RGB); //QVGA, RBG output
	writeData(REG_COM7, COM7_FMT_QCIF);
	//writeData(REG_RGB444, 0x00); //disable RGB444
	//writeData(REG_COM15, COM15_RGB565); // RGB565
	//writeData(REG_COM3, 0x08); //Enable scaling

	HAL_NVIC_SetPriority(PCLK_INT, 0xF0, 0);
	isOk = true;

	return isOk;
}

void OV7670::writeData(const uint8_t reg, uint8_t value)
{
	HAL_I2C_Mem_Write(i2cHandle, CAM_I2C_ADDR, (uint16_t)reg, I2C_MEMADD_SIZE_8BIT, &value, 1, SCCB_TIMEOUT_MAX);
}

uint8_t OV7670::readData(uint8_t reg)
{
	uint8_t data = 0;
	uint8_t i = 3;
	while (i > 0) {
		i--;
		HAL_StatusTypeDef stat = HAL_I2C_Master_Transmit(i2cHandle, CAM_I2C_ADDR, &reg, 1, SCCB_TIMEOUT_MAX);
		if (stat != HAL_OK)
		{
			continue;
		}
		stat = HAL_I2C_Master_Receive(i2cHandle, CAM_I2C_ADDR, &data, 1, SCCB_TIMEOUT_MAX);
		if (stat == HAL_OK)
		{
			i = 0;
		}
	}
	return data;
}

bool OV7670::isFrameReady()
{
	return isCameraFrameReady;
}

void OV7670::captureFrame()
{
	if (!isOk) return;
	if (isCaptureInProgress) return;
	capturePtr = fb_addr;
	isCaptureInProgress = true;
	isCameraFrameReady = false;
	//allow pclk interrupt
	HAL_NVIC_EnableIRQ(PCLK_INT);
}


void OV7670::stopCapture()
{
	//disable pclk interrupt
	HAL_NVIC_DisableIRQ(PCLK_INT);
	isCaptureInProgress = false;
	isHrefHigh = false;
	linesCount = 0;
	isCameraFrameReady = true;
}

void OV7670::PclkInterrupt()
{
	if (!isCaptureInProgress) return;
	const bool isHrefActual = (hrefPort->IDR & hrefPin) != 0;
	if (isHrefActual)
	{
		if (!isHrefHigh)
		{
			isHrefHigh = true;
			linesCount++;
		}
		//read byte from camera
		uint8_t camData = (GPIOA->IDR & 0xF8) >> 3;
		camData |= (GPIOD->IDR & 0x1C) << 3;
		line[bytesCount] = camData;
		bytesCount++;
	}
	else //href is down - end if line
	{
		isHrefHigh = false;
		memcpy((uint8_t *)this->capturePtr, (const uint8_t*)&line, bytesCount);
		this->capturePtr += bytesCount;
		bytesCount = 0;
		if (linesCount >= CAM_FRAME_HEIGHT)
		{
			stopCapture();
		}
	}
}

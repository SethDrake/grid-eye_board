#include "ov7670.h"
#include "delay.h"
#include "main.h"

OV7670::OV7670()
{
	this->fb_addr = 0;
	this->i2cHandle = nullptr;
	this->isOk = false;
	this->isCaptureInProgress = false;
	this->isCameraFrameReady = true;
	this->cameraID = 0x0000;
	this->effect = CE_NONE;
}

OV7670::~OV7670()
{
}

bool OV7670::init(I2C_HandleTypeDef* i2cHandle, const uint32_t fb_addr)
{
	this->i2cHandle = i2cHandle;
	this->fb_addr = fb_addr;

	//init DCMI HW
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();

	GPIO_InitTypeDef  GPIO_InitStruct;
	GPIO_InitStruct.Pin = CAM_PCLK_PIN | CAM_HREF_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
	HAL_GPIO_Init(CAM_PCLK_HREF_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = CAM_VSYNC_PIN | CAM_DATA3_PIN;
	HAL_GPIO_Init(CAM_VSYNC_D3_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = CAM_DATA0_PIN | CAM_DATA1_PIN | CAM_DATA2_PIN;
	HAL_GPIO_Init(CAM_D0_1_2_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = CAM_DATA4_PIN | CAM_DATA6_PIN | CAM_DATA7_PIN;
	HAL_GPIO_Init(CAM_D4_6_7_PORT, &GPIO_InitStruct);	
	
	GPIO_InitStruct.Pin = CAM_DATA5_PIN;
	HAL_GPIO_Init(CAM_DATA5_PORT, &GPIO_InitStruct);

	__HAL_RCC_DCMI_CLK_ENABLE();

	dcmiHandle.Instance = DCMI;
	dcmiHandle.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
	dcmiHandle.Init.PCKPolarity = DCMI_PCKPOLARITY_RISING;
	dcmiHandle.Init.VSPolarity = DCMI_VSPOLARITY_HIGH;
	dcmiHandle.Init.HSPolarity = DCMI_HSPOLARITY_LOW;
	dcmiHandle.Init.CaptureRate = DCMI_CR_ALL_FRAME;
	dcmiHandle.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
	dcmiHandle.Init.JPEGMode = DCMI_JPEG_DISABLE;
	if (HAL_DCMI_Init(&dcmiHandle) != HAL_OK)
	{
		return false;
	}

	//HAL_DCMI_DisableCrop(&dcmiHandle);
	HAL_DCMI_ConfigCrop(&dcmiHandle, (CAM_SENSOR_WIDTH - CAM_FRAME_WIDTH)/2, (CAM_SENSOR_HEIGHT - CAM_FRAME_HEIGHT)/2, 
									 (CAM_FRAME_WIDTH * 2) - 1, CAM_FRAME_HEIGHT - 1);
	HAL_DCMI_EnableCrop(&dcmiHandle);

	HAL_NVIC_SetPriority(DCMI_IRQn, 0x0A, 0);
	HAL_NVIC_EnableIRQ(DCMI_IRQn);


	__HAL_RCC_DMA2_CLK_ENABLE();

	dcmiDmaHandle.Instance = DMA2_Stream1;
	dcmiDmaHandle.Init.Channel = DMA_CHANNEL_1;
	dcmiDmaHandle.Init.Direction = DMA_PERIPH_TO_MEMORY;
	dcmiDmaHandle.Init.PeriphInc = DMA_PINC_DISABLE;
	dcmiDmaHandle.Init.MemInc = DMA_MINC_ENABLE;
	dcmiDmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
	dcmiDmaHandle.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
	dcmiDmaHandle.Init.Mode = DMA_NORMAL;
	dcmiDmaHandle.Init.Priority = DMA_PRIORITY_LOW;
	dcmiDmaHandle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	if (HAL_DMA_Init(&dcmiDmaHandle) != HAL_OK)
	{
		return false;
	}

	__HAL_LINKDMA(&dcmiHandle, DMA_Handle, dcmiDmaHandle);

	HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0x05, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

	DelayManager::DelayMs(50);
	writeData(REG_COM7, COM7_RESET); //reset
	DelayManager::DelayMs(30);
	//read PID & VID
	const uint8_t camPID = readData(REG_PID);
	if (camPID != 0x76)
	{
		return false;
	}
	cameraID = ((uint16_t)camPID << 8) | readData(REG_VER);

	executeSequence(OV7670_reg);
	writeData(REG_CLKRC, 0x80);

	setEffect(CE_NONE);

	isOk = true;

	return isOk;
}


void OV7670::setEffect(const CAM_EFFECT effect)
{
	this->effect = effect;
	if (this->effect == CE_NONE)
	{
		writeData(REG_TSLB, 0x04 | 0x08);
		writeData(0x67, 0xC0);
		writeData(0x68, 0x80);
	}
	else if (this->effect == CE_BW)
	{
		writeData(REG_TSLB, 0x14 | 0x08);
		writeData(0x67, 0x80);
		writeData(0x68, 0x80);
	}
	else if (this->effect == CE_NEGATIVE)
	{
		writeData(REG_TSLB, 0x24);
		writeData(0x67, 0x80);
		writeData(0x68, 0x80);
	}
	else if (this->effect == CE_T800)
	{
		writeData(REG_TSLB, 0x34);
		writeData(0x67, 0x80);
		writeData(0x68, 0x80);
	}
}

void OV7670::DCMI_Interrupt()
{
	HAL_DCMI_IRQHandler(&dcmiHandle);
}

void OV7670::DCMI_DMA_Interrupt()
{
	HAL_DMA_IRQHandler(&dcmiDmaHandle);
}

void OV7670::writeData(const uint8_t reg, uint8_t value)
{
	HAL_I2C_Mem_Write(i2cHandle, CAM_I2C_ADDR, (uint16_t)reg, I2C_MEMADD_SIZE_8BIT, &value, 1, SCCB_TIMEOUT_MAX);
	DelayManager::DelayMs(1);
}

uint8_t OV7670::readData(uint8_t reg)
{
	uint8_t data = 0x77;
	uint8_t i = 10;
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

void OV7670::executeSequence(const regval_list reglist[])
{
	const struct regval_list *next = reglist;
	for (;;) {
		const uint8_t reg_addr = next->reg_num;
		const uint8_t reg_val = next->value;
		if ((reg_addr == 0xff) && (reg_val == 0xff)) {
			break;
		}
		writeData(reg_addr, reg_val);
		if (reg_addr != 0x12) {
			const uint8_t new_val = readData(reg_addr);
			if (new_val != reg_val)
			{
				Error_Handler(0x33, nullptr, "OV7670 INIT ERROR");
				while (true) {};
			}
		}
		*next++;
	}
}

bool OV7670::isFrameReady()
{
	return isCameraFrameReady;
}


bool OV7670::isCameraOk()
{
	return isOk;
}


uint16_t OV7670::getCameraId()
{
	return cameraID;
}

void OV7670::captureFrame()
{
	if (!isOk) return;
	if (isCaptureInProgress) return;
	isCaptureInProgress = true;
	isCameraFrameReady = false;
	HAL_DCMI_Start_DMA(&dcmiHandle, DCMI_MODE_SNAPSHOT, fb_addr, CAM_FRAME_WIDTH * CAM_FRAME_HEIGHT / 2);
}

void OV7670::frameCompleted()
{
	HAL_DCMI_Stop(&dcmiHandle);
	isCaptureInProgress = false;
	isCameraFrameReady = true;
}


#include <stdio.h>
#include "delay.h"
#include "ili9341.h"
#include "cmsis_os.h"

ILI9341::ILI9341() {
	isDataSending = 0;
	manualCsControl = 0;
	disableDMA = 0;
	spi = nullptr;
	spiPrescaler = SPI_BAUDRATEPRESCALER_8;
	rsPort = nullptr;
	csPort = nullptr;
	rsPin = 0;
	csPin = 0;

	color = WHITE;
	bgColor = BLACK;
	
	isLandscape = false;
	isOk = false;
}

ILI9341::~ILI9341() {
}

void ILI9341::setupHw(SPI_HandleTypeDef* spi, const uint32_t spiPrescaler, GPIO_TypeDef* rsPort, const uint16_t rsPin, GPIO_TypeDef* csPort, const uint16_t csPin) {
	this->spi = spi;
	this->spiPrescaler = spiPrescaler;
	this->rsPort = rsPort;
	this->csPort = csPort;
	this->rsPin = rsPin;
	this->csPin = csPin;

	/* RS & CS pins of ILI9341 */
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.Pin = rsPin;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init(rsPort, &GPIO_InitStructure);
	
	GPIO_InitStructure.Pin = csPin;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FAST;
	HAL_GPIO_Init(csPort, &GPIO_InitStructure);

	__HAL_RCC_DMA2_CLK_ENABLE();

	HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, 0x0F, 0);
}

void ILI9341::setSPISpeed(const uint32_t prescaler) {
	__HAL_SPI_DISABLE(spi);
	spi->Instance->CR1 &= ~SPI_CR1_BR; // Clear SPI baud rate bits
	spi->Instance->CR1 |= prescaler; // Set SPI baud rate bits
	__HAL_SPI_ENABLE(spi);
}

void ILI9341::setSPIDataSize(const uint32_t spiDataSize) {
	__HAL_SPI_DISABLE(spi);
	spi->Instance->CR1 &= ~SPI_CR1_DFF; // Clear SPI data frame format
	spi->Instance->CR1 |= spiDataSize; // Set SPI data frame format
	__HAL_SPI_ENABLE(spi);
}

void ILI9341::init(void) {

	setSPISpeed(SPI_BAUDRATEPRESCALER_16);
	switchCs(0);

	/*sendCmd(ILI9341_SOFT_RESET_REG); //software reset
	DelayManager::DelayMs(15);
	sendCmd(ILI9341_DISPLAYOFF_REG); // display off
	DelayManager::DelayMs(15);*/

	sendCmd(0xCA);
	sendData(0xC3);
	sendCmd(0x08);
	sendData(0x50);

	sendCmd(ILI9341_POWERCTLB_REG);
	sendData(0x00);
	sendData(0xC1);
	sendData(0x30);

	sendCmd(ILI9341_POWONSEQCTL_REG);
	sendData(0x64);
	sendData(0x03);
	sendData(0x12);
	sendData(0x81);

	sendCmd(ILI9341_DIVTIMCTL_A_REG);
	sendData(0x85);
	sendData(0x00);
	sendData(0x78);

	sendCmd(ILI9341_POWERCTLA_REG);
	sendData(0x39);
	sendData(0x2C);
	sendData(0x00);
	sendData(0x34);
	sendData(0x02);

	sendCmd(ILI9341_PUMPRATIOCTL_REG);
	sendData(0x20);
	sendCmd(ILI9341_DIVTIMCTL_B_REG);
	sendData(0x00);
	sendData(0x00);

	sendCmd(ILI9341_FRAMECTL_NOR_REG);
	sendData(0x00);
	sendData(0x1B);

	sendCmd(ILI9341_FUNCTONCTL_REG);    // Display Function Control
	sendData(0x08);
	sendData(0x82);
	sendData(0x27);

	sendCmd(ILI9341_POWERCTL1_REG);    	//Power control
	sendData(0x10);   	//VRH[5:0]

	sendCmd(ILI9341_POWERCTL2_REG);    	//Power control
	sendData(0x10);   	//SAP[2:0];BT[3:0]

	sendCmd(ILI9341_VCOMCTL1_REG);    	//VCM control
	sendData(0x45); //Contrast
	sendData(0x15);

	sendCmd(ILI9341_VCOMCTL2_REG);    	//VCM control2
	sendData(0x90);

	sendCmd(ILI9341_MEMACCESS_REG);    	// Memory Access Control
	sendData(0xE8);  //my,mx,mv,ml,BGR,mh,0.0

	sendCmd(ILI9341_PIXFORMATSET_REG);
	sendData(0x55);

	sendCmd(ILI9341_ENABLE_3G_REG);    	// 3Gamma Function Disable
	sendData(0x00);

	DelayManager::DelayMs(200);

	sendCmd(ILI9341_GAMMASET_REG);    	//Gamma curve selected
	sendData(0x01);

	sendCmd(ILI9341_POSGAMMACORRECTION_REG);    	//Set Gamma
	sendData(0x0F);
	sendData(0x31);
	sendData(0x2B);
	sendData(0x0C);
	sendData(0x0E);
	sendData(0x08);
	sendData(0x4E);
	sendData(0xF1);
	sendData(0x37);
	sendData(0x07);
	sendData(0x10);
	sendData(0x03);
	sendData(0x0E);
	sendData(0x09);
	sendData(0x00);


	sendCmd(ILI9341_NEGGAMMACORRECTION_REG);    	//Set Gamma
	sendData(0x00);
	sendData(0x0E);
	sendData(0x14);
	sendData(0x03);
	sendData(0x11);
	sendData(0x07);
	sendData(0x31);
	sendData(0xC1);
	sendData(0x48);
	sendData(0x08);
	sendData(0x0F);
	sendData(0x0C);
	sendData(0x31);
	sendData(0x36);
	sendData(0x0F);

	sendCmd(ILI9341_SLEEP_OUT_REG);    	//Exit Sleep
	DelayManager::DelayMs(200);
	sendCmd(ILI9341_DISPLAYON_REG);   	//Display on
	DelayManager::DelayMs(200);

	//sendCmd(ILI9341_WRITEBRIGHT_REG);   	//Change brightness
	//sendData(0x50);
	
	sendCmd(ILI9341_MEMORYWRITE_REG);
	for(uint16_t i = 0; i < 1000; i++)
	{
		sendWord(0x0000);
	}

	switchCs(1);
	setSPISpeed(this->spiPrescaler);
	isOk = true;
}


void ILI9341::enable(short on) {
	switchCs(0);
	if (on == 0) {
		sendCmd(ILI9341_DISPLAYOFF_REG);
		isOk = false;
	} else {
		sendCmd(ILI9341_DISPLAYON_REG);
		isOk = true;
	}
	switchCs(1);
	DelayManager::DelayMs(100);
}


void ILI9341::sleep(short on){
	switchCs(0);
	if (on == 0) {
		sendCmd(ILI9341_SLEEP_OUT_REG);
	} else {
		sendCmd(ILI9341_SLEEP_ENTER_REG);
	}
	switchCs(1);
	DelayManager::DelayMs(100);
}

void ILI9341::clear(const uint16_t color)
{
	fillScreen(TFT_MIN_X, TFT_MIN_Y, TFT_MAX_X, TFT_MAX_Y, color);
}

void ILI9341::setCol(uint16_t StartCol, uint16_t EndCol)
{
	sendCmd(ILI9341_COLADDRSET_REG);    // Column Command address
	sendWords(StartCol, EndCol);
}

void ILI9341::setPage(uint16_t StartPage, uint16_t EndPage)
{
	sendCmd(ILI9341_PAGEADDRSET_REG);   // Page Command address
	sendWords(StartPage, EndPage);
}

void ILI9341::setWindow(uint16_t startX, uint16_t startY, uint16_t stopX, uint16_t stopY)
{
	if (isLandscape)
	{
		setPage(startX, stopX);
		setCol(startY, stopY);	
	}
	else
	{
		setPage(startY, stopY);
		setCol(startX, stopX);
	}
}	

void ILI9341::setLandscape()
{
	isLandscape = true;
}

void ILI9341::setPortrait()
{
	isLandscape = false;
}

void ILI9341::fillScreen(uint16_t xstart, uint16_t ystart, uint16_t xstop, uint16_t ystop, uint16_t color)
{
	while (this->isDataSending){}; //wait until all data wasn't sended
	const uint16_t max_buf_size = 65535;
	uint32_t pixels = (xstop - xstart + 1) * (ystop - ystart + 1);

	switchCs(0);   // CS=0;
	setWindow(xstart, ystart, xstop, ystop);
	sendCmd(ILI9341_MEMORYWRITE_REG);

	switchRs(1);

	if (!disableDMA) {
		initDMAforSendSPI(true);
		manualCsControl = 1;
		while (pixels > max_buf_size) {
			DMATXStart(&color, max_buf_size);
			pixels -= max_buf_size;
			while (this->isDataSending) {}; //wait until all data wasn't sended
		}
		manualCsControl = 0;
		DMATXStart(&color, pixels);
		while (this->isDataSending) {}; //wait until all data wasn't sended
	}
	else 
	{
		setSPIDataSize(SPI_DATASIZE_16BIT);
		while (pixels) {
			sendWord16bitMode(color);
			pixels--;
		}
		setSPIDataSize(SPI_DATASIZE_8BIT);
		switchCs(1);   // CS=1;
	}
}

void ILI9341::pixelDraw(uint16_t xpos, uint16_t ypos, uint16_t color)
{
	while (this->isDataSending){}; //wait until all data wasn't sended
	switchCs(0);   // CS=0;
	setWindow(xpos, ypos, xpos, ypos);
	sendCmd(ILI9341_MEMORYWRITE_REG);
	sendWord(color);
	switchCs(1);   // CS=1;
}

void ILI9341::bufferDraw(uint16_t x, uint16_t y, uint16_t xsize, uint16_t ysize, uint16_t* buf)
{
	while (this->isDataSending) {}; //wait until all data wasn't sended
	const uint16_t max_buf_size = 65535;
	uint32_t pixels = xsize * ysize;
	
	switchCs(0);   // CS=0
	setWindow(x, y, x + xsize - 1, y + ysize - 1);
	sendCmd(ILI9341_MEMORYWRITE_REG);
	switchRs(1);
	
	if (!disableDMA) {
		initDMAforSendSPI(false);
		manualCsControl = 1;
		while (pixels > max_buf_size) {
			DMATXStart(buf, max_buf_size);
			pixels -= max_buf_size;
			buf += max_buf_size;
			while (this->isDataSending) {}; //wait until all data wasn't sended
		}
		manualCsControl = 0;
		DMATXStart(buf, pixels);
		while (this->isDataSending) {}; //wait until all data wasn't sended
	}
	else 
	{
		setSPIDataSize(SPI_DATASIZE_16BIT);
		for (uint32_t l = 0; l < xsize * ysize; l++) {
			sendWord16bitMode(buf[l]);
		}
		setSPIDataSize(SPI_DATASIZE_8BIT);
		switchCs(1);   // CS=1;
	}
	while (this->isDataSending); //wait until all data wasn't sended
}

void ILI9341::lineDraw(uint16_t ypos, uint16_t* line,  uint32_t size)
{
	bufferDraw(0, ypos, size, 1, line);
}

void ILI9341::drawBorder(uint16_t xpos, uint16_t ypos, uint16_t width, uint16_t height, uint16_t bw, uint16_t color)
{
	fillScreen(xpos, ypos, xpos + bw, ypos + height, color);
	fillScreen(xpos + bw, ypos + height - bw, xpos + width, ypos + height, color);
	fillScreen(xpos + width - bw, ypos, xpos + width, ypos + height - bw, color);
	fillScreen(xpos + bw, ypos, xpos + width - bw, ypos + bw, color);
}

void ILI9341::sendByteInt(const uint8_t byte)
{
	HAL_SPI_Transmit(spi, (uint8_t*)&byte, 1, 100);
	/*while (!__HAL_SPI_GET_FLAG(spi, SPI_FLAG_TXE)){};
	spi->Instance->DR = byte;*/
}

void ILI9341::sendWordInt(const uint16_t data)
{
	HAL_SPI_Transmit(spi, (uint8_t*)&data, 2, 100);
	/*while (!__HAL_SPI_GET_FLAG(spi, SPI_FLAG_TXE)){};
	spi->Instance->DR = data;*/
}

void ILI9341::sendCmd(const uint8_t cmd)
{
	switchRs(0);
	sendByteInt(cmd);
	while (__HAL_SPI_GET_FLAG(spi, SPI_FLAG_BSY)) {};
}

void ILI9341::sendData(const uint8_t data)
{
	switchRs(1);
	sendByteInt(data);
	while (__HAL_SPI_GET_FLAG(spi, SPI_FLAG_BSY)) {};
}

void ILI9341::sendWord(const uint16_t data)
{
	switchRs(1);
	sendByteInt(data >> 8);
	sendByteInt(data & 0xFF);
	while (__HAL_SPI_GET_FLAG(spi, SPI_FLAG_BSY)) {};
}

void ILI9341::sendWord16bitMode(const uint16_t data)
{
	switchRs(1);
	sendWordInt(data);
	while (__HAL_SPI_GET_FLAG(spi, SPI_FLAG_BSY)) {};
}

void ILI9341::sendWords(const uint16_t data1, const uint16_t data2)
{
	switchRs(1);
	sendByteInt(data1 >> 8);
	sendByteInt(data1 & 0xFF);
	sendByteInt(data2 >> 8);
	sendByteInt(data2 & 0xFF);
	while (__HAL_SPI_GET_FLAG(spi, SPI_FLAG_BSY)) {};
}

void ILI9341::initDMAforSendSPI(const uint8_t singleColor)
{
	static DMA_HandleTypeDef dmaSpi5TxHandle;
	dmaSpi5TxHandle.Instance = DMA2_Stream4;
	dmaSpi5TxHandle.Init.Channel = DMA_CHANNEL_2;
	dmaSpi5TxHandle.Init.Direction = DMA_MEMORY_TO_PERIPH;
	dmaSpi5TxHandle.Init.PeriphInc = DMA_PINC_DISABLE;
	dmaSpi5TxHandle.Init.MemInc = singleColor ? DMA_MINC_DISABLE : DMA_MINC_ENABLE;
	dmaSpi5TxHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
	dmaSpi5TxHandle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
	dmaSpi5TxHandle.Init.Mode = DMA_NORMAL;
	dmaSpi5TxHandle.Init.Priority = DMA_PRIORITY_HIGH;
	dmaSpi5TxHandle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	dmaSpi5TxHandle.Init.MemBurst = DMA_MBURST_SINGLE;
	dmaSpi5TxHandle.Init.PeriphBurst = DMA_PBURST_SINGLE;
	HAL_DMA_Init(&dmaSpi5TxHandle);

	__HAL_LINKDMA(spi, hdmatx, dmaSpi5TxHandle);
	HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);
}

void ILI9341::DMATXStart(uint16_t* buffer, const uint16_t size)
{
	if (!isOk) return;
	setSPIDataSize(SPI_DATASIZE_16BIT);
	HAL_SPI_Transmit_DMA(spi, (uint8_t*)buffer, size);
	this->isDataSending = 1;
}

void ILI9341::DMATXInterrupt()
{
	if (!isOk && isDataSending) return;
	HAL_DMA_IRQHandler(spi->hdmatx);
}

void ILI9341::DMATXCompleted()
{
	if (!isOk) return;
	setSPIDataSize(SPI_DATASIZE_8BIT);
	if (!manualCsControl) {
		switchCs(1);   // CS=1;
	}
	this->isDataSending = 0;
}

uint8_t ILI9341::IsDataSending()
{
	return this->isDataSending;
}


void ILI9341::resetIsDataSending()
{
	this->isDataSending = 0;
}

bool ILI9341::isReady()
{
	return this->isOk;
}

void ILI9341::setDisableDMA(uint8_t isDisable)
{
	this->disableDMA = isDisable; 
}


void ILI9341::setColor(uint16_t color, uint16_t bgColor)
{
	this->color = color;
	this->bgColor = bgColor;
}

uint16_t ILI9341::RGB888ToRGB565(uint8_t r, uint8_t g, uint8_t b)
{
	uint16_t r5 = (uint16_t)((r * 249 + 1014) >> 11);
	uint16_t g6 = (uint16_t)((g * 253 + 505) >> 10);
	uint16_t b5 = (uint16_t)((b * 249 + 1014) >> 11);
	return (uint16_t)(r5 << 11 | g6 << 5 | b5);
}

void ILI9341::switchCs(const uint8_t BitVal)
{
	if (BitVal != GPIO_PIN_RESET) {
		csPort->BSRR = csPin;
	} else {
		csPort->BSRR = (uint32_t)csPin << 16U;
	}
}

void ILI9341::switchRs(const uint8_t BitVal)
{
	if (BitVal != GPIO_PIN_RESET) {
		rsPort->BSRR = rsPin;
	} else {
		rsPort->BSRR = (uint32_t)rsPin << 16U;
	}
}

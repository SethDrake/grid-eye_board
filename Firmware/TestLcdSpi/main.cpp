
#include "main.h"
#include "sdram.h"
#include "thermal.h"
#include "delay.h"

SPI_HandleTypeDef lcdSpiHandle;
I2C_HandleTypeDef i2cHandle;

SDRAM sdram;
ILI9341 display;
Framebuffer fbMain;
Framebuffer fbInfo;
IRSensor irSensor;

__IO uint8_t vis_mode = 1;
__IO uint8_t sensorReady = 0;

osThreadId LEDThread1Handle, LEDThread2Handle, GridEyeThreadHandle, ReadKeysThreadHandle, DrawThreadHandle;

static void LED_Thread1(void const *argument);
static void LED_Thread2(void const *argument);
static void GridEye_Thread(void const *argument);
static void ReadKeys_Thread(void const *argument);
static void Draw_Thread(void const *argument);

static void SystemClock_Config();
static void GPIO_Config();
static void SPI_Config();
static void I2C_Config();


/**
* @brief  System Clock Configuration
*         The system Clock is configured as follow :
*            System Clock source            = PLL (HSE)
*            SYSCLK(Hz)                     = 240000000
*            HCLK(Hz)                       = 240000000
*            AHB Prescaler                  = 1
*            APB1 Prescaler                 = 4
*            APB2 Prescaler                 = 2
*            HSE Frequency(Hz)              = 8000000
*            PLL_M                          = 4
*            PLL_N                          = 240
*            PLL_P                          = 2
*            PLL_Q                          = 10
*            VDD(V)                         = 3.3
*            Main regulator output voltage  = Scale1 mode
*            Flash Latency(WS)              = 5
* @retval None
*/
static void SystemClock_Config()
{
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;

	/* Enable Power Control clock */
	__HAL_RCC_PWR_CLK_ENABLE();

	/* The voltage scaling allows optimizing the power consumption when the device is
	clocked below the maximum system frequency, to update the voltage scaling value
	regarding system frequency refer to product datasheet.  */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/* Enable HSE Oscillator and activate PLL with HSE as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 240;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 10;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);

	/* Activate the Over-Drive mode */
	HAL_PWREx_EnableOverDrive();

	/* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
	clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

	/*##-2- LTDC Clock Configuration ###########################################*/
	/* LCD clock configuration */
	/* PLLSAI_VCO Input = HSE_VALUE/PLL_M = 2 MHz */
	/* PLLSAI_VCO Output = PLLSAI_VCO Input * PLLSAIN = 384 MHz */
	/* PLLLCDCLK = PLLSAI_VCO Output/PLLSAIR = 384/4 = 96 MHz */
	/* LTDC clock frequency = PLLLCDCLK / RCC_PLLSAIDIVR_8 = 96/8 = 12 MHz */
	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
	PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
	PeriphClkInitStruct.PLLSAI.PLLSAIR = 4;
	PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_8;
	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
}

static void GPIO_Config()
{
	__GPIOA_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
	__GPIOG_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.Pin = BTN_PIN;
	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Pull = GPIO_PULLDOWN;
	GPIO_InitStructure.Speed = GPIO_SPEED_FAST;
	HAL_GPIO_Init(BTN_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.Pin = LED_1_PIN | LED_2_PIN;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LED_PORT, &GPIO_InitStructure);
}

static void SPI_Config()
{
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_SPI5_CLK_ENABLE();

	/* configure SPI SCK, MOSI and MISO */
	GPIO_InitTypeDef   GPIO_InitStructure;
	GPIO_InitStructure.Pin = (GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
	GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStructure.Pull = GPIO_PULLDOWN;
	GPIO_InitStructure.Speed = GPIO_SPEED_MEDIUM;
	GPIO_InitStructure.Alternate = GPIO_AF5_SPI5;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStructure);
	
	lcdSpiHandle.Instance = SPI5;
	/* SPI baudrate is set to (PCLK2/SPI_BaudRatePrescaler = 120/8 = 15 MHz)*/
	lcdSpiHandle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	lcdSpiHandle.Init.Direction = SPI_DIRECTION_2LINES;
	lcdSpiHandle.Init.CLKPhase = SPI_PHASE_1EDGE;
	lcdSpiHandle.Init.CLKPolarity = SPI_POLARITY_LOW;
	lcdSpiHandle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLED;
	lcdSpiHandle.Init.CRCPolynomial = 7;
	lcdSpiHandle.Init.DataSize = SPI_DATASIZE_8BIT;
	lcdSpiHandle.Init.FirstBit = SPI_FIRSTBIT_MSB;
	lcdSpiHandle.Init.NSS = SPI_NSS_SOFT;
	lcdSpiHandle.Init.TIMode = SPI_TIMODE_DISABLED;
	lcdSpiHandle.Init.Mode = SPI_MODE_MASTER;
	HAL_SPI_Init(&lcdSpiHandle);
}

static void I2C_Config()
{
	if (HAL_I2C_GetState(&i2cHandle) == HAL_I2C_STATE_RESET)
	{
		i2cHandle.Instance = THERMAL_I2C;
		i2cHandle.Init.ClockSpeed = 400000;
		i2cHandle.Init.DutyCycle = I2C_DUTYCYCLE_2;
		i2cHandle.Init.OwnAddress1 = 0;
		i2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
		i2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
		i2cHandle.Init.OwnAddress2 = 0;
		i2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
		i2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;

		__HAL_RCC_GPIOC_CLK_ENABLE();
		__HAL_RCC_GPIOA_CLK_ENABLE();

		GPIO_InitTypeDef  GPIO_InitStruct;
		/* Configure I2C Tx as alternate function  */
		GPIO_InitStruct.Pin = GPIO_PIN_8;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* Configure I2C Rx as alternate function  */
		GPIO_InitStruct.Pin = GPIO_PIN_9;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);


		/* Configure the Discovery I2Cx peripheral -------------------------------*/
		/* Enable I2C3 clock */
		__HAL_RCC_I2C3_CLK_ENABLE();

		/* Force the I2C Peripheral Clock Reset */
		__HAL_RCC_I2C3_FORCE_RESET();

		/* Release the I2C Peripheral Clock Reset */
		__HAL_RCC_I2C3_RELEASE_RESET();

		HAL_I2C_Init(&i2cHandle);
	}
}

int main()
{
  /* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */

	HAL_Init();
	
	SystemClock_Config();
	GPIO_Config();
	SPI_Config();
	I2C_Config();

	sdram.init();

	display.setupHw(&lcdSpiHandle, SPI_BAUDRATEPRESCALER_8, GPIOD, GPIO_PIN_13, GPIOC, GPIO_PIN_2);
	display.init();
	display.clear(COLOR_BLACK);

	fbMain.init(&display, THERMAL_FB_ADDR, 240, 240, COLOR_WHITE, COLOR_BLACK);
	fbMain.setWindowPos(0, 0);
	fbMain.setOrientation(LANDSCAPE);
	fbMain.clear(COLOR_BLACK);
	fbMain.redraw();

	fbInfo.init(&display, INFO_FB_ADDR, 80, 240, COLOR_WHITE, COLOR_BLACK);
	fbInfo.setWindowPos(240, 0);
	fbInfo.setOrientation(LANDSCAPE);
	fbInfo.clear(COLOR_BLACK);
	
	fbInfo.redraw();

	irSensor.init(&i2cHandle, THERMAL_FB_ADDR, THERMAL_RESOLUTION, THERMAL_RESOLUTION, ALTERNATE_COLOR_SCHEME);

	/* Thread 1 definition */
	osThreadDef(LED1, LED_Thread1, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(LED2, LED_Thread2, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(GRID_EYE, GridEye_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(READ_KEYS, ReadKeys_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(DRAW, Draw_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE + 128);
  
	LEDThread1Handle = osThreadCreate(osThread(LED1), NULL);
	LEDThread2Handle = osThreadCreate(osThread(LED2), NULL);
	GridEyeThreadHandle = osThreadCreate(osThread(GRID_EYE), NULL);
	ReadKeysThreadHandle = osThreadCreate(osThread(READ_KEYS), NULL);
	DrawThreadHandle = osThreadCreate(osThread(DRAW), NULL);
  
	/* Start scheduler */
	osKernelStart();

	  /* We should never get here as control is now taken by the scheduler */
	for (;;)
		;
}


/**
  * @brief  Toggle LED1
  * @param  thread not used
  * @retval None
  */
static void LED_Thread1(void const *argument)
{
	(void) argument;
  
	for (;;)
	{
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);
		osDelay(2000);
		
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);
		osThreadSuspend(LEDThread2Handle);
		osDelay(2000);
		
		osThreadResume(LEDThread2Handle);
	}
}

/**
  * @brief  Toggle LED2 thread
  * @param  argument not used
  * @retval None
  */
static void LED_Thread2(void const *argument)
{
	uint32_t count;
	(void) argument;
  
	for (;;)
	{
		HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_14);
		osDelay(200);
	}
}

static void GridEye_Thread(void const *argument)
{
	(void)argument;
	for (;;)
	{
		irSensor.readImage();
		if (!sensorReady)
		{
			sensorReady = true;
		}
		osDelay(90);
	}
}

static void Draw_Thread(void const *argument)
{
	(void)argument;

	TickType_t xExecutionTime = 0;
	uint8_t minTemp = 0;
	uint8_t maxTemp = 0;
	uint8_t coldDotX = 0;
	uint8_t coldDotY = 0;
	uint8_t hotDotX = 0;
	uint8_t hotDotY = 0;

	uint16_t cpuUsage = 0;
	const uint8_t hpUpdDelay = 6;
	uint8_t cntr = hpUpdDelay;
	bool oneTimeActionDone = false;

	for (;;)
	{
		if (sensorReady)
		{
			const TickType_t xTime1 = xTaskGetTickCount();
			if (!oneTimeActionDone)
			{
				irSensor.setFbAddress(INFO_FB_ADDR);
				irSensor.drawGradient(&fbInfo, 6, 20, 16, 175);
				oneTimeActionDone = true;
			}

			if (cntr >= hpUpdDelay)
			{
				cntr = 0;
				const uint8_t hotDot = irSensor.getHotDotIndex();
				hotDotX = hotDot / 8;
				hotDotY = hotDot % 8;
				const uint8_t coldDot = irSensor.getColdDotIndex();
				coldDotX = coldDot / 8;
				coldDotY = coldDot % 8;
				cpuUsage = osGetCPUUsage();
				maxTemp = irSensor.getMaxTemp();
				minTemp = irSensor.getMinTemp();

				fbInfo.printf(4, 200, "VM: %u", vis_mode);
				fbInfo.printf(4, 230, "CPU: %u%%", cpuUsage);
				fbInfo.printf(4, 215, "T: %04u", xExecutionTime);
				fbInfo.printf(4, 5, COLOR_RED, COLOR_BLACK, "MAX:%u\x81", maxTemp);
				fbInfo.printf(4, 180, COLOR_GREEN, COLOR_BLACK, "MIN:%u\x81", minTemp);
				fbInfo.redraw();
			}
			cntr++;

			irSensor.setFbAddress(THERMAL_FB_ADDR);
			irSensor.visualizeImage(THERMAL_RESOLUTION, THERMAL_RESOLUTION, vis_mode);
			fbMain.printf(hotDotX * (THERMAL_RESOLUTION / 8), hotDotY * (THERMAL_RESOLUTION / 8), COLOR_RED, COLOR_TRANSP, "%u\x81", maxTemp);
			fbMain.printf(coldDotX * (THERMAL_RESOLUTION / 8), coldDotY * (THERMAL_RESOLUTION / 8), COLOR_GREEN, COLOR_TRANSP, "%u\x81", minTemp);
			fbMain.redraw();

			const TickType_t xTime2 = xTaskGetTickCount();
			xExecutionTime = xTime2 - xTime1;
		}

		osDelay(50);
	}
}

static void ReadKeys_Thread(void const *argument)
{
	(void)argument;
	bool isPressed = false;
	for (;;)
	{
		const bool isKeyPressed = HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN);
		if (isKeyPressed)
		{
			if (isPressed)
			{
				osDelay(350);
				continue;
			}
			vis_mode++;
			if (vis_mode > 2)
			{
				vis_mode = 0;
			}

			isPressed = true;
		}
		else
		{
			isPressed = false;
		}
		osDelay(75);
	}
}


void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi->Instance == DISPLAY_SPI)
	{
		display.DMATXCompleted();
	}
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi->Instance == DISPLAY_SPI)
	{
		display.DMATXCompleted();
	}
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, const char *taskName)
{
	Error_Handler(42);
}

void Error_Handler(const uint8_t source)
{
	uint8_t k = source;
	while (true) {}
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
	while (1)
	{
	}
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

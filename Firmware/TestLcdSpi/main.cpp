#include "main.h"
#include "sdram.h"
#include "thermal.h"

SPI_HandleTypeDef lcdSpiHandle;
I2C_HandleTypeDef ti2cHandle;
I2C_HandleTypeDef ci2cHandle;
TIM_HandleTypeDef tim10Handle;

SDRAM sdram;
ILI9341 display;
OV7670 camera;
Framebuffer fbMain;
Framebuffer fbInfo;
Framebuffer fbCamera;
IRSensor irSensor;

__IO uint8_t vis_mode = 1;
__IO uint8_t sensorReady = 0;

osThreadId LEDThread1Handle, LEDThread2Handle, GridEyeThreadHandle, ReadKeysThreadHandle, DrawThreadHandle, CameraThreadHandle;

static void LED_Thread1(void const *argument);
static void LED_Thread2(void const *argument);
static void GridEye_Thread(void const *argument);
static void ReadKeys_Thread(void const *argument);
static void Draw_Thread(void const *argument);
static void Camera_Thread(void const *argument);

static void SystemClock_Config();
static void GPIO_Config();
static void SPI_Config();
static void I2C_Config();
static void TIM_Config();


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
	__GPIOB_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
	__GPIOG_CLK_ENABLE();
	__GPIOF_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.Pin = BTN_PIN;
	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Pull = GPIO_PULLDOWN;
	GPIO_InitStructure.Speed = GPIO_SPEED_FAST;
	HAL_GPIO_Init(BTN_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.Pin = GREEN_LED_PIN | RED_LED_PIN;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LED_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.Pin = CAM_PCLK_PIN;
	GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(CAM_PCLK_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.Pin = CAM_HREF_PIN;
	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(CAM_HREF_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.Pin = CAM_RESET_PIN;
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	GPIO_InitStructure.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(CAM_RESET_PORT, &GPIO_InitStructure);
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
	if (HAL_I2C_GetState(&ti2cHandle) == HAL_I2C_STATE_RESET)
	{
		ti2cHandle.Instance = THERMAL_I2C;
		ti2cHandle.Init.ClockSpeed = 500000;
		ti2cHandle.Init.DutyCycle = I2C_DUTYCYCLE_2;
		ti2cHandle.Init.OwnAddress1 = 0;
		ti2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
		ti2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
		ti2cHandle.Init.OwnAddress2 = 0;
		ti2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
		ti2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;

		__HAL_RCC_GPIOC_CLK_ENABLE();
		__HAL_RCC_GPIOA_CLK_ENABLE();

		GPIO_InitTypeDef  GPIO_InitStruct;
		/* Configure I2C3 SCL as alternate function  */
		GPIO_InitStruct.Pin = GPIO_PIN_8;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* Configure I2C3 SDA as alternate function  */
		GPIO_InitStruct.Pin = GPIO_PIN_9;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

		__HAL_RCC_I2C3_CLK_ENABLE();
		__HAL_RCC_I2C3_FORCE_RESET();
		__HAL_RCC_I2C3_RELEASE_RESET();
		HAL_I2C_Init(&ti2cHandle);
	}
	
	if (HAL_I2C_GetState(&ci2cHandle) == HAL_I2C_STATE_RESET)
	{
		ci2cHandle.Instance = CAMERA_I2C;
		ci2cHandle.Init.ClockSpeed = 100000;
		ci2cHandle.Init.DutyCycle = I2C_DUTYCYCLE_2;
		ci2cHandle.Init.OwnAddress1 = 0;
		ci2cHandle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
		ci2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
		ci2cHandle.Init.OwnAddress2 = 0;
		ci2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
		ci2cHandle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;

		__HAL_RCC_GPIOB_CLK_ENABLE();

		GPIO_InitTypeDef  GPIO_InitStruct;
		/* Configure I2C1 SCL & SDA as alternate function  */
		GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
		GPIO_InitStruct.Pull = GPIO_PULLUP;
		GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		__HAL_RCC_I2C1_CLK_ENABLE();
		__HAL_RCC_I2C1_FORCE_RESET();
		__HAL_RCC_I2C1_RELEASE_RESET();
		HAL_I2C_Init(&ci2cHandle);
	}
}

static void TIM_Config()
{
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_TIM10_CLK_ENABLE();

	GPIO_InitTypeDef  GPIO_InitStruct;
	GPIO_InitStruct.Pin = CAM_XCLK_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF3_TIM10;
	HAL_GPIO_Init(CAM_XCLK_PORT, &GPIO_InitStruct);

	const uint16_t period = (SystemCoreClock / 8000000); // 12MHz

	tim10Handle.Instance = TIM10;
	tim10Handle.Init.Period = period - 1;
	tim10Handle.Init.Prescaler = 0;
	tim10Handle.Init.ClockDivision = 0;
	tim10Handle.Init.CounterMode = TIM_COUNTERMODE_UP;
	HAL_TIM_Base_Init(&tim10Handle);
	HAL_TIM_PWM_Init(&tim10Handle);

	TIM_OC_InitTypeDef tim10OCConfig;
	tim10OCConfig.OCMode = TIM_OCMODE_PWM1;
	tim10OCConfig.OCIdleState = TIM_OCIDLESTATE_SET;
	tim10OCConfig.Pulse = period / 2;
	tim10OCConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
	tim10OCConfig.OCFastMode = TIM_OCFAST_ENABLE;
	HAL_TIM_PWM_ConfigChannel(&tim10Handle, &tim10OCConfig, TIM_CHANNEL_1);

	HAL_TIM_Base_Start(&tim10Handle);
	HAL_TIM_PWM_Start(&tim10Handle, TIM_CHANNEL_1);
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

	SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
	
	SystemClock_Config();
	GPIO_Config();
	SPI_Config();
	I2C_Config();
	TIM_Config();

	sdram.init();

	display.setupHw(&lcdSpiHandle, SPI_BAUDRATEPRESCALER_2, GPIOD, GPIO_PIN_13, GPIOC, GPIO_PIN_2);
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
	
	fbCamera.init(&display, CAMERA_FB_ADDR, 176, 144, COLOR_WHITE, COLOR_BLACK);
	fbCamera.setWindowPos(0, 0);
	fbCamera.setOrientation(LANDSCAPE);
	fbCamera.clear(COLOR_WHITE);
	fbCamera.redraw();

	irSensor.init(&ti2cHandle, THERMAL_FB_ADDR, THERMAL_RESOLUTION, THERMAL_RESOLUTION, ALTERNATE_COLOR_SCHEME);

	camera.init(&ci2cHandle, CAM_HREF_PORT, CAM_HREF_PIN, CAM_RESET_PORT, CAM_RESET_PIN, CAMERA_FB_ADDR);

	camera.captureFrame();

	/*while(!camera.isFrameReady())
	{
	}*/

	/* Thread 1 definition */
	osThreadDef(LED1, LED_Thread1, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(LED2, LED_Thread2, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(GRID_EYE, GridEye_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(READ_KEYS, ReadKeys_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(DRAW, Draw_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE + 1024);
	osThreadDef(CAMERA, Camera_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE + 1024);
  
	LEDThread1Handle = osThreadCreate(osThread(LED1), nullptr);
	LEDThread2Handle = osThreadCreate(osThread(LED2), nullptr);
	GridEyeThreadHandle = osThreadCreate(osThread(GRID_EYE), nullptr);
	ReadKeysThreadHandle = osThreadCreate(osThread(READ_KEYS), nullptr);
	DrawThreadHandle = osThreadCreate(osThread(DRAW), nullptr);
	CameraThreadHandle = osThreadCreate(osThread(CAMERA), nullptr);
  
	/* Start scheduler */
	osKernelStart();

	  /* We should never get here as control is now taken by the scheduler */
	for (;;);
}

static void LED_Thread1(void const *argument)
{
	(void) argument;
  
	for (;;)
	{
		HAL_GPIO_WritePin(LED_PORT, GREEN_LED_PIN, GPIO_PIN_SET);
		osDelay(2000);
		
		HAL_GPIO_WritePin(LED_PORT, GREEN_LED_PIN, GPIO_PIN_RESET);
		osThreadSuspend(LEDThread2Handle);
		osDelay(2000);
		
		osThreadResume(LEDThread2Handle);
	}
}

static void LED_Thread2(void const *argument)
{
	(void) argument;
  
	for (;;)
	{
		HAL_GPIO_TogglePin(LED_PORT, RED_LED_PIN);
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

	const uint8_t hpUpdDelay = 8;
	uint8_t cntr = hpUpdDelay;
	bool oneTimeActionDone = false;

	for (;;)
	{
		if (sensorReady && camera.isFrameReady())
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
				hotDotY = hotDot / 8;
				hotDotX = hotDot % 8;
				const uint8_t coldDot = irSensor.getColdDotIndex();
				coldDotY = coldDot / 8;
				coldDotX = coldDot % 8;
				maxTemp = irSensor.getMaxTemp();
				minTemp = irSensor.getMinTemp();
				const uint16_t cpuUsage = osGetCPUUsage();

				fbInfo.printf(4, 200, "VM: %u", vis_mode);
				fbInfo.printf(4, 230, "CPU: %u%%", cpuUsage);
				fbInfo.printf(4, 215, "T: %04u", xExecutionTime);
				fbInfo.printf(4, 5, COLOR_RED, COLOR_BLACK, "MAX:%u\x81", maxTemp);
				fbInfo.printf(4, 180, COLOR_GREEN, COLOR_BLACK, "MIN:%u\x81", minTemp);
				fbInfo.redraw();
			}
			cntr++;

			irSensor.setFbAddress(THERMAL_FB_ADDR);
			/*irSensor.visualizeImage(THERMAL_RESOLUTION, THERMAL_RESOLUTION, vis_mode);
			fbMain.printf(hotDotX * (THERMAL_RESOLUTION / 8), hotDotY * (THERMAL_RESOLUTION / 8), COLOR_BLACK, COLOR_TRANSP, "%u\x81", maxTemp);
			fbMain.printf(coldDotX * (THERMAL_RESOLUTION / 8), coldDotY * (THERMAL_RESOLUTION / 8), COLOR_GREEN, COLOR_TRANSP, "%u\x81", minTemp);
			fbMain.redraw();*/

			const TickType_t xTime2 = xTaskGetTickCount();
			xExecutionTime = xTime2 - xTime1;
		}

		osDelay(75);
	}
}

static void Camera_Thread(void const *argument)
{
	(void)argument;

	for (;;)
	{
		if (camera.isFrameReady())
		{
			fbCamera.redraw();
			camera.captureFrame();
		}
		osDelay(70);
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
		osDelay(150);
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

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	camera.PclkInterrupt();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, const char *taskName)
{
	Error_Handler(42, nullptr);
}

void Error_Handler(const uint8_t reason, unsigned int * hardfault_args)
{
	HAL_GPIO_WritePin(LED_PORT, GREEN_LED_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LED_PORT, RED_LED_PIN, GPIO_PIN_SET);

	fbMain.init(&display, SDRAM_DEVICE_ADDR, 320, 240, COLOR_WHITE, COLOR_BLUE);
	fbMain.setWindowPos(0, 0);
	fbMain.setOrientation(LANDSCAPE);
	fbMain.clear(COLOR_BLUE);

	if (reason == 0)
	{
		const unsigned int stacked_r0 = ((unsigned long)hardfault_args[1]);
		const unsigned int stacked_r1 = ((unsigned long)hardfault_args[2]);
		const unsigned int stacked_r2 = ((unsigned long)hardfault_args[3]);
		const unsigned int stacked_r3 = ((unsigned long)hardfault_args[4]);
		const unsigned int stacked_r12 = ((unsigned long)hardfault_args[5]);
		const unsigned int stacked_lr = ((unsigned long)hardfault_args[6]);
		const unsigned int stacked_pc = ((unsigned long)hardfault_args[7]);
		const unsigned int stacked_psr = ((unsigned long)hardfault_args[8]);

		fbMain.printf(30, 10, "HARD FAULT DETECTED --- SYSTEM STOPPED");
		fbMain.printf(10, 25, "R0 = %x", stacked_r0);
		fbMain.printf(10, 40, "R1 = %x", stacked_r1);
		fbMain.printf(10, 55, "R2 = %x", stacked_r2);
		fbMain.printf(10, 70, "R3 = %x", stacked_r3);
		fbMain.printf(10, 85, "R12 = %x", stacked_r12);
		fbMain.printf(10, 100, "LR [R14] = %x", stacked_lr);
		fbMain.printf(10, 115, "PC [R15] = %x", stacked_pc);
		fbMain.printf(10, 130, "PSR = %x", stacked_psr);
		fbMain.printf(10, 220, "SCB_SHCSR = %x", SCB->SHCSR);
	} 
	else
	{
		fbMain.printf(30, 10, "STOP ERROR DETECTED --- REASON: %u");
	}

	fbMain.redraw();
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

	Error_Handler(7, nullptr);
	fbMain.clear(COLOR_BLUE);
	fbMain.printf(1, 10, "Wrong parameters value:");
	fbMain.printf(1, 25, "file %s on line %d", file, line);
	fbMain.redraw();

  /* Infinite loop */
	while (1)
	{
	}
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

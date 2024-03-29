
#include "main.h"
#include "cmsis_os.h"
#include "cpu_utils.h"
#include <framebuffer.h>
#include <thermal.h>

osThreadId LEDThread1Handle, LEDThread2Handle, LTDCThreadHandle, IRSensorThreadHandle, ReadKeysTaskHandle, SwapBuffersTaskHandle; 

LTDC_HandleTypeDef LtdcHandle;
DMA2D_HandleTypeDef dma2dHandle;
Framebuffer fbMainLayer;
Framebuffer fbInfoLayer;
IRSensor irSensor;

volatile uint32_t ReloadFlag = 0;
volatile uint8_t vis_mode = 1;
volatile bool isSensorReady = false;
volatile bool isSensorReadDone = false;
volatile bool isFrameReady = false;
volatile TickType_t xExecutionTime = 0;
volatile uint32_t inWait = 0;


/* Private function prototypes -----------------------------------------------*/
static void LED_Thread1(void const *argument);
static void LED_Thread2(void const *argument);
static void LTDC_Thread(void const *argument);
static void IrSensor_Thread(void const *argument);
static void ReadKeys_Thread(void const *argument);
static void SwapBuffers_Thread(void const *argument);

static void SystemClock_Config();
static void LCD_Config();
static void DMA2D_Config();

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @retval None
  */
int main()
{
	  /* STM32F4xx HAL library initialization:
		   - Configure the Flash prefetch, instruction and Data caches
		   - Configure the Systick to generate an interrupt each 1 msec
		   - Set NVIC Group Priority to 4
		   - Global MSP (MCU Support Package) initialization
		 */
	HAL_Init();  
  
	/* Configure LED3 and LED4 */
	BSP_LED_Init(LED3);
	BSP_LED_Init(LED4);

	BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);
  
	/* Configure the system clock to 168 MHz */
	SystemClock_Config();

	/* Init I2C3 */
	I2Cx_Init();

	DMA2D_Config();

	BSP_SDRAM_Init();
	LCD_Config();

	fbMainLayer.init(&dma2dHandle, 1, FRAMEBUFFER_ADDR, 320, 240, 0xffff, 0x0000);
	fbMainLayer.setOrientation(LANDSCAPE);
	fbMainLayer.clear(0xFF000000);

	fbInfoLayer.init(&dma2dHandle, 1, FRAMEBUFFER2_ADDR, 320, 240, ARGB_COLOR_WHITE | 0x8000, ARGB_COLOR_BLACK);
	fbInfoLayer.setOrientation(LANDSCAPE);
	fbInfoLayer.clear(0x00000000);

	isSensorReady = irSensor.init(&dma2dHandle, 1, FRAMEBUFFER_ADDR, 320, 240, ALTERNATE_COLOR_SCHEME);
  
	osThreadDef(LED3, LED_Thread1, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(LED4, LED_Thread2, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(LDTC, LTDC_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE + 128);
	osThreadDef(IR_SENSOR, IrSensor_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE + 2048);
	osThreadDef(READ_KEYS, ReadKeys_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
	osThreadDef(SWAP_BUFFERS, SwapBuffers_Thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
  
	LEDThread1Handle = osThreadCreate(osThread(LED3), NULL);
	LEDThread2Handle = osThreadCreate(osThread(LED4), NULL);
	LTDCThreadHandle = osThreadCreate(osThread(LDTC), NULL);
	IRSensorThreadHandle = osThreadCreate(osThread(IR_SENSOR), NULL);
	ReadKeysTaskHandle = osThreadCreate(osThread(READ_KEYS), NULL);
	SwapBuffersTaskHandle = osThreadCreate(osThread(SWAP_BUFFERS), NULL);
  
	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */
	while (true)
	{
	}
}

static void showSP()
{
	const uint16_t sp = irSensor.getSubPage();
	if (sp == 0)
	{
		BSP_LED_On(LED3);
	} else {
		BSP_LED_Off(LED3);
	}
}

/**
  * @brief  Toggle LED3 and LED4 thread
  * @param  argument not used
  * @retval None
  */
static void LED_Thread1(void const *argument)
{
	(void) argument;
  
	for (;;)
	{
		/*if (sp == 1)
		{
			BSP_LED_On(LED3);
		} else
		{
			BSP_LED_Off(LED3);
		}*/
		// BSP_LED_Toggle(LED3);
		osDelay(1000);
	}
}

/**
  * @brief  Toggle LED4 thread
  * @param  argument not used
  * @retval None
  */
static void LED_Thread2(void const *argument)
{
	(void) argument;
  
	for (;;)
	{
		osDelay(1000);
	}
}

static void IrSensor_Thread(void const *argument)
{
	(void) argument;
	for (;;)
	{
		if (isSensorReady) {
			const TickType_t xTime1 = xTaskGetTickCount();
			while(!irSensor.isFrameReady())
			{
				inWait = inWait + 1;
				osDelay(5);
			}
			irSensor.readImage(0.95f); //first subpage
			showSP();
			while(!irSensor.isFrameReady())
			{
				inWait = inWait + 1;
				osDelay(5);
			}
			irSensor.readImage(0.95f);// second subpage
			showSP();
			irSensor.findMinAndMaxTemp();
			const TickType_t xTime2 = xTaskGetTickCount();
			xExecutionTime = xTime2 - xTime1;
			isSensorReadDone = true;
		}
		osDelay(10);
	}
}



static void LTDC_Thread(void const *argument)
{
	(void) argument;

	/* reconfigure the layer1 position  without Reloading*/
	//HAL_LTDC_SetWindowPosition_NoReload(&LtdcHandle, 0, 0, 0);
	/* reconfigure the layer2 position  without Reloading*/
	//HAL_LTDC_SetWindowPosition_NoReload(&LtdcHandle, 0, 0, 1);

	uint16_t minTemp = 0;
	uint16_t maxTemp = 0;
	uint16_t coldDotX = 0;
	uint16_t coldDotY = 0;
	uint16_t hotDotX = 0;
	uint16_t hotDotY = 0;

	uint16_t cpuUsage = 0;
	const uint8_t hpUpdDelay = 20;
	uint8_t cntr = hpUpdDelay;
	bool oneTimeActionDone = false;

	for (;;)
	{
		if (isSensorReady)
		{
			// const TickType_t xTime1 = xTaskGetTickCount();

			irSensor.visualizeImage(THERMAL_SCALE, vis_mode);

			if (!oneTimeActionDone && isSensorReadDone)
			{
				irSensor.drawGradient(250, 50, 260, 225);	
				oneTimeActionDone = true;
			}
			
			if (cntr >= hpUpdDelay)
			{
				cntr = 0;
				const uint16_t hotDotIdx = irSensor.getHotDotIndex();
				hotDotX = 31 - (hotDotIdx % 32);
				hotDotY = 23 - (hotDotIdx / 32);
				const uint16_t coldDotIdx = irSensor.getColdDotIndex();
				coldDotX = 31 - (coldDotIdx % 32);
				coldDotY = 23 - (coldDotIdx / 32);
				cpuUsage = osGetCPUUsage();
				maxTemp = (int16_t)irSensor.getMaxTemp();
				minTemp = (int16_t)irSensor.getMinTemp();

				fbInfoLayer.clear(0x00000000);

				fbInfoLayer.printf(hotDotX * THERMAL_SCALE, hotDotY * THERMAL_SCALE, ARGB_COLOR_BLACK | 0x8000, ARGB_COLOR_BLACK, "%u\x81", maxTemp);
				fbInfoLayer.printf(coldDotX * THERMAL_SCALE, coldDotY * THERMAL_SCALE, ARGB_COLOR_GREEN | 0x8000, ARGB_COLOR_BLACK, "%u\x81", minTemp);
				fbInfoLayer.printf(250, 0, "CPU: %u%%", cpuUsage);
				fbInfoLayer.printf(250, 12, "T: %04u", xExecutionTime);
				fbInfoLayer.printf(250, 24, "V: %04u", vis_mode);
				fbInfoLayer.printf(250, 225, ARGB_COLOR_RED | 0x8000, ARGB_COLOR_BLACK, "MAX:%u\x81", maxTemp);
				fbInfoLayer.printf(250, 38, ARGB_COLOR_GREEN | 0x8000, ARGB_COLOR_BLACK, "MIN:%u\x81", minTemp);

				// const TickType_t xTime2 = xTaskGetTickCount();
				// xExecutionTime = xTime2 - xTime1;
			}
			cntr++;
		}
		else
		{
			fbInfoLayer.clear(0x00000000);
			fbInfoLayer.printf(10, 110, "SENSOR ERROR");
		}
				
		osDelay(20);
	}
}

static void SwapBuffers_Thread(void const *argument)
{
	(void) argument;
	for (;;)
	{
		osDelay(2000);
	}
}

static void ReadKeys_Thread(void const *argument)
{
	(void) argument;
	bool isPressed = false;
	for (;;)
	{
		const bool isKeyPressed = BSP_PB_GetState(BUTTON_KEY);
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

/**
  * @brief  Reload Event callback.
  * @param  hltdc: pointer to a LTDC_HandleTypeDef structure that contains
  *                the configuration information for the LTDC.
  * @retval None
  */
void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
	ReloadFlag = 1;
}

/**
  * @brief LCD Configuration.
  * @note  This function Configure the LTDC peripheral :
  *        1) Configure the Pixel Clock for the LCD
  *        2) Configure the LTDC Timing and Polarity
  *        3) Configure the LTDC Layer 1 :
  *           - direct color (RGB565) as pixel format  
  *           - The frame buffer is located at FLASH memory
  *           - The Layer size configuration : 240x160
  *        4) Configure the LTDC Layer 2 :
  *           - direct color (RGB565) as pixel format 
  *           - The frame buffer is located at FLASH memory
  *           - The Layer size configuration : 240x160                      
  * @retval
  *  None
  */
static void LCD_Config()
{  
	LTDC_LayerCfgTypeDef pLayerCfg;
	LTDC_LayerCfgTypeDef pLayerCfg1;

	  /* Initialization of ILI9341 component*/
	ili9341_Init();
  
	/* LTDC Initialization -------------------------------------------------------*/
  
	  /* Polarity configuration */
	  /* Initialize the horizontal synchronization polarity as active low */
	LtdcHandle.Init.HSPolarity = LTDC_HSPOLARITY_AL;
	/* Initialize the vertical synchronization polarity as active low */ 
	LtdcHandle.Init.VSPolarity = LTDC_VSPOLARITY_AL; 
	/* Initialize the data enable polarity as active low */ 
	LtdcHandle.Init.DEPolarity = LTDC_DEPOLARITY_AL; 
	/* Initialize the pixel clock polarity as input pixel clock */  
	LtdcHandle.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  
	/* Timing configuration  (Typical configuration from ILI9341 datasheet)
	    HSYNC=10 (9+1)
	    HBP=20 (29-10+1)
	    ActiveW=240 (269-20-10+1)
	    HFP=10 (279-240-20-10+1)

	    VSYNC=2 (1+1)
	    VBP=2 (3-2+1)
	    ActiveH=320 (323-2-2+1)
	    VFP=4 (327-320-2-2+1)
	*/  

	/* Timing configuration */
	/* Horizontal synchronization width = Hsync - 1 */  
	LtdcHandle.Init.HorizontalSync = 9;
	/* Vertical synchronization height = Vsync - 1 */
	LtdcHandle.Init.VerticalSync = 1;
	/* Accumulated horizontal back porch = Hsync + HBP - 1 */
	LtdcHandle.Init.AccumulatedHBP = 29;
	/* Accumulated vertical back porch = Vsync + VBP - 1 */
	LtdcHandle.Init.AccumulatedVBP = 3; 
	/* Accumulated active width = Hsync + HBP + Active Width - 1 */ 
	LtdcHandle.Init.AccumulatedActiveH = 323;
	/* Accumulated active height = Vsync + VBP + Active Heigh - 1 */
	LtdcHandle.Init.AccumulatedActiveW = 269;
	/* Total height = Vsync + VBP + Active Heigh + VFP - 1 */
	LtdcHandle.Init.TotalHeigh = 327;
	/* Total width = Hsync + HBP + Active Width + HFP - 1 */
	LtdcHandle.Init.TotalWidth = 279;
  
	/* Configure R,G,B component values for LCD background color */
	LtdcHandle.Init.Backcolor.Blue = 0xFF;
	LtdcHandle.Init.Backcolor.Green = 0xFF;
	LtdcHandle.Init.Backcolor.Red = 0xFF;

	LtdcHandle.Instance = LTDC;
  
	/* Layer1 Configuration ------------------------------------------------------*/
  
	/* Windowing configuration */ 
	pLayerCfg.WindowX0 = 0;
	pLayerCfg.WindowX1 = 240;
	pLayerCfg.WindowY0 = 0;
	pLayerCfg.WindowY1 = 320;
  
	/* Pixel Format configuration*/ 
	pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  
	/* Start Address configuration : frame buffer is located at FLASH memory */
	pLayerCfg.FBStartAdress = FRAMEBUFFER_ADDR;
  
	/* Alpha constant (255 totally opaque) */
	pLayerCfg.Alpha = 255;
  
	/* Default Color configuration (configure A,R,G,B component values) */
	pLayerCfg.Alpha0 = 0;
	pLayerCfg.Backcolor.Blue = 0;
	pLayerCfg.Backcolor.Green = 0;
	pLayerCfg.Backcolor.Red = 0;
  
	/* Configure blending factors */
	pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
	pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
  
	/* Configure the number of lines and number of pixels per line */
	pLayerCfg.ImageWidth = 240;
	pLayerCfg.ImageHeight = 320;

/* Layer2 Configuration ------------------------------------------------------*/
  
  /* Windowing configuration */
	pLayerCfg1.WindowX0 = 0;
	pLayerCfg1.WindowX1 = 240;
	pLayerCfg1.WindowY0 = 0;
	pLayerCfg1.WindowY1 = 320;
  
	/* Pixel Format configuration*/ 
	pLayerCfg1.PixelFormat = LTDC_PIXEL_FORMAT_ARGB1555;
  
	/* Start Address configuration : frame buffer is located at FLASH memory */
	pLayerCfg1.FBStartAdress = FRAMEBUFFER2_ADDR;
  
	/* Alpha constant (255 totally opaque) */
	pLayerCfg1.Alpha = 205;
  
	/* Default Color configuration (configure A,R,G,B component values) */
	pLayerCfg1.Alpha0 = 0;
	pLayerCfg1.Backcolor.Blue = 0;
	pLayerCfg1.Backcolor.Green = 0;
	pLayerCfg1.Backcolor.Red = 0;
  
	/* Configure blending factors */
	pLayerCfg1.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
	pLayerCfg1.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
  
	/* Configure the number of lines and number of pixels per line */
	pLayerCfg1.ImageWidth = 240;
	pLayerCfg1.ImageHeight = 320;  
   
	/* Configure the LTDC */  
	if (HAL_LTDC_Init(&LtdcHandle) != HAL_OK)
	{
	  /* Initialization Error */
		Error_Handler(10); 
	}

	  /* Configure the Background Layer*/
	if (HAL_LTDC_ConfigLayer(&LtdcHandle, &pLayerCfg, 0) != HAL_OK)
	{
	  /* Initialization Error */
		Error_Handler(11); 
	}
  
	/* Configure the Foreground Layer*/
	if (HAL_LTDC_ConfigLayer(&LtdcHandle, &pLayerCfg1, 1) != HAL_OK)
	{
	  /* Initialization Error */
		Error_Handler(12); 
	}  
}


void DMA2D_Config()
{
	__HAL_RCC_DMA2D_CLK_ENABLE(); 
	HAL_NVIC_SetPriority(DMA2D_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA2D_IRQn); 
}

void Error_Handler(const uint8_t source)
{
	uint8_t k = source;
	BSP_LED_On(LED4);
	while (true) {}
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
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

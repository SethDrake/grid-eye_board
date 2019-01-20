#include "stm32f4xx_it.h"
#include "cmsis_os.h"
#include "main.h"
#include "delay.h"

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/


/**
  * @brief  This function handles NMI exception.
  * @retval None
  */
void NMI_Handler()
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @retval None
  */
void HardFault_Handler()
{
	__ASM("TST LR, #4");
	__ASM("ITE EQ");
	__ASM("MRSEQ R0, MSP");
	__ASM("MRSNE R0, PSP");
	__ASM("B hard_fault_handler");
}

/**
  * @brief  This function handles Memory Manage exception.
  * @retval None
  */
void MemManage_Handler()
{
	Error_Handler(1, nullptr);
	while (true)
	{
	}
}

/**
  * @brief  This function handles Bus Fault exception.
  * @retval None
  */
void BusFault_Handler()
{
	Error_Handler(2, nullptr);
	while (true)
	{
	}
}

/**
  * @brief  This function handles Usage Fault exception.
  * @retval None
  */
void UsageFault_Handler()
{
	Error_Handler(3, nullptr);
	while (true)
	{
	}
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @retval None
   */
void DebugMon_Handler()
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @retval None
  */
void SysTick_Handler()
{
	HAL_IncTick();
	osSystickHandler();
	DelayManager::SysTickIncrement();
}

void DCMI_IRQHandler()
{
	camera.DCMI_Interrupt();
}

void DMA2_Stream1_IRQHandler()
{
	camera.DCMI_DMA_Interrupt();
}

void DMA2_Stream4_IRQHandler()
{
	display.DMATXInterrupt();
}

void DMA2D_IRQHandler()
{
	fbMain.DMA2D_Interrupt();
}


void hard_fault_handler(unsigned int * hardfault_args)
{
	Error_Handler(0, hardfault_args);
	while(true);
}


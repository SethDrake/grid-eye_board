#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler();
void HardFault_Handler();
void MemManage_Handler();
void BusFault_Handler();
void UsageFault_Handler();
void SVC_Handler();
void DebugMon_Handler();
void PendSV_Handler();
void SysTick_Handler();
void DCMI_IRQHandler();
void DMA2_Stream4_IRQHandler();
void DMA2_Stream1_IRQHandler();

extern void hard_fault_handler(unsigned int * hardfault_args);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_IT_H */

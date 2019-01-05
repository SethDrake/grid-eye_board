
#include "delay.h"
#include <stm32f4xx_hal.h>
#include "cmsis_os.h"

volatile uint32_t DelayManager::timingDelay;
volatile uint64_t DelayManager::sysTickCount;

DelayManager::DelayManager() {
	this->sysTickCount = 0;
	this->timingDelay = 0;
}

DelayManager::~DelayManager() {

}

void DelayManager::TimingDelay_Decrement(void){
  if (timingDelay > 0){
    timingDelay--;
  }
}

void DelayManager::SysTickIncrement(void) {
	sysTickCount++;
	TimingDelay_Decrement();
}

uint64_t DelayManager::GetSysTickCount() {
	return sysTickCount;
}

void DelayManager::Delay(const volatile uint32_t nTime)
{
	osDelay(nTime);
}

void DelayManager::DelayMs(const volatile uint32_t nTime)
{
	HAL_Delay(nTime);
}

void DelayManager::DelayUs(volatile uint32_t nTime)
{
	const uint32_t start = DWT->CYCCNT;
	nTime *= (HAL_RCC_GetHCLKFreq() / 1000000);
	while ((DWT->CYCCNT - start) < nTime);
}


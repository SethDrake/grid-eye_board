
#pragma once
#ifndef __DELAY_H_
#define __DELAY_H_

#include <stm32f4xx_hal.h>

class DelayManager {
private:
	DelayManager();
	~DelayManager();
	
	volatile static uint32_t timingDelay;
	volatile static uint64_t sysTickCount;

public:
	static void Delay(volatile uint32_t nTime);
	static void DelayMs(volatile uint32_t nTime);
	static void DelayUs(volatile uint32_t nTime);
	//static void DelayUs(volatile uint32_t nTime);
	static void TimingDelay_Decrement();
	static void SysTickIncrement();
	static uint64_t GetSysTickCount();
};

#endif

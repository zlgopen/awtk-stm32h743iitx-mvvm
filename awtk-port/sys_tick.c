#include "sys.h"

void sys_tick_init(int SYSCLK)
{
	u32 reload=SYSCLK * 1000;
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
	
	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk;
	SysTick->LOAD=reload; 				
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk; 
}		

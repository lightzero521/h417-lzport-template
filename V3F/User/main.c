#include "debug.h"

extern void lzport_test_user(void);
extern void lzport_test_runner(void);

int main(void)
{
	SystemInit();
	SystemAndCoreClockUpdate();
	Delay_Init();
	USART_Printf_Init(460800);

	printf("SystemClk:%d\r\n", SystemClock);
	printf("V3F SystemCoreClk:%d\r\n", SystemCoreClock);
#if (Run_Core == Run_Core_V3FandV5F)
	NVIC_WakeUp_V5F(Core_V5F_StartAddr);//wake up V5
	HSEM_ITConfig(HSEM_ID0, ENABLE);
	NVIC->SCTLR |= 1<<4;//enable 2 Level interrupt nesting
	RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
	PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE);
	HSEM_ClearFlag(HSEM_ID0);
	printf("V3F wake up\r\n");
	
#elif (Run_Core == Run_Core_V3F)
	Hardware();

#elif (Run_Core == Run_Core_V5F)
	NVIC_WakeUp_V5F(Core_V5F_StartAddr);//wake up V5
	PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE);
	printf("V3F wake up\r\n");
#endif

	lzport_test_user();
	while(1)
	{
		lzport_test_runner();
	}
}

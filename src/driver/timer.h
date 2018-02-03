#ifndef __TIMER_H
#define __TIMER_H
#include "sys.h"
  
extern  u8 Lock_Open_Tim5s;
extern u8 Lock_Close_Tim5s;





enum timer
{
    tim1_cnt,
    tim2_cnt,
	tim3_cnt,
	tim4_cnt,
	tim5_cnt,
	timer_lock,
	timer_uart1,
	timer_uart2,
	timer_uart3,
	timer_gprs,
	timer_at,
	timer_max
};




//如果index对应的时间超时，清零并返回1，未超时返回0
#define IS_TIMEOUT_1MS(index, count)    ((g_tim_cnt[(uint8_t)(index)] >= (count)) ?  	\
                                        ((g_tim_cnt[(uint8_t)(index)] = 0) == 0) : 0)



extern volatile uint32_t g_timer_cnt[(uint8_t)timer_max];

void timer2_init(u16 arr, u16 psc);
u8 timer_is_timeout_1ms(uint8_t type, uint32_t count);
void timer_delay_1ms(uint32_t ms);


 
#endif

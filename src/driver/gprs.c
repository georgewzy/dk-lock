/*************************************************
  Copyright (C), 1988-1999,  Tech. Co., Ltd.
  File name:      
  Author:       Version:        Date:
  Description:   
  Others:         
  Function List:  
    1. ....
  History:         
                   
    1. Date:
       Author:
       Modification:
    2. ...
*************************************************/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/
#include <stdio.h>
#include <string.h>
#include "gprs.h"
#include "bsp.h"
#include "usart.h"
#include "timer.h"
#include "transport.h"





extern usart_buff_t  usart1_rx_buff;
extern usart_buff_t  usart2_rx_buff;


//extern u8 usart2_buff[512];
//extern u16 usart2_cnt;

extern u8 usart2_rx_status;





uint8_t gprs_err_cnt = 0; 	//GPRS错误计数器
uint8_t gprs_status = 0;	//GPRS的状态

uint8_t gprs_rx_buf[512];
uint16_t gprs_rx_cnt = 0;

uint8_t gprs_send_at_flag = 0;	
uint8_t gprs_rx_flag = 0;







u8 PARK_LOCK_Buffer[17] = {0};
extern u8 lock_id[16];
u8 topic_id = 0;

/*
*********************************************************************************************************
*                                          gprs_power_on()
*
* Description : Create application tasks.
*
* Argument(s) : none
*
* Return(s)   : none
*
* Caller(s)   : gprs_init_task_fun()
*
* Note(s)     : none.
*********************************************************************************************************
*/
void gprs_power_on(void)
{

//	
	GPIO_SetBits(GPIOC, GPIO_Pin_2);				//GPRS_PWR
	timer_delay_1ms(10);
	GPIO_ResetBits(GPIOC, GPIO_Pin_2);
	timer_delay_1ms(1200);
	
}






/*
*********************************************************************************************************
*                                          gprs_check_cmd()
*
* Description : 检测字符串中是否存在子字符串.
*
* Argument(s) : p_str ： 要检测的子字符串
*
* Return(s)   : 返回子字符串的位置指针
*
* Caller(s)   : gprs_send_at()
*
* Note(s)     : 
*********************************************************************************************************
*/
u8 *gprs_check_cmd(u8 *src_str, u8 *p_str)
{
	char *str = NULL;
	
	str = strstr((const char*)src_str, (const char*)p_str);

	return (u8*)str;
}


/*
*********************************************************************************************************
*                                          gprs_send_at()
*
* Description : Create application tasks.
*
* Argument(s) : none
*
* Return(s)   : none
*
* Caller(s)   : gprs_init_task_fun()
*
* Note(s)     : none.
*********************************************************************************************************
*/
u8* gprs_send_at(u8 *cmd, u8 *ack, u16 waittime, u16 timeout)
{
	u8 res = 1;
	u8 buff[512];
	
	timer_is_timeout_1ms(timer_at, 0);	//开始定时器timer_at
	while (res)
	{	
		memset(&usart2_rx_buff, 0, sizeof(usart_buff_t));
		
		usart2_rx_status = 0;
		USART_OUT(USART2, cmd);		
		timer_delay_1ms(waittime);				//AT指令延时
	
		usart2_rx_status = 1;	//数据未处理 不在接收数据
		USART_OUT(USART1, usart2_rx_buff.pdata);

		if (gprs_check_cmd(usart2_rx_buff.pdata, ack))	
		{
			res = 0;				//监测到正确的应答数据
			usart2_rx_status = 0;	//数据处理完 开始接收数据
			
			memcpy(buff, usart2_rx_buff.pdata, 512);
			
			memset(&usart2_rx_buff, 0, sizeof(usart_buff_t));	
			
			return buff;
		}
			
		if (timer_is_timeout_1ms(timer_at, timeout) == 0)	//定时器timer_at结束
		{
			res = 0;
			usart2_rx_status = 0;	//数据处理完 开始接收数据
			return NULL;
		}
	}	
}



int gprs_send_data(u8 *data, u16 data_len, u8 *ack, u16 waittime)
{
	u8 res = 0;	

	usart_send(USART2, data, data_len);	
	
	timer_delay_1ms(waittime);				//AT指令延时

//	while (!res)
	{	
		memset(&usart2_rx_buff, 0, sizeof(usart_buff_t));
		
		usart_send(USART2, data, data_len);							
		timer_delay_1ms(waittime);				//AT指令延时
		USART_OUT(USART1, usart2_rx_buff.pdata);
		if(strstr((const char*)usart2_rx_buff.pdata, (const char*)ack))
		{
			res = 1;				//监测到正确的应答数据
			
			memset(&usart2_rx_buff, 0, sizeof(usart_buff_t));
		}
	}
		
	return res;
}






/*
*********************************************************************************************************
*                                          gprs_init_task_fun()
*
* Description : GPRS初始化任务函数.
*
* Argument(s) : p_arg :该任务要传的参数
*
* Return(s)   : none
*
* Caller(s)   : 
*
* Note(s)     : none.
*********************************************************************************************************
*/
void gprs_init_task(void)
{

	int mqtt_rc = 0;

	u8 *ret;
	static u8 gprs_init_flag = true;		//
		
	while(1)
	{
		switch(gprs_status)
		{
			case 0:
				gprs_power_on();
			
				gprs_status = 1;
				gprs_err_cnt = 0;
			
			break;
					
			case 1:
				ret = gprs_send_at("AT\r\n", "OK", 300,10000);
				if (ret != NULL)
				{
					gprs_status++;
					gprs_err_cnt = 0;
				}
				else
				{
					gprs_err_cnt++;
					if (gprs_err_cnt > 5)
					{
						gprs_status = 0;
					}
				}
			break;
			
			case 2:
				ret = gprs_send_at("ATE1\r\n", "OK", 500,10000);
				if (ret != NULL)
				{
					gprs_status++;
					gprs_err_cnt = 0;
				}
				else
				{
					gprs_err_cnt++;
					if (gprs_err_cnt > 5)
					{
						gprs_status = 0;
					}
				}
			break;
				
			case 3:
				ret = gprs_send_at("AT+CGSN\r\n", "OK", 500, 10000);
				if (ret != NULL)
				{
					gprs_status++;
					gprs_err_cnt = 0;
				}
				else
				{
					gprs_err_cnt++;
					if (gprs_err_cnt > 5)
					{
						gprs_status = 0;
					}
				}
			break;
			
			case 4:			
				ret = gprs_send_at("AT+CPIN?\r\n", "OK", 500, 10000);
				if (ret != NULL)
				{
					gprs_status++;
					gprs_err_cnt = 0;
				}
				else
				{
					gprs_err_cnt++;
					if (gprs_err_cnt > 5)
					{
						gprs_status = 0;
					}
				}
			break;
			
			case 5:
				ret = gprs_send_at("AT+CSQ\r\n", "OK", 500, 10000);
				if (ret != NULL)
				{
					gprs_status++;
					gprs_err_cnt = 0;
				}
				else
				{
					if (gprs_err_cnt > 5)
					{
						gprs_status = 0;
					}
				}
			break;
			
			case 6:
				ret = gprs_send_at("AT+CREG?\r\n", "OK", 500, 10000);
				if (ret != NULL)
				{
					gprs_status++;
					gprs_err_cnt = 0;
				}
				else
				{
					gprs_err_cnt++;
					if (gprs_err_cnt > 5)
					{
						gprs_status = 0;
					}
				}
			break;
			
			case 7:
//				gprs_status++;
				ret = gprs_send_at("AT+CIPCLOSE\r\n", "ERROR", 800, 10000);//
				if (ret != NULL)
				{
					gprs_status++;
					gprs_err_cnt = 0;
				}
				else
				{
					gprs_err_cnt++;
					if (gprs_err_cnt > 5)
					{
						gprs_status = 0;
					}
				}
			break;
			
			case 8:
//				ret = gprs_send_at("AT+CIPSTART=\"TCP\",\"103.46.128.47\",14947\r\n", "CONNECT OK", 1500, 20000);//
				ret = gprs_send_at("AT+CIPSTART=\"TCP\",\"118.31.69.148\",1883\r\n", "CONNECT OK", 1500, 20000);
				if (ret != NULL)
				{
					gprs_status++;
					gprs_err_cnt = 0;
				}
				else
				{
					gprs_err_cnt++;
					if (gprs_err_cnt > 5)
					{
						gprs_status = 0;
					}
				}
			break;
				
			case 9:
				mqtt_rc = mqtt_connect();
				if(1 == mqtt_rc)
				{
					gprs_status++;
					USART_OUT(USART1, "mqtt_connect ok\r\n");
				}	
			break;
				
				
			case 10:
				mqtt_rc = mqtt_subscribe_msg("test", 2, 1);
				if(1 == mqtt_rc)
				{
					gprs_status++;
					USART_OUT(USART1, "mqtt_subscribe_msg 1 ok\r\n");
				}
			break;
				
			case 11:
				mqtt_rc = mqtt_subscribe_msg("test1", 0, 2);
				if(1 == mqtt_rc)
				{
					gprs_status = 255;
					USART_OUT(USART1, "mqtt_subscribe_msg 2 ok\r\n");
				}
			break;
			
			case 255:	//gprs 初始化完成后进行数据传输
				
//				gprs_recv_task_create();
			
			break;
				
				
			default:
			break;		
		}	//switch end	
		
		if(gprs_status == 255)
		{
			break;
		}
	} // while end
		
}


void gprs_sleep(void)
{
	gprs_send_at("AT+CSCLK=1\r\n", "OK", 100, 1000);
}

void gprs_wakeup(uint8_t mode)
{
	if(mode == 0)
	{
		gprs_send_at("AT+CSCLK=0\r\n", "OK", 50, 1000);
	}
	else
	{
		//DTR
	}
	
}














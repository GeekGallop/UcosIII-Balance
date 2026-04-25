#ifndef __USART_H
#define __USART_H
#include "string.h"
#include "stdio.h"
#include "os.h"
#include "sys.h"
#include "stm32f4xx.h"


#define USARTx                   USART1
#define USARTx_CLK               RCC_APB2Periph_USART1
#define USARTx_GPIO_CLK          RCC_AHB1Periph_GPIOA
#define USARTx_TX_GPIO_PORT      GPIOA
#define USARTx_TX_GPIO_PIN       GPIO_Pin_9
#define USARTx_RX_GPIO_PORT      GPIOA
#define USARTx_RX_GPIO_PIN       GPIO_Pin_10
#define USARTx_IRQn              USART1_IRQn
#define USARTx_IRQHandler        USART1_IRQHandler


#define USART_RX_QUEUE_SIZE      1024  
#define USART_BAUDRATE           115200

extern OS_MUTEX  USART_Mutex;    
extern OS_Q      USART_Rx_Queue; 

  void USART_Send_Byte(uint8_t byte);
void usart_config(void);        
int fputc(int ch, FILE *f);

#endif /* __USART_H */


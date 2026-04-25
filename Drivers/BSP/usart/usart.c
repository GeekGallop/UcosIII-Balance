#include "usart.h"
#include "stdio.h"
#include "string.h"
#include "sys.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_usart.h"
#include "misc.h"
#include "os.h"

/* uC/OS-III synchronization objects */
OS_MUTEX  USART_Mutex;
OS_Q      USART_Rx_Queue;

char Serial_RxPacket[100];                
uint8_t Serial_RxFlag;    


/**
 * @brief  USART hardware configuration (Standard Library)
 * @note   Configure GPIO, clock, interrupt, baud rate, etc.
 */
static void USART_HW_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    /* 1. Enable clocks */
    RCC_AHB1PeriphClockCmd(USARTx_GPIO_CLK, ENABLE);  // GPIO clock
    RCC_APB2PeriphClockCmd(USARTx_CLK, ENABLE);       // USART clock

    /* 2. Configure TX pin (PA9): Alternate function push-pull output */
    GPIO_InitStruct.GPIO_Pin = USARTx_TX_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;         // Alternate function mode
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(USARTx_TX_GPIO_PORT, &GPIO_InitStruct);

    /* 3. Configure RX pin (PA10): Alternate function floating input */
    GPIO_InitStruct.GPIO_Pin = USARTx_RX_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(USARTx_RX_GPIO_PORT, &GPIO_InitStruct);

    /* 4. Pin alternate function mapping */
    GPIO_PinAFConfig(USARTx_TX_GPIO_PORT, GPIO_PinSource9, GPIO_AF_USART1);
    GPIO_PinAFConfig(USARTx_RX_GPIO_PORT, GPIO_PinSource10, GPIO_AF_USART1);

    /* 5. Configure USART parameters */
    USART_InitStruct.USART_BaudRate = USART_BAUDRATE;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USARTx, &USART_InitStruct);

    /* 6. Configure interrupt (receive not empty interrupt) */
    USART_ITConfig(USARTx, USART_IT_RXNE, ENABLE);  // Enable RXNE interrupt

    /* 7. Configure NVIC */
    NVIC_InitStruct.NVIC_IRQChannel = USARTx_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 5;  // Priority > OS_CPU_CFG_INT_PRIO_MIN
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    /* 8. Enable USART */
    USART_Cmd(USARTx, ENABLE);
}

/**
 * @brief  USART initialization (including uC/OS synchronization objects creation)
 */
void usart_config(void)
{
    OS_ERR err;

    /* Mutex: Solve multi-task USART send conflict */
    OSMutexCreate(&USART_Mutex, "USART1 Mutex", &err);
    
    /* Message queue: Store bytes received in interrupt (1 byte per message) */
    OSQCreate(&USART_Rx_Queue, "USART1 Rx Queue", USART_RX_QUEUE_SIZE, &err);

    USART_HW_Config();
}

/**
 * @brief  Send single byte via USART
 * @param  byte: Byte to send
 */
void USART_Send_Byte(uint8_t byte)
{
    /* Wait for transmit data register empty (TXE flag) */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    
    /* Write to data register */
    USART_SendData(USART1, byte);
    
    /* Wait for transmission complete */
    while(USART_GetFlagStatus(USART1,USART_FLAG_TC)==RESET);
}

struct __FILE 
{ 
    int handle; 
}; 
FILE __stdout;
void _sys_exit(int x) 
{ 
    x = x; 
}
/**
 * @brief  Redirect fputc to support printf output to USART
 * @note   Protected by mutex to avoid multi-task printf garbled output
 */
int fputc(int ch, FILE *f)
{
    OS_ERR err;
    
    /* Mutex protection for printf, avoid multi-task printf garbled output */
    //OSMutexPend(&USART_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    while((USART1->SR & 0X40) == 0);  // 等待发送完成
    USART1->DR = (u8) ch; 
    //OSMutexPost(&USART_Mutex, OS_OPT_POST_NONE, &err);

    return ch;
}

/**
 * @brief  USART1 interrupt handler
 * @note   Receive byte and post to queue for task processing
 */
void USART1_IRQHandler(void)                    
{
    OSIntEnter();    
    OS_ERR err;
    
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)    
    {
        uint8_t data_rx = USART_ReceiveData(USART1);

        /* Convert received byte to pointer and pass to queue */
        void *p_msg = (void *)(uintptr_t)data_rx;

        OSQPost(&USART_Rx_Queue, p_msg, 0, OS_OPT_POST_FIFO, &err);

        USART_ClearITPendingBit(USART1, USART_IT_RXNE);        
    }
    OSIntExit();                                             
}

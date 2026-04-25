/**
 ******************************************************************************
 * @file    tim.c
 * @brief   TIM8 timer driver implementation for periodic interrupt
 * @note    Provides 10ms periodic interrupt for encoder speed calculation
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Implementation Details:
 * -----------------------
 * 
 * 1. Clock Configuration:
 *    - TIM8 is on APB2 bus (168MHz)
 *    - Prescaler: 1680 -> Counter clock = 100kHz
 *    - Period: 1000 -> Interrupt every 10ms
 * 
 * 2. Interrupt Service:
 *    - TIM8_UP_TIM13_IRQHandler handles update interrupt
 *    - Calls Encoder_UpdateAll() for speed calculation
 *    - Clears interrupt pending bit
 * 
 * 3. Usage:
 *    - Call TIM8_Int_Init() during system initialization
 *    - Encoder_UpdateAll() is called automatically every 10ms
 *    - Add user code in ISR for other periodic tasks
 ******************************************************************************/

#include "./tim/tim.h"
#include "./encoder/encoder.h"
#include "os.h"
/**
 * @brief  Initialize TIM8 for 10ms periodic interrupt
 * @param  None
 * @retval None
 * @note   TIM8 clock: APB2 = 168MHz
 *         Prescaler = 1679 (divide by 1680), counter freq = 100kHz
 *         Period = 999 (1000 counts), interrupt period = 10ms
 *         Interrupt priority: Preemption=1, Sub=3
 */
void TIM8_Int_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    /* Enable TIM8 clock (APB2 bus) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
    
    /* Configure TIM8 time base */
    TIM_TimeBaseInitStructure.TIM_Period = 999;                     /* Auto-reload value: 1000-1 */
    TIM_TimeBaseInitStructure.TIM_Prescaler = 1679;                 /* Prescaler: 1680-1 */
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; /* Up counting mode */
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;     /* No clock division */
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;            /* Repetition counter (advanced timer only) */
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseInitStructure);
    
    /* Enable TIM8 update interrupt */
    TIM_ITConfig(TIM8, TIM_IT_Update, ENABLE);
    
    /* Enable TIM8 counter */
    //TIM_Cmd(TIM8, ENABLE);
    
    /* Configure NVIC for TIM8 update interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM8_UP_TIM13_IRQn;        /* TIM8 update interrupt channel */
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 9;    /* Preemption priority: 1 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;           /* Sub priority: 3 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief  TIM8 update interrupt service routine
 * @param  None
 * @retval None
 * @note   Called every 10ms (100Hz)
 *         Updates encoder data for speed calculation
 *         Add user code here for other periodic tasks
 */
void TIM8_UP_TIM13_IRQHandler(void)
{
    OSIntEnter();  
    if(TIM_GetITStatus(TIM8, TIM_IT_Update) == SET)
    {
        /* Update encoder data for speed calculation */
        
        
        /* Add user periodic tasks here */
        /* Example: PID control, data logging, etc. */
    }
    TIM_ClearITPendingBit(TIM8, TIM_IT_Update);
    OSIntExit();
}

/* End of file */

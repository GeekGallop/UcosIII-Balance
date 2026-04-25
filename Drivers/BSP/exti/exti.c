/**
 * @brief  External interrupt configuration
 * @note   Configure GPIO, EXTI, and NVIC for external interrupts
 */
#include "os.h"
#include "exti.h"
#include "stm32f4xx_gpio.h"
#include "misc.h"
#include "stm32f4xx_syscfg.h"
#include "./LED/led.h"

/**
 * Interrupt configuration steps:
 * 1. Configure clock
 * 2. Configure interrupt corresponding pins
 * 3. Connect pins to corresponding EXTI lines
 * 4. Configure priority
 * 5. Configure NVIC
 */
void exti_config(void)
{   
    GPIO_InitTypeDef GPIO_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    EXTI_InitTypeDef EXTI_InitStructure;

    /* Enable GPIO clocks */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);
    
    /* Enable SYSCFG clock for NVIC configuration */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG,ENABLE);

    /* Configure PA0 (WK_UP button) */
    GPIO_InitStruct.GPIO_Pin=GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_OType=GPIO_OType_OD;
    GPIO_InitStruct.GPIO_PuPd=GPIO_PuPd_UP;           // Pull-up
    GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
    GPIO_Init(GPIOA,&GPIO_InitStruct);

    /* Configure PE2, PE3, PE4 (KEY0, KEY1, KEY2) */
    GPIO_InitStruct.GPIO_Pin=GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4;
    GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_OType=GPIO_OType_OD;
    GPIO_InitStruct.GPIO_PuPd=GPIO_PuPd_DOWN;         // Pull-down
    GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
    GPIO_Init(GPIOE,&GPIO_InitStruct);

    /* Map GPIO to corresponding EXTI lines */
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA,EXTI_PinSource0);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE,EXTI_PinSource2);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE,EXTI_PinSource3);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE,EXTI_PinSource4);

    /* Configure EXTI Line0 (PA0) */
    EXTI_DeInit();
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Falling;  // Falling edge trigger
    EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_LineCmd=ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Configure EXTI Line2 (PE2) */
    EXTI_InitStructure.EXTI_Line = EXTI_Line2;
    EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Rising;   // Rising edge trigger
    EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_LineCmd=ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Configure EXTI Line3 (PE3) */
    EXTI_InitStructure.EXTI_Line = EXTI_Line3;
    EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Rising;   // Rising edge trigger
    EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_LineCmd=ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Configure EXTI Line4 (PE4) */
    EXTI_InitStructure.EXTI_Line = EXTI_Line4;
    EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Rising;   // Rising edge trigger
    EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_LineCmd=ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Configure NVIC for EXTI0 */
    NVIC_InitStruct.NVIC_IRQChannel=EXTI0_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd=ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority=3;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority=3;
    NVIC_Init(&NVIC_InitStruct);

    /* Configure NVIC for EXTI2 */
    NVIC_InitStruct.NVIC_IRQChannel=EXTI2_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd=ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority=3;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority=2;
    NVIC_Init(&NVIC_InitStruct);

    /* Configure NVIC for EXTI3 */
    NVIC_InitStruct.NVIC_IRQChannel=EXTI3_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd=ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority=3;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority=1;
    NVIC_Init(&NVIC_InitStruct);

    /* Configure NVIC for EXTI4 */
    NVIC_InitStruct.NVIC_IRQChannel=EXTI4_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd=ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority=3;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority=0;
    NVIC_Init(&NVIC_InitStruct);
}

/**
 * @brief  EXTI0 interrupt handler
 * @note   Toggle LED0 with software debounce
 */
void EXTI0_IRQHandler(void)
{
    static uint32_t last_time=0;
    OS_ERR err;
    OSIntEnter();
    
    if(EXTI_GetITStatus(EXTI_Line0)!=RESET)
    {   
        uint32_t current_time=OSTimeGet(&err);  // Software debounce

        if(current_time-last_time>50)
        {
            LED0=!LED0;
        }
        last_time=current_time;
        EXTI_ClearITPendingBit(EXTI_Line0);  // Clear interrupt flag
    }
    OSIntExit();
}

/**
 * @brief  EXTI2 interrupt handler
 */
void EXTI2_IRQHandler(void)
{
    OSIntEnter();
    if(EXTI_GetITStatus(EXTI_Line2)!=RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line2);  // Clear interrupt flag
    }
    OSIntExit();
}

/**
 * @brief  EXTI3 interrupt handler
 */
void EXTI3_IRQHandler(void)
{
    OSIntEnter();
    if(EXTI_GetITStatus(EXTI_Line3)!=RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line3);  // Clear interrupt flag
    }
    OSIntExit();
}

/**
 * @brief  EXTI4 interrupt handler
 */
void EXTI4_IRQHandler(void)
{
    OSIntEnter();
    if(EXTI_GetITStatus(EXTI_Line4)!=RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line4);  // Clear interrupt flag
    }
    OSIntExit();
}

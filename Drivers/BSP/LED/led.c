#include "./LED/led.h"

/**
 * @brief  Initialize LED GPIO
 * @note   Initialize PB1 and PC13 as output pins and enable their clocks
 */
void led_config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    /* Enable GPIOB and GPIOC clock */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC, ENABLE);

    /* Configure PB1 (LED0) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;          // Output mode
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;         // Push-pull
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;     // 100MHz
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;           // Pull-up
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* Configure PC13 (LED1) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;          // Output mode
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;         // Push-pull
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;     // 100MHz
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;           // Pull-up
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    /* Set LED pins to high (LED off) */
    GPIO_SetBits(GPIOB,GPIO_Pin_1);
    GPIO_SetBits(GPIOC,GPIO_Pin_13);
}

#include "key.h"
#include "os.h"

/**
 * @brief  Initialize KEY GPIO
 * @note   Configure PA0, PE2, PE3, PE4 as input pins
 */
void key_config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;

    /* Enable GPIOA and GPIOE clock */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOE, ENABLE);
 
    /* Configure PE2, PE3, PE4 (KEY0, KEY1, KEY2) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;           // Input mode
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;     // 100MHz
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;           // Pull-up
    GPIO_Init(GPIOE, &GPIO_InitStructure);
    
    /* Configure PA0 (WK_UP) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;         // Pull-down
    GPIO_Init(GPIOA, &GPIO_InitStructure);
} 

/**
 * @brief  Scan key press
 * @param  mode: 0=Single trigger, 1=Continuous trigger
 * @retval 0=No key pressed, 1=KEY0, 2=KEY1, 3=KEY2, 4=WK_UP
 */
u8 KEY_Scan(u8 mode)
{    
    OS_ERR err;
    static u8 key_up=1;  // Key release flag
    
    if(mode)
        key_up=1;  // Support continuous trigger
    
    if(key_up && (KEY0==0 || KEY1==0 || KEY2==0 || WK_UP==1))
    {
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);  // Debounce delay
        key_up=0;
        
        if(KEY0==0)
            return 1;
        else if(KEY1==0)
            return 2;
        else if(KEY2==0)
            return 3;
        else if(WK_UP==1)
            return 4;
    }
    else if(KEY0==1 && KEY1==1 && KEY2==1 && WK_UP==0)
        key_up=1;
    
    return 0;  // No key pressed
}

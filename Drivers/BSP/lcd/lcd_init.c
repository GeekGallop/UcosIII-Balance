/**
 ******************************************************************************
 * @file    lcd_init.c
 * @brief   LCD initialization and low-level driver functions
 * @note    Fixed formatting and added English comments
 ******************************************************************************
 */

#include "./lcd/lcd_init.h"
#include "./delay/delay.h"
#include "os.h"
#include "./lcd/lcd.h"

/* Forward declaration */
void LCD_Init(void);

/* LCD mutex for RTOS */
OS_MUTEX LCD_Mutex;

/**
 * @brief  LCD configuration (initialize hardware and create mutex)
 * @param  None
 * @retval None
 */
void lcd_config(void)
{
    OS_ERR err;
    LCD_Init();
    OSMutexCreate(&LCD_Mutex, "LCD Mutex", &err);
	LCD_Fill(0,0,LCD_HEIGHT,LCD_WIDTH,WHITE);
}

/**
 * @brief  Initialize LCD GPIO pins
 * @param  None
 * @retval None
 */
void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* Enable GPIOB and GPIOC clock */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC, ENABLE);
    
    /* Configure PC6 and PC7 (DC and BLK) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;       /* Output mode */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;      /* Push-pull */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;  /* 100MHz */
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;        /* Pull-up */
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    GPIO_SetBits(GPIOC, GPIO_Pin_6 | GPIO_Pin_7);
    
    /* Configure PB12, PB13, PB14, PB15 (CS, SCK, RES, MOSI) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;       /* Output mode */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;      /* Push-pull */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;  /* 100MHz */
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;        /* Pull-up */
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);
}

/**
 * @brief  Write data to LCD via SPI bus (software simulation)
 * @param  dat: Data to write
 * @retval None
 */
void LCD_Writ_Bus(u8 dat)
{
    u8 i;
    
    LCD_CS_Clr();
    for (i = 0; i < 8; i++)
    {
        LCD_SCLK_Clr();
        if (dat & 0x80)
        {
            LCD_MOSI_Set();
        }
        else
        {
            LCD_MOSI_Clr();
        }
        LCD_SCLK_Set();
        dat <<= 1;
    }
    LCD_CS_Set();
}

/**
 * @brief  Write 8-bit data to LCD
 * @param  dat: 8-bit data to write
 * @retval None
 */
void LCD_WR_DATA8(u8 dat)
{
    LCD_Writ_Bus(dat);
}

/**
 * @brief  Write 16-bit data to LCD
 * @param  dat: 16-bit data to write
 * @retval None
 */
void LCD_WR_DATA(u16 dat)
{
    LCD_Writ_Bus(dat >> 8);
    LCD_Writ_Bus(dat);
}

/**
 * @brief  Write register/command to LCD
 * @param  dat: Register address or command
 * @retval None
 */
void LCD_WR_REG(u8 dat)
{
    LCD_DC_Clr();           /* Command mode */
    LCD_Writ_Bus(dat);
    LCD_DC_Set();           /* Data mode */
}

/**
 * @brief  Set LCD display area
 * @param  x1: Start X coordinate
 * @param  y1: Start Y coordinate
 * @param  x2: End X coordinate
 * @param  y2: End Y coordinate
 * @retval None
 */
void LCD_Address_Set(u16 x1, u16 y1, u16 x2, u16 y2)
{
    if (USE_HORIZONTAL == 0)
    {
        LCD_WR_REG(0x2a);   /* Column address set */
        LCD_WR_DATA(x1 + 2);
        LCD_WR_DATA(x2 + 2);
        LCD_WR_REG(0x2b);   /* Row address set */
        LCD_WR_DATA(y1 + 1);
        LCD_WR_DATA(y2 + 1);
        LCD_WR_REG(0x2c);   /* Memory write */
    }
    else if (USE_HORIZONTAL == 1)
    {
        LCD_WR_REG(0x2a);   /* Column address set */
        LCD_WR_DATA(x1 + 2);
        LCD_WR_DATA(x2 + 2);
        LCD_WR_REG(0x2b);   /* Row address set */
        LCD_WR_DATA(y1 + 1);
        LCD_WR_DATA(y2 + 1);
        LCD_WR_REG(0x2c);   /* Memory write */
    }
    else if (USE_HORIZONTAL == 2)
    {
        LCD_WR_REG(0x2a);   /* Column address set */
        LCD_WR_DATA(x1 + 1);
        LCD_WR_DATA(x2 + 1);
        LCD_WR_REG(0x2b);   /* Row address set */
        LCD_WR_DATA(y1 + 2);
        LCD_WR_DATA(y2 + 2);
        LCD_WR_REG(0x2c);   /* Memory write */
    }
    else
    {
        LCD_WR_REG(0x2a);   /* Column address set */
        LCD_WR_DATA(x1 + 1);
        LCD_WR_DATA(x2 + 1);
        LCD_WR_REG(0x2b);   /* Row address set */
        LCD_WR_DATA(y1 + 2);
        LCD_WR_DATA(y2 + 2);
        LCD_WR_REG(0x2c);   /* Memory write */
    }
}

/**
 * @brief  Initialize LCD controller (ST7735S)
 * @param  None
 * @retval None
 */
void LCD_Init(void)
{
    LCD_GPIO_Init();        /* Initialize GPIO */
    
    LCD_RES_Clr();          /* Reset LCD */
    delay_ms(100);
    LCD_RES_Set();
    delay_ms(100);
    
    LCD_BLK_Set();          /* Turn on backlight */
    delay_ms(100);
    
    /* Sleep out */
    LCD_WR_REG(0x11);
    delay_ms(120);
    
    /* Memory data access control */
    LCD_WR_REG(0x36);
    if (USE_HORIZONTAL == 0)
        LCD_WR_DATA8(0x00);
    else if (USE_HORIZONTAL == 1)
        LCD_WR_DATA8(0xC0);
    else if (USE_HORIZONTAL == 2)
        LCD_WR_DATA8(0x70);
    else
        LCD_WR_DATA8(0xA0);
    
    /* RGB 5-6-5 format */
    LCD_WR_REG(0x3A);
    LCD_WR_DATA8(0x05);
    
    /* Porch setting */
    LCD_WR_REG(0xB2);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x33);
    LCD_WR_DATA8(0x33);
    
    /* Gate control */
    LCD_WR_REG(0xB7);
    LCD_WR_DATA8(0x35);
    
    /* VCOM setting */
    LCD_WR_REG(0xBB);
    LCD_WR_DATA8(0x19);
    
    /* LCM control */
    LCD_WR_REG(0xC0);
    LCD_WR_DATA8(0x2C);
    
    /* VDV and VRH command enable */
    LCD_WR_REG(0xC2);
    LCD_WR_DATA8(0x01);
    
    /* VRH set */
    LCD_WR_REG(0xC3);
    LCD_WR_DATA8(0x12);
    
    /* VDV set */
    LCD_WR_REG(0xC4);
    LCD_WR_DATA8(0x20);
    
    /* Frame rate control in normal mode */
    LCD_WR_REG(0xC6);
    LCD_WR_DATA8(0x0F);
    
    /* Power control 1 */
    LCD_WR_REG(0xD0);
    LCD_WR_DATA8(0xA4);
    LCD_WR_DATA8(0xA1);
    
    /* Positive voltage gamma control */
    LCD_WR_REG(0xE0);
    LCD_WR_DATA8(0xD0);
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x11);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x2B);
    LCD_WR_DATA8(0x3F);
    LCD_WR_DATA8(0x54);
    LCD_WR_DATA8(0x4C);
    LCD_WR_DATA8(0x18);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x0B);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x23);
    
    /* Negative voltage gamma control */
    LCD_WR_REG(0xE1);
    LCD_WR_DATA8(0xD0);
    LCD_WR_DATA8(0x04);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x11);
    LCD_WR_DATA8(0x13);
    LCD_WR_DATA8(0x2C);
    LCD_WR_DATA8(0x3F);
    LCD_WR_DATA8(0x44);
    LCD_WR_DATA8(0x51);
    LCD_WR_DATA8(0x2F);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x1F);
    LCD_WR_DATA8(0x20);
    LCD_WR_DATA8(0x23);
    
    /* Inversion on */
    LCD_WR_REG(0x21);
    
    /* Display on */
    LCD_WR_REG(0x29);
}

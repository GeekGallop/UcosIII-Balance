/**
 ******************************************************************************
 * @file    lcd_init.h
 * @brief   LCD initialization header file
 * @note    Fixed formatting and added English comments
 ******************************************************************************
 */

#ifndef __LCD_INIT_H
#define __LCD_INIT_H

#include "sys.h"
#include "os.h"

/* LCD mutex for RTOS */
extern OS_MUTEX LCD_Mutex;

/* Display orientation configuration */
/* 0 or 1: Portrait mode, 2 or 3: Landscape mode */
#define USE_HORIZONTAL      1

/* LCD resolution based on orientation */
#if USE_HORIZONTAL == 0 || USE_HORIZONTAL == 1
#define LCD_W               128
#define LCD_H               160
#else
#define LCD_W               160
#define LCD_H               128
#endif

/* ==================== LCD Pin Definitions ==================== */

/* SCL = SCLK = SCK (Clock) */
#define LCD_SCLK_Clr()      GPIO_ResetBits(GPIOB, GPIO_Pin_13)
#define LCD_SCLK_Set()      GPIO_SetBits(GPIOB, GPIO_Pin_13)

/* SDA = MOSI = SDI (Data) */
#define LCD_MOSI_Clr()      GPIO_ResetBits(GPIOB, GPIO_Pin_15)
#define LCD_MOSI_Set()      GPIO_SetBits(GPIOB, GPIO_Pin_15)

/* RES = RST (Reset) */
#define LCD_RES_Clr()       GPIO_ResetBits(GPIOB, GPIO_Pin_14)
#define LCD_RES_Set()       GPIO_SetBits(GPIOB, GPIO_Pin_14)

/* DC = RS (Data/Command select) */
#define LCD_DC_Clr()        GPIO_ResetBits(GPIOC, GPIO_Pin_6)
#define LCD_DC_Set()        GPIO_SetBits(GPIOC, GPIO_Pin_6)

/* CS (Chip select) */
#define LCD_CS_Clr()        GPIO_ResetBits(GPIOB, GPIO_Pin_12)
#define LCD_CS_Set()        GPIO_SetBits(GPIOB, GPIO_Pin_12)

/* BLK (Backlight) */
#define LCD_BLK_Clr()       GPIO_ResetBits(GPIOC, GPIO_Pin_7)
#define LCD_BLK_Set()       GPIO_SetBits(GPIOC, GPIO_Pin_7)

/* ==================== Function Declarations ==================== */

void LCD_GPIO_Init(void);                                   /* Initialize GPIO */
void LCD_Writ_Bus(u8 dat);                                  /* Simulate SPI timing */
void LCD_WR_DATA8(u8 dat);                                  /* Write one byte */
void LCD_WR_DATA(u16 dat);                                  /* Write two bytes */
void LCD_WR_REG(u8 dat);                                    /* Write register/command */
void LCD_Address_Set(u16 x1, u16 y1, u16 x2, u16 y2);      /* Set display area */
void lcd_config(void);                                      /* LCD configuration (init + mutex) */
void LCD_Init(void);                                        /* LCD initialization */

#endif /* __LCD_INIT_H */

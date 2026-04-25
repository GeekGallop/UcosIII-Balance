/**
 ******************************************************************************
 * @file    task_lcd.h
 * @brief   LCD display task header file
 * @note    Unified display task for multiple sensor data
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Features:
 * ---------
 * - Displays MPU6050 sensor data (Roll, Pitch, Temperature)
 * - Displays Encoder data (Position, Speed, RPM for Left and Right motors)
 * - Uses message queue for data reception from multiple sources
 * - Optimized screen refresh with partial update support
 ******************************************************************************/

#ifndef __TASK_LCD_H
#define __TASK_LCD_H

#include "os.h"
#include "./mpu6050/mpu6050.h"
#include "./encoder/encoder.h"

/* ==================== Display Update Rate ==================== */
#define LCD_REFRESH_RATE_HZ     20      /* 20Hz = 50ms refresh rate */
#define LCD_REFRESH_PERIOD_MS   (1000 / LCD_REFRESH_RATE_HZ)

/* ==================== Screen Layout Definitions ==================== */
/* MPU6050 Data Area */
#define LCD_MPU_START_Y         0
#define LCD_MPU_ROW_HEIGHT      20

/* Encoder Data Area */
#define LCD_ENC_START_Y         80
#define LCD_ENC_ROW_HEIGHT      16

/* ==================== Data Types ==================== */

/**
 * @brief  Encoder display data structure
 * @note   Packaged data for LCD display
 */
typedef struct {
    int32_t position_left;      /* Left encoder position */
    int32_t position_right;     /* Right encoder position */
    int16_t speed_left;         /* Left encoder speed (counts/period) */
    int16_t speed_right;        /* Right encoder speed (counts/period) */
    float rpm_left;             /* Left motor RPM */
    float rpm_right;            /* Right motor RPM */
    int8_t direction_left;      /* Left motor direction */
    int8_t direction_right;     /* Right motor direction */
} Encoder_Display_Data_t;

/**
 * @brief  MPU6050 display data packet
 * @note   Sent through MPU message queue to LCD task
 */
typedef struct {
    MPU6050_Attitude_t attitude;    /* Calculated attitude (Roll, Pitch) */
    float temperature;              /* Temperature in Celsius */
} MPU_Display_Data_t;

/* ==================== External Variables ==================== */
extern OS_Q g_mpu_display_queue;        /* Message queue for MPU6050 display data */
extern OS_Q g_encoder_display_queue;    /* Message queue for Encoder display data */

/* ==================== Function Declarations ==================== */

/**
 * @brief  LCD display task
 * @param  p_arg: Task argument (unused)
 * @retval None
 * @note   Main display task, receives data from multiple sources via queue
 */
void Task_LCD_Display(void *p_arg);

/**
 * @brief  Initialize LCD display system
 * @param  None
 * @retval 0 on success, -1 on failure
 * @note   Creates message queue and initializes LCD hardware
 */
int LCD_Display_Init(void);

/**
 * @brief  Send MPU6050 data to display queue
 * @param  attitude: Pointer to attitude data
 * @param  temp: Temperature value
 * @retval 0 on success, -1 on failure
 * @note   Non-blocking, drops data if queue is full
 */
int LCD_Send_MPU_Data(MPU6050_Attitude_t *attitude, float temp);

/**
 * @brief  Send Encoder data to display queue
 * @param  encoder_data: Pointer to encoder display data
 * @retval 0 on success, -1 on failure
 * @note   Non-blocking, drops data if queue is full
 */
int LCD_Send_Encoder_Data(Encoder_Display_Data_t *encoder_data);

#endif /* __TASK_LCD_H */

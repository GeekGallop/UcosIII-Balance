/**
 ******************************************************************************
 * @file    task_mpu.h
 * @brief   MPU6050 task header file
 * @note    Handles MPU6050 sensor reading and data distribution
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Features:
 * ---------
 * - 100Hz sampling rate for MPU6050 data
 * - Sends data to LCD display via message queue
 * - Replaces semaphore-based notification with queue-based communication
 ******************************************************************************/

#ifndef __TASK_MPU_H
#define __TASK_MPU_H

#include "os.h"
#include "./mpu6050/mpu6050.h"

/* ==================== External Variables ==================== */
extern OS_MUTEX MPU6050_Mutex;          /* Data protection mutex */
extern MPU6050_Data_t g_mpu6050_data;   /* Global MPU6050 data */

/* ==================== Function Declarations ==================== */

/**
 * @brief  MPU6050 reading task
 * @param  p_arg: Task argument (unused)
 * @retval None
 * @note   Reads sensor data at 100Hz
 *         Sends data to LCD display task via queue
 */
void Task_MPU6050_Read(void *p_arg);


#endif /* __TASK_MPU_H */

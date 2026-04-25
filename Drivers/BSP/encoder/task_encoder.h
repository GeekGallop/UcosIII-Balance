/**
 ******************************************************************************
 * @file    task_encoder.h
 * @brief   Encoder task header file
 * @note    Handles encoder speed measurement and data distribution
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Features:
 * ---------
 * - 100Hz sampling rate for encoder data
 * - Sends data to LCD display via message queue
 * - Thread-safe with mutex protection
 ******************************************************************************/

#ifndef __TASK_ENCODER_H
#define __TASK_ENCODER_H

#include "os.h"
#include "os_cfg.h"
/**
 * @brief  Encoder speed measurement task
 * @param  p_arg: Task argument (unused)
 * @retval None
 * @note   Runs at 100Hz (10ms period)
 *         Sends data to LCD display task via queue
 */
void Task_Encoder_Speed(void *p_arg);

#endif /* __TASK_ENCODER_H */

/**
 ******************************************************************************
 * @file    tim.h
 * @brief   TIM8 timer driver for periodic interrupt (10ms)
 * @note    Used for encoder speed calculation and periodic tasks
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Hardware Configuration:
 * -----------------------
 * Timer: TIM8 (Advanced timer)
 * Clock Source: APB2 = 168MHz
 * 
 * Timing Configuration:
 * ---------------------
 * Prescaler: 1680-1 = 1679
 * Counter Frequency: 168MHz / 1680 = 100kHz
 * Period: 1000-1 = 999
 * Interrupt Period: 1000 / 100kHz = 10ms (100Hz)
 * 
 * Interrupt Priority:
 * -------------------
 * Preemption Priority: 1
 * Sub Priority: 3
 ******************************************************************************/

#ifndef __TIM_H
#define __TIM_H

#include "stm32f4xx.h"

/* ==================== Function Declarations ==================== */

void TIM8_Int_Init(void);

#endif /* __TIM_H */

/**
 ******************************************************************************
 * @file    motor_smooth.h
 * @brief   Motor speed smoothing and ramp control
 * @note    Prevents HardFault by limiting speed change rate
 * @author  User
 * @date    2026-02-20
 ******************************************************************************
 */

#ifndef __MOTOR_SMOOTH_H
#define __MOTOR_SMOOTH_H

#include <stdint.h>

/* ==================== Configuration ==================== */
#define MOTOR_RAMP_DEFAULT_STEP     50      /* Default speed change per cycle */
#define MOTOR_RAMP_MIN_STEP         10      /* Minimum step for fine control */
#define MOTOR_RAMP_MAX_STEP         200     /* Maximum step for fast response */
#define MOTOR_RAMP_CYCLE_MS         10      /* Ramp update period (ms) */

#define MOTOR_BRAKE_THRESHOLD       100     /* Speed threshold for brake mode */
#define MOTOR_BRAKE_DURATION_MS     50      /* Brake duration before reverse */

/* ==================== Data Types ==================== */
typedef struct {
    int16_t current_speed;          /* Current smoothed speed */
    int16_t target_speed;           /* Target speed from controller */
    int16_t last_target;            /* Previous target for direction detection */
    uint16_t ramp_step;             /* Max speed change per cycle */
    uint8_t brake_state;            /* Brake state machine */
    uint32_t brake_timer;           /* Brake timing */
    uint8_t direction_changing;     /* Flag for direction transition */
} Motor_Ramp_Controller_t;

/* ==================== Function Declarations ==================== */

/**
 * @brief  Initialize motor ramp controller
 * @param  ramp: Pointer to ramp controller structure
 * @param  step: Maximum speed change per cycle (10-200)
 * @retval None
 */
void Motor_Ramp_Init(Motor_Ramp_Controller_t *ramp, uint16_t step);

/**
 * @brief  Set target speed with smoothing
 * @param  ramp: Pointer to ramp controller
 * @param  target: Target speed (-1000 to 1000)
 * @retval Smoothed speed value
 * @note   Handles direction change with brake period
 */
int16_t Motor_Ramp_SetSpeed(Motor_Ramp_Controller_t *ramp, int16_t target);

/**
 * @brief  Emergency stop (no ramp)
 * @param  ramp: Pointer to ramp controller
 * @retval 0 (stopped)
 * @note   Immediately sets speed to 0
 */
int16_t Motor_Ramp_EmergencyStop(Motor_Ramp_Controller_t *ramp);

/**
 * @brief  Check if ramp has reached target
 * @param  ramp: Pointer to ramp controller
 * @retval 1 if reached, 0 if still ramping
 */
uint8_t Motor_Ramp_IsSettled(Motor_Ramp_Controller_t *ramp);

#endif /* __MOTOR_SMOOTH_H */

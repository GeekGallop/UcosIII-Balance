/**
 ******************************************************************************
 * @file    motor_smooth.c
 * @brief   Motor speed smoothing and ramp control implementation
 * @note    Prevents HardFault by limiting speed change rate and handling direction changes
 * @author  User
 * @date    2026-02-20
 ******************************************************************************
 * 
 * Key Features:
 * -------------
 * 1. Speed Ramp Control: Limits acceleration/deceleration rate
 * 2. Direction Change Protection: Brake before reverse to prevent current spike
 * 3. Smooth Transition: Gradual speed changes reduce mechanical stress
 * 
 * Direction Change Sequence:
 * --------------------------
 * Forward(1000) -> Brake(0) -> Wait(50ms) -> Reverse(-100)
 * This prevents direct 1000 -> -1000 transition which causes:
 * - High reverse current spike
 * - Power supply voltage dip
 * - Potential HardFault
 ******************************************************************************/

#include "./motor/motor_smooth.h"
#include "./motor/motor.h"
#include <stdlib.h>

/**
 * @brief  Initialize motor ramp controller
 * @param  ramp: Pointer to ramp controller structure
 * @param  step: Maximum speed change per cycle (10-200)
 * @retval None
 */
void Motor_Ramp_Init(Motor_Ramp_Controller_t *ramp, uint16_t step)
{
    if (ramp == NULL) return;
    
    ramp->current_speed = 0;
    ramp->target_speed = 0;
    ramp->last_target = 0;
    ramp->ramp_step = (step < MOTOR_RAMP_MIN_STEP) ? MOTOR_RAMP_MIN_STEP :
                      (step > MOTOR_RAMP_MAX_STEP) ? MOTOR_RAMP_MAX_STEP : step;
    ramp->brake_state = 0;
    ramp->brake_timer = 0;
    ramp->direction_changing = 0;
}

/**
 * @brief  Set target speed with smoothing and direction change protection
 * @param  ramp: Pointer to ramp controller
 * @param  target: Target speed (-1000 to 1000)
 * @retval Smoothed speed value to apply to motor
 * 
 * Direction Change Logic:
 * - If target and current have opposite signs: direction change detected
 * - First: ramp to 0 (brake)
 * - Wait for brake duration
 * - Then: ramp from 0 to new target
 */
int16_t Motor_Ramp_SetSpeed(Motor_Ramp_Controller_t *ramp, int16_t target)
{
    int16_t delta;
    int16_t new_speed;
    
    if (ramp == NULL) return 0;
    
    /* Constrain target to valid range */
    if (target > 1000) target = 1000;
    if (target < -1000) target = -1000;
    
    ramp->target_speed = target;
    
    /* Check for direction change */
    if (!ramp->direction_changing) {
        /* Detect direction change: signs are different AND both non-zero */
        if ((ramp->current_speed > 0 && target < 0) || 
            (ramp->current_speed < 0 && target > 0)) {
            /* Direction change detected - enter brake mode first */
            ramp->direction_changing = 1;
            ramp->brake_state = 1;
            ramp->brake_timer = 0;
            /* Target becomes 0 for now (brake) */
            target = 0;
        }
    }
    
    /* Handle brake state machine */
    if (ramp->direction_changing) {
        switch (ramp->brake_state) {
            case 1: /* Braking to 0 */
                if (abs(ramp->current_speed) <= ramp->ramp_step) {
                    /* Reached near zero, full stop */
                    ramp->current_speed = 0;
                    ramp->brake_state = 2;
                    ramp->brake_timer = 0;
                } else {
                    /* Ramp toward 0 */
                    if (ramp->current_speed > 0) {
                        ramp->current_speed -= ramp->ramp_step;
                    } else {
                        ramp->current_speed += ramp->ramp_step;
                    }
                }
                break;
                
            case 2: /* Waiting in brake */
                ramp->brake_timer += MOTOR_RAMP_CYCLE_MS;
                if (ramp->brake_timer >= MOTOR_BRAKE_DURATION_MS) {
                    /* Brake period complete, start new direction */
                    ramp->brake_state = 3;
                }
                break;
                
            case 3: /* Ramping to new target */
                /* Continue to normal ramp logic below */
                ramp->direction_changing = 0;
                ramp->brake_state = 0;
                break;
                
            default:
                ramp->direction_changing = 0;
                ramp->brake_state = 0;
                break;
        }
        
        /* If still in brake states 1 or 2, return current speed (0) */
        if (ramp->direction_changing && ramp->brake_state != 3) {
            return ramp->current_speed;
        }
    }
    
    /* Normal ramp logic */
    delta = target - ramp->current_speed;
    
    if (abs(delta) <= ramp->ramp_step) {
        /* Close enough, go directly to target */
        new_speed = target;
    } else {
        /* Ramp toward target */
        if (delta > 0) {
            new_speed = ramp->current_speed + ramp->ramp_step;
        } else {
            new_speed = ramp->current_speed - ramp->ramp_step;
        }
    }
    
    ramp->current_speed = new_speed;
    ramp->last_target = ramp->target_speed;
    
    return new_speed;
}

/**
 * @brief  Emergency stop (no ramp)
 * @param  ramp: Pointer to ramp controller
 * @retval 0 (stopped)
 * @note   Immediately sets speed to 0, clears all states
 */
int16_t Motor_Ramp_EmergencyStop(Motor_Ramp_Controller_t *ramp)
{
    if (ramp == NULL) return 0;
    
    ramp->current_speed = 0;
    ramp->target_speed = 0;
    ramp->brake_state = 0;
    ramp->brake_timer = 0;
    ramp->direction_changing = 0;
    
    return 0;
}

/**
 * @brief  Check if ramp has reached target
 * @param  ramp: Pointer to ramp controller
 * @retval 1 if reached, 0 if still ramping
 */
uint8_t Motor_Ramp_IsSettled(Motor_Ramp_Controller_t *ramp)
{
    if (ramp == NULL) return 1;
    
    return (ramp->current_speed == ramp->target_speed && 
            !ramp->direction_changing) ? 1 : 0;
}

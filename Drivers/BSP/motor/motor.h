/**
 ******************************************************************************
 * @file    motor.h
 * @brief   DC motor driver with PWM control and direction management
 * @note    Optimized for STM32F4 with TIM4 PWM output
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Hardware Configuration:
 * -----------------------
 * Motor A (Left):
 *   - PWM: TIM4_CH2 (PD13)
 *   - Direction: PD11 (AIN1), PD12 (AIN2)
 * 
 * Motor B (Right):
 *   - PWM: TIM4_CH3 (PD14)
 *   - Direction: PC10 (BIN1), PC11 (BIN2)
 * 
 * PWM Frequency: 10kHz (adjustable via prescaler and period)
 * PWM Resolution: 0-1000 (0-100% duty cycle)
 ******************************************************************************/

#ifndef __MOTOR_H
#define __MOTOR_H

#include "sys.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"

/* ==================== Motor Selection ==================== */

#define MOTOR_A         0   /* Left motor */
#define MOTOR_B         1   /* Right motor */
#define MOTOR_BOTH      2   /* Both motors */

/* ==================== Direction Definitions ==================== */

#define MOTOR_STOP      0   /* Motor stop (brake) */
#define MOTOR_FORWARD   1   /* Motor forward rotation */
#define MOTOR_BACKWARD  2   /* Motor backward rotation */
#define MOTOR_COAST     3   /* Motor coast (freewheel) */

/* ==================== PWM Configuration ==================== */

#define MOTOR_PWM_TIM           TIM4
#define MOTOR_PWM_RCC           RCC_APB1Periph_TIM4

#define MOTOR_PWM_FREQ          10000   /* 10kHz PWM frequency */
#define MOTOR_PWM_PERIOD        1000    /* PWM period (0-1000 = 0-100%) */

/* Motor A PWM - TIM4 CH2 */
#define MOTOR_A_PWM_GPIO        GPIOD
#define MOTOR_A_PWM_PIN         GPIO_Pin_13
#define MOTOR_A_PWM_AF          GPIO_AF_TIM4
#define MOTOR_A_PWM_SOURCE      GPIO_PinSource13
#define MOTOR_A_PWM_CHANNEL     TIM_OC2
#define MOTOR_A_PWM_PRELOAD     TIM_OCPreload_Enable

/* Motor B PWM - TIM4 CH3 */
#define MOTOR_B_PWM_GPIO        GPIOD
#define MOTOR_B_PWM_PIN         GPIO_Pin_14
#define MOTOR_B_PWM_AF          GPIO_AF_TIM4
#define MOTOR_B_PWM_SOURCE      GPIO_PinSource14
#define MOTOR_B_PWM_CHANNEL     TIM_OC3
#define MOTOR_B_PWM_PRELOAD     TIM_OCPreload_Enable

/* ==================== Direction Pin Configuration ==================== */

/* Motor A Direction Pins */
#define MOTOR_A_DIR1_GPIO       GPIOC
#define MOTOR_A_DIR1_PIN        GPIO_Pin_11
#define MOTOR_A_DIR2_GPIO       GPIOC
#define MOTOR_A_DIR2_PIN        GPIO_Pin_10

/* Motor B Direction Pins */
#define MOTOR_B_DIR1_GPIO       GPIOD
#define MOTOR_B_DIR1_PIN        GPIO_Pin_8
#define MOTOR_B_DIR2_GPIO       GPIOD
#define MOTOR_B_DIR2_PIN        GPIO_Pin_9

/* ==================== Direction Control Macros ==================== */

/* Motor A Direction Control */
#define MOTOR_A_FORWARD()       do { \
    GPIO_SetBits(MOTOR_A_DIR1_GPIO, MOTOR_A_DIR1_PIN); \
    GPIO_ResetBits(MOTOR_A_DIR2_GPIO, MOTOR_A_DIR2_PIN); \
} while(0)

#define MOTOR_A_BACKWARD()      do { \
    GPIO_ResetBits(MOTOR_A_DIR1_GPIO, MOTOR_A_DIR1_PIN); \
    GPIO_SetBits(MOTOR_A_DIR2_GPIO, MOTOR_A_DIR2_PIN); \
} while(0)

#define MOTOR_A_STOP()          do { \
    GPIO_SetBits(MOTOR_A_DIR1_GPIO, MOTOR_A_DIR1_PIN); \
    GPIO_SetBits(MOTOR_A_DIR2_GPIO, MOTOR_A_DIR2_PIN); \
} while(0)

#define MOTOR_A_COAST()         do { \
    GPIO_ResetBits(MOTOR_A_DIR1_GPIO, MOTOR_A_DIR1_PIN); \
    GPIO_ResetBits(MOTOR_A_DIR2_GPIO, MOTOR_A_DIR2_PIN); \
} while(0)

/* Motor B Direction Control */
#define MOTOR_B_FORWARD()       do { \
    GPIO_SetBits(MOTOR_B_DIR1_GPIO, MOTOR_B_DIR1_PIN); \
    GPIO_ResetBits(MOTOR_B_DIR2_GPIO, MOTOR_B_DIR2_PIN); \
} while(0)

#define MOTOR_B_BACKWARD()      do { \
    GPIO_ResetBits(MOTOR_B_DIR1_GPIO, MOTOR_B_DIR1_PIN); \
    GPIO_SetBits(MOTOR_B_DIR2_GPIO, MOTOR_B_DIR2_PIN); \
} while(0)

#define MOTOR_B_STOP()          do { \
    GPIO_SetBits(MOTOR_B_DIR1_GPIO, MOTOR_B_DIR1_PIN); \
    GPIO_SetBits(MOTOR_B_DIR2_GPIO, MOTOR_B_DIR2_PIN); \
} while(0)

#define MOTOR_B_COAST()         do { \
    GPIO_ResetBits(MOTOR_B_DIR1_GPIO, MOTOR_B_DIR1_PIN); \
    GPIO_ResetBits(MOTOR_B_DIR2_GPIO, MOTOR_B_DIR2_PIN); \
} while(0)

/* ==================== PWM Duty Cycle Macros ==================== */

#define MOTOR_A_SET_SPEED(speed)    TIM_SetCompare2(MOTOR_PWM_TIM, (speed))
#define MOTOR_B_SET_SPEED(speed)    TIM_SetCompare3(MOTOR_PWM_TIM, (speed))

/* ==================== Function Declarations ==================== */

void motor_config(void);
void Motor_PWM_Init(uint32_t prescaler, uint32_t period);
void Motor_Direction_Init(void);

void Motor_SetSpeed(uint8_t motor, int16_t speed);
void Motor_SetDirection(uint8_t motor, uint8_t direction);
void Motor_Stop(uint8_t motor);
void Motor_Coast(uint8_t motor);

void Motor_Brake(uint8_t motor);
void Motor_SetSpeedPercent(uint8_t motor, float percent);

#endif /* __MOTOR_H */

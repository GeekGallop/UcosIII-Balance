/**
 ******************************************************************************
 * @file    motor.c
 * @brief   DC motor driver implementation with PWM control
 * @note    Supports dual motor control with independent speed and direction
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Implementation Details:
 * -----------------------
 * 1. PWM Generation: TIM4 Channels 2 and 3
 *    - Frequency: 10kHz (configurable)
 *    - Resolution: 0-1000 (0.1% precision)
 * 
 * 2. Direction Control: H-bridge driver via GPIO
 *    - AIN1/BIN1: Forward direction control
 *    - AIN2/BIN2: Backward direction control
 *    - Both HIGH: Brake (short circuit)
 *    - Both LOW: Coast (high impedance)
 * 
 * 3. Speed Control: PWM duty cycle adjustment
 *    - Positive value: Forward rotation
 *    - Negative value: Backward rotation
 *    - Zero: Motor stop (brake)
 ******************************************************************************/

#include "./motor/motor.h"
void Motor_SetSpeed(uint8_t motor, int16_t speed);
/**
 * @brief  Initialize motor driver (PWM + direction pins)
 * @param  None
 * @retval None
 * @note   Must be called before any motor operation
 */
void motor_config(void)
{

    Motor_PWM_Init(419,200);
    
    /* Initialize direction control pins */
    Motor_Direction_Init();
    
    /* Set initial state: motors stopped */
    //Motor_Stop(MOTOR_BOTH);
}

/**
 * @brief  Initialize PWM output for motor speed control
 * @param  prescaler: TIM prescaler value (0-65535)
 * @param  period: TIM period value (0-65535)
 * @retval None
 * @note   TIM4 CH2 (PD13) for Motor A, TIM4 CH3 (PD14) for Motor B
 */
void Motor_PWM_Init(uint32_t prescaler, uint32_t period)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    
    /* Enable peripheral clocks */
    RCC_APB1PeriphClockCmd(MOTOR_PWM_RCC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    
    /* Configure GPIO pins for alternate function (PWM output) */
    /* Motor A: PD13 - TIM4_CH2 */
    GPIO_PinAFConfig(MOTOR_A_PWM_GPIO, MOTOR_A_PWM_SOURCE, MOTOR_A_PWM_AF);
    
    /* Motor B: PD14 - TIM4_CH3 */
    GPIO_PinAFConfig(MOTOR_B_PWM_GPIO, MOTOR_B_PWM_SOURCE, MOTOR_B_PWM_AF);
    
    /* Configure PWM pins as alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin = MOTOR_A_PWM_PIN | MOTOR_B_PWM_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    
    /* Configure TIM4 time base */
    TIM_TimeBaseStructure.TIM_Prescaler = prescaler;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = period;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(MOTOR_PWM_TIM, &TIM_TimeBaseStructure);
    
    /* Configure PWM mode for Motor A (TIM4_CH2) */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_Pulse = 0;  /* Initial duty cycle = 0% */
    TIM_OC2Init(MOTOR_PWM_TIM, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(MOTOR_PWM_TIM, MOTOR_A_PWM_PRELOAD);
    
    /* Configure PWM mode for Motor B (TIM4_CH3) */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_Pulse = 0;  /* Initial duty cycle = 0% */
    TIM_OC3Init(MOTOR_PWM_TIM, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(MOTOR_PWM_TIM, MOTOR_B_PWM_PRELOAD);
    
    /* Enable auto-reload preload for smooth PWM updates */
    TIM_ARRPreloadConfig(MOTOR_PWM_TIM, ENABLE);
    
    /* Enable TIM4 counter */
    TIM_Cmd(MOTOR_PWM_TIM, ENABLE);
}

/**
 * @brief  Initialize direction control GPIO pins
 * @param  None
 * @retval None
 * @note   Configure as push-pull output for H-bridge control
 */
void Motor_Direction_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* Enable GPIO clock */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    /* Configure direction pins as push-pull output */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    
    /* Motor A direction pins: PD11, PD12 */
    GPIO_InitStructure.GPIO_Pin = MOTOR_A_DIR1_PIN | MOTOR_A_DIR2_PIN;
    GPIO_Init(MOTOR_A_DIR1_GPIO, &GPIO_InitStructure);
    
    /* Motor B direction pins: PD8, PD9 */
    GPIO_InitStructure.GPIO_Pin = MOTOR_B_DIR1_PIN | MOTOR_B_DIR2_PIN;
    GPIO_Init(MOTOR_B_DIR1_GPIO, &GPIO_InitStructure);
	
		MOTOR_A_FORWARD();
    MOTOR_B_FORWARD();

    //Motor_SetSpeed(MOTOR_A,50);
    //Motor_SetSpeed(MOTOR_B,50);
}

/**
 * @brief  Set motor speed and direction
 * @param  motor: MOTOR_A, MOTOR_B, or MOTOR_BOTH
 * @param  speed: -1000 to 1000 (negative = backward, positive = forward)
 * @retval None
 * @note   Speed value is directly mapped to PWM duty cycle
 *         -1000 = 100% backward, 0 = stop, 1000 = 100% forward
 */
void Motor_SetSpeed(uint8_t motor, int16_t speed)
{
    uint8_t direction;
    uint16_t pwm_value;
    
    /* Constrain speed to valid range */
    if (speed > 1000)
        speed = 1000;
    else if (speed < -1000)
        speed = -1000;
    
    /* Determine direction and PWM value */
    if (speed > 0)
    {
        direction = MOTOR_FORWARD;
        pwm_value = (uint16_t)speed;
    }
    else if (speed < 0)
    {
        direction = MOTOR_BACKWARD;
        pwm_value = (uint16_t)(-speed);
    }
    else
    {
        /* speed == 0: brake */
        Motor_Stop(motor);
        return;
    }
    
    /* Set direction and PWM */
    Motor_SetDirection(motor, direction);
    
    if (motor == MOTOR_A || motor == MOTOR_BOTH)
    {
        MOTOR_A_SET_SPEED(pwm_value);
    }
    
    if (motor == MOTOR_B || motor == MOTOR_BOTH)
    {
        MOTOR_B_SET_SPEED(pwm_value);
    }
}

/**
 * @brief  Set motor direction without changing speed
 * @param  motor: MOTOR_A, MOTOR_B, or MOTOR_BOTH
 * @param  direction: MOTOR_FORWARD, MOTOR_BACKWARD, MOTOR_STOP, or MOTOR_COAST
 * @retval None
 */
void Motor_SetDirection(uint8_t motor, uint8_t direction)
{
    if (motor == MOTOR_A || motor == MOTOR_BOTH)
    {
        switch (direction)
        {
            case MOTOR_FORWARD:
                MOTOR_A_FORWARD();
                break;
            case MOTOR_BACKWARD:
                MOTOR_A_BACKWARD();
                break;
            case MOTOR_STOP:
                MOTOR_A_STOP();
                break;
            case MOTOR_COAST:
                MOTOR_A_COAST();
                break;
            default:
                MOTOR_A_STOP();
                break;
        }
    }
    
    if (motor == MOTOR_B || motor == MOTOR_BOTH)
    {
        switch (direction)
        {
            case MOTOR_FORWARD:
                MOTOR_B_FORWARD();
                break;
            case MOTOR_BACKWARD:
                MOTOR_B_BACKWARD();
                break;
            case MOTOR_STOP:
                MOTOR_B_STOP();
                break;
            case MOTOR_COAST:
                MOTOR_B_COAST();
                break;
            default:
                MOTOR_B_STOP();
                break;
        }
    }
}

/**
 * @brief  Stop motor (brake mode)
 * @param  motor: MOTOR_A, MOTOR_B, or MOTOR_BOTH
 * @retval None
 * @note   Brake mode: both direction pins HIGH (short circuit motor terminals)
 *         This provides maximum braking torque
 */
void Motor_Stop(uint8_t motor)
{
    /* Set PWM to 0 */
    if (motor == MOTOR_A || motor == MOTOR_BOTH)
    {
        MOTOR_A_SET_SPEED(0);
        MOTOR_A_STOP();
    }
    
    if (motor == MOTOR_B || motor == MOTOR_BOTH)
    {
        MOTOR_B_SET_SPEED(0);
        MOTOR_B_STOP();
    }
}

/**
 * @brief  Coast motor (freewheel mode)
 * @param  motor: MOTOR_A, MOTOR_B, or MOTOR_BOTH
 * @retval None
 * @note   Coast mode: both direction pins LOW (high impedance)
 *         Motor spins freely with minimal resistance
 */
void Motor_Coast(uint8_t motor)
{
    /* Set PWM to 0 */
    if (motor == MOTOR_A || motor == MOTOR_BOTH)
    {
        MOTOR_A_SET_SPEED(0);
        MOTOR_A_COAST();
    }
    
    if (motor == MOTOR_B || motor == MOTOR_BOTH)
    {
        MOTOR_B_SET_SPEED(0);
        MOTOR_B_COAST();
    }
}

/**
 * @brief  Active braking (same as Motor_Stop)
 * @param  motor: MOTOR_A, MOTOR_B, or MOTOR_BOTH
 * @retval None
 * @note   Provides explicit brake function for clarity
 */
void Motor_Brake(uint8_t motor)
{
    Motor_Stop(motor);
}

/**
 * @brief  Set motor speed as percentage
 * @param  motor: MOTOR_A, MOTOR_B, or MOTOR_BOTH
 * @param  percent: -100.0 to 100.0 (percent of max speed)
 * @retval None
 * @note   Convenient for PID control and user interfaces
 */
void Motor_SetSpeedPercent(uint8_t motor, float percent)
{
    int16_t speed;
    
    /* Constrain percentage */
    if (percent > 100.0f)
        percent = 100.0f;
    else if (percent < -100.0f)
        percent = -100.0f;
    
    /* Convert percentage to PWM value */
    speed = (int16_t)(percent * 10.0f);  /* 100% = 1000 */
    
    Motor_SetSpeed(motor, speed);
}

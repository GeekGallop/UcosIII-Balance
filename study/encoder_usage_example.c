/**
 ******************************************************************************
 * @file    encoder_usage_example.c
 * @brief   Example code for using encoder driver in RTOS environment
 * @note    Demonstrates speed measurement and position tracking
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 */

#include "os.h"
#include "./encoder/encoder.h"
#include "./motor/motor.h"
#include <stdio.h>

/* ==================== Task Configuration ==================== */

#define ENCODER_TASK_PRIO       6   /* High priority for consistent sampling */
#define ENCODER_TASK_STK_SIZE   512
OS_TCB      Encoder_Task_TCB;
CPU_STK     Encoder_Task_STK[ENCODER_TASK_STK_SIZE];

/* ==================== Encoder Task ==================== */

/**
 * @brief  Encoder speed measurement task
 * @note   Runs at 100Hz (10ms period) for speed calculation
 *         Updates encoder data and calculates RPM
 */
void Task_Encoder_Speed(void *p_arg)
{
    OS_ERR err;
    int16_t speed_left, speed_right;
    float rpm_left, rpm_right;
    int32_t pos_left, pos_right;
    
    (void)p_arg;
    
    /* Initialize encoder hardware */
    Encoder_Init();
    
    printf("Encoder Task Started - 100Hz sampling\r\n");
    
    while (1) {
        /* Update both encoders - handles counter overflow automatically */
        Encoder_UpdateAll();
        
        /* Calculate RPM based on 10ms sample time */
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        Encoder_Calculate_Speed(&g_encoder_right_data, 10.0f);
        
        /* Get speed data for motor control */
        speed_left = Encoder_GetSpeed(ENCODER_LEFT);
        speed_right = Encoder_GetSpeed(ENCODER_RIGHT);
        rpm_left = Encoder_GetRPM(ENCODER_LEFT);
        rpm_right = Encoder_GetRPM(ENCODER_RIGHT);
        
        /* Get position data */
        pos_left = Encoder_GetPosition(ENCODER_LEFT);
        pos_right = Encoder_GetPosition(ENCODER_RIGHT);
        
        /* Use data for motor PID control... */
        /* Example: Motor_Control_Update(speed_left, speed_right); */
        
        /* Display every 100ms (every 10th iteration) */
        static uint8_t display_cnt = 0;
        if (++display_cnt >= 10) {
            display_cnt = 0;
            printf("Encoder: L=%5d cnts (%5.1f RPM), R=%5d cnts (%5.1f RPM)\r\n",
                   speed_left, rpm_left, speed_right, rpm_right);
        }
        
        /* 10ms period = 100Hz sampling rate */
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/**
 * @brief  Create encoder measurement task
 * @param  None
 * @retval None
 */
void Encoder_Task_Create(void)
{
    OS_ERR err;
    
    OSTaskCreate(&Encoder_Task_TCB,
                 "Encoder",
                 Task_Encoder_Speed,
                 NULL,
                 ENCODER_TASK_PRIO,
                 &Encoder_Task_STK[0],
                 ENCODER_TASK_STK_SIZE / 10,
                 ENCODER_TASK_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err);
}

/* ==================== Simple Usage Examples ==================== */

#if 0  /* Example code - not compiled */

/**
 * Example 1: Basic encoder reading
 */
void Example_Basic_Reading(void)
{
    /* Initialize */
    Encoder_Init();
    
    /* In main loop or task */
    while (1) {
        /* Update encoders */
        Encoder_UpdateAll();
        
        /* Read values */
        int32_t pos = Encoder_GetPosition(ENCODER_LEFT);
        int16_t speed = Encoder_GetSpeed(ENCODER_LEFT);
        
        /* Use values... */
        
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);  /* 10ms */
    }
}

/**
 * Example 2: RPM calculation
 */
void Example_RPM_Calculation(void)
{
    Encoder_Init();
    
    while (1) {
        /* Update encoder */
        Encoder_Update(ENCODER_LEFT);
        
        /* Calculate RPM for 10ms sample time */
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        
        /* Get RPM */
        float rpm = Encoder_GetRPM(ENCODER_LEFT);
        
        printf("RPM: %.1f\r\n", rpm);
        
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/**
 * Example 3: Position tracking with reset
 */
void Example_Position_Tracking(void)
{
    Encoder_Init();
    
    /* Reset to zero */
    Encoder_ResetAll();
    
    while (1) {
        Encoder_UpdateAll();
        
        int32_t left_pos = Encoder_GetPosition(ENCODER_LEFT);
        int32_t right_pos = Encoder_GetPosition(ENCODER_RIGHT);
        
        /* Check if traveled 10 revolutions */
        if (left_pos > 10 * ENCODER_COUNTS_PER_REV) {
            printf("Reached target position!\r\n");
            Encoder_Reset(ENCODER_LEFT);  /* Reset for next cycle */
        }
        
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/**
 * Example 4: Direction detection
 */
void Example_Direction_Detection(void)
{
    Encoder_Init();
    
    while (1) {
        Encoder_UpdateAll();
        
        int8_t dir_left = Encoder_GetDirection(ENCODER_LEFT);
        int8_t dir_right = Encoder_GetDirection(ENCODER_RIGHT);
        
        const char* dir_str[] = {"STOP", "FWD", "BWD"};
        
        printf("Direction: L=%s, R=%s\r\n",
               dir_str[dir_left + 1],
               dir_str[dir_right + 1]);
        
        OSTimeDly(50, OS_OPT_TIME_DLY, &err);  /* 50ms update */
    }
}

/**
 * Example 5: Motor speed closed-loop control
 */
void Example_Speed_Control(void)
{
    float target_rpm = 100.0f;  /* Target 100 RPM */
    float current_rpm;
    float error;
    float pwm_output;
    
    Encoder_Init();
    Motor_Init();
    
    while (1) {
        /* Update encoder */
        Encoder_Update(ENCODER_LEFT);
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        
        /* Get current speed */
        current_rpm = Encoder_GetRPM(ENCODER_LEFT);
        
        /* Calculate error */
        error = target_rpm - current_rpm;
        
        /* Simple P control */
        pwm_output += error * 0.1f;  /* Kp = 0.1 */
        
        /* Limit PWM */
        if (pwm_output > 100.0f) pwm_output = 100.0f;
        if (pwm_output < -100.0f) pwm_output = -100.0f;
        
        /* Set motor speed */
        Motor_SetSpeedPercent(MOTOR_A, pwm_output);
        
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

#endif /* Example code */

/* End of file */

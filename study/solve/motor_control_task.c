/**
 ******************************************************************************
 * @file    motor_control_task.c
 * @brief   Motor control tasks with speed PID control
 * @note    RTOS-based motor speed control using encoder feedback
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Task Overview:
 * --------------
 * 1. Task_Motor_Speed_Control: High priority task for speed control loop
 *    - Runs at 100Hz (10ms period)
    - Reads encoder data
 *    - Calculates PID output
 *    - Updates motor PWM
 * 
 * 2. Task_Motor_Monitor: Low priority task for monitoring and display
 *    - Runs at 10Hz (100ms period)
 *    - Displays speed and status
 *    - Handles user commands
 * 
 * Control Architecture:
 * ---------------------
 *                    ┌─────────────┐
 *   Target Speed ───→│   PID       │──────→ Motor PWM
 *                    │  Controller │        ┌─────────┐
 *   Encoder Speed ──→│             │        │  Motor  │
 *                    └─────────────┘        └────┬────┘
 *                           ↑                    │
 *                           └────────────────────┘
 *                                    Encoder
 ******************************************************************************/

#include "os.h"
#include "task.h"
#include "./motor/motor.h"
#include "./encoder/encoder.h"
#include "./usart/usart.h"
#include <stdio.h>
#include <math.h>

/* ==================== Task Definitions ==================== */

/* Motor Control Task */
#define MOTOR_CTRL_TASK_PRIO        8
#define MOTOR_CTRL_TASK_STK_SIZE    512
OS_TCB      MotorCtrl_Task_TCB;
CPU_STK     MotorCtrl_Task_STK[MOTOR_CTRL_TASK_STK_SIZE];
void Task_Motor_Speed_Control(void *p_arg);

/* Motor Monitor Task */
#define MOTOR_MON_TASK_PRIO         15
#define MOTOR_MON_TASK_STK_SIZE     512
OS_TCB      MotorMon_Task_TCB;
CPU_STK     MotorMon_Task_STK[MOTOR_MON_TASK_STK_SIZE];
void Task_Motor_Monitor(void *p_arg);

/* ==================== PID Controller Structure ==================== */

typedef struct {
    float kp;               /* Proportional gain */
    float ki;               /* Integral gain */
    float kd;               /* Derivative gain */
    
    float setpoint;         /* Target value */
    float input;            /* Current value (feedback) */
    float output;           /* PID output */
    
    float error;            /* Current error */
    float last_error;       /* Previous error */
    float integral;         /* Integral accumulation */
    float derivative;       /* Derivative term */
    
    float output_min;       /* Output limit minimum */
    float output_max;       /* Output limit maximum */
    float integral_max;     /* Integral windup limit */
    
    uint8_t enabled;        /* PID enabled flag */
} PID_Controller_t;

/* ==================== Motor Control Structure ==================== */

typedef struct {
    uint8_t motor_id;           /* MOTOR_A or MOTOR_B */
    uint8_t encoder_id;         /* ENCODER_A or ENCODER_B */
    
    float target_speed_rpm;     /* Target speed in RPM */
    float current_speed_rpm;    /* Current speed in RPM */
    float pwm_output;           /* PWM output (-100 to 100) */
    
    PID_Controller_t pid;       /* PID controller */
    
    uint8_t control_mode;       /* 0=manual, 1=speed PID */
    uint8_t direction;          /* 0=stop, 1=forward, 2=backward */
} Motor_Control_t;

/* ==================== Global Variables ==================== */

static Motor_Control_t g_motor_a_ctrl;
static Motor_Control_t g_motor_b_ctrl;

OS_MUTEX Motor_Ctrl_Mutex;      /* Protect motor control data */
OS_SEM   Motor_Ctrl_Sem;        /* Signal control update */

/* ==================== PID Controller Functions ==================== */

/**
 * @brief  Initialize PID controller
 * @param  pid: Pointer to PID structure
 * @param  kp: Proportional gain
 * @param  ki: Integral gain
 * @param  kd: Derivative gain
 * @retval None
 */
void PID_Init(PID_Controller_t *pid, float kp, float ki, float kd)
{
    if (pid == NULL) return;
    
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    
    pid->setpoint = 0.0f;
    pid->input = 0.0f;
    pid->output = 0.0f;
    
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->integral = 0.0f;
    pid->derivative = 0.0f;
    
    pid->output_min = -100.0f;  /* -100% PWM */
    pid->output_max = 100.0f;   /* +100% PWM */
    pid->integral_max = 50.0f;  /* Anti-windup limit */
    
    pid->enabled = 0;
}

/**
 * @brief  Compute PID output
 * @param  pid: Pointer to PID structure
 * @param  input: Current feedback value
 * @param  dt: Time step in seconds
 * @retval PID output
 */
float PID_Compute(PID_Controller_t *pid, float input, float dt)
{
    float output;
    
    if (pid == NULL || dt <= 0) return 0.0f;
    if (!pid->enabled) return 0.0f;
    
    /* Store input */
    pid->input = input;
    
    /* Calculate error */
    pid->error = pid->setpoint - pid->input;
    
    /* Calculate integral with anti-windup */
    pid->integral += pid->error * dt;
    if (pid->integral > pid->integral_max)
        pid->integral = pid->integral_max;
    else if (pid->integral < -pid->integral_max)
        pid->integral = -pid->integral_max;
    
    /* Calculate derivative */
    pid->derivative = (pid->error - pid->last_error) / dt;
    pid->last_error = pid->error;
    
    /* Calculate PID output */
    output = (pid->kp * pid->error) + 
             (pid->ki * pid->integral) + 
             (pid->kd * pid->derivative);
    
    /* Limit output */
    if (output > pid->output_max)
        output = pid->output_max;
    else if (output < pid->output_min)
        output = pid->output_min;
    
    pid->output = output;
    
    return output;
}

/**
 * @brief  Set PID setpoint
 * @param  pid: Pointer to PID structure
 * @param  setpoint: Target value
 * @retval None
 */
void PID_SetSetpoint(PID_Controller_t *pid, float setpoint)
{
    if (pid == NULL) return;
    pid->setpoint = setpoint;
}

/**
 * @brief  Enable/disable PID controller
 * @param  pid: Pointer to PID structure
 * @param  enable: 1=enable, 0=disable
 * @retval None
 */
void PID_Enable(PID_Controller_t *pid, uint8_t enable)
{
    if (pid == NULL) return;
    
    if (enable && !pid->enabled)
    {
        /* Reset state when enabling */
        pid->integral = 0.0f;
        pid->last_error = 0.0f;
    }
    
    pid->enabled = enable;
}

/* ==================== Motor Control Functions ==================== */

/**
 * @brief  Initialize motor control system
 * @param  None
 * @retval None
 */
void Motor_Control_Init(void)
{
    OS_ERR err;
    
    /* Initialize hardware */
    Motor_Init();
    Encoder_Init();
    
    /* Create synchronization objects */
    OSMutexCreate(&Motor_Ctrl_Mutex, "Motor Ctrl Mutex", &err);
    OSSemCreate(&Motor_Ctrl_Sem, "Motor Ctrl Sem", 0, &err);
    
    /* Initialize Motor A control structure */
    g_motor_a_ctrl.motor_id = MOTOR_A;
    g_motor_a_ctrl.encoder_id = ENCODER_A;
    g_motor_a_ctrl.target_speed_rpm = 0.0f;
    g_motor_a_ctrl.current_speed_rpm = 0.0f;
    g_motor_a_ctrl.pwm_output = 0.0f;
    g_motor_a_ctrl.control_mode = 0;  /* Manual mode */
    g_motor_a_ctrl.direction = 0;
    PID_Init(&g_motor_a_ctrl.pid, 2.0f, 0.5f, 0.1f);  /* Default PID values */
    
    /* Initialize Motor B control structure */
    g_motor_b_ctrl.motor_id = MOTOR_B;
    g_motor_b_ctrl.encoder_id = ENCODER_B;
    g_motor_b_ctrl.target_speed_rpm = 0.0f;
    g_motor_b_ctrl.current_speed_rpm = 0.0f;
    g_motor_b_ctrl.pwm_output = 0.0f;
    g_motor_b_ctrl.control_mode = 0;
    g_motor_b_ctrl.direction = 0;
    PID_Init(&g_motor_b_ctrl.pid, 2.0f, 0.5f, 0.1f);
}

/**
 * @brief  Set motor target speed
 * @param  motor: MOTOR_A or MOTOR_B
 * @param  speed_rpm: Target speed in RPM
 * @retval None
 */
void Motor_Control_SetSpeed(uint8_t motor, float speed_rpm)
{
    Motor_Control_t *ctrl;
    
    if (motor == MOTOR_A)
        ctrl = &g_motor_a_ctrl;
    else if (motor == MOTOR_B)
        ctrl = &g_motor_b_ctrl;
    else
        return;
    
    ctrl->target_speed_rpm = speed_rpm;
    PID_SetSetpoint(&ctrl->pid, speed_rpm);
    
    /* If in manual mode, directly set PWM */
    if (ctrl->control_mode == 0)
    {
        /* Simple open-loop: assume 100 RPM per 10% PWM */
        float pwm = speed_rpm / 10.0f;
        Motor_SetSpeedPercent(ctrl->motor_id, pwm);
    }
}

/**
 * @brief  Enable speed PID control
 * @param  motor: MOTOR_A or MOTOR_B
 * @param  enable: 1=enable PID, 0=manual mode
 * @retval None
 */
void Motor_Control_EnablePID(uint8_t motor, uint8_t enable)
{
    Motor_Control_t *ctrl;
    
    if (motor == MOTOR_A)
        ctrl = &g_motor_a_ctrl;
    else if (motor == MOTOR_B)
        ctrl = &g_motor_b_ctrl;
    else
        return;
    
    ctrl->control_mode = enable ? 1 : 0;
    PID_Enable(&ctrl->pid, enable);
}

/**
 * @brief  Update motor control (called from control task)
 * @param  ctrl: Pointer to motor control structure
 * @param  dt: Time step in seconds
 * @retval None
 */
void Motor_Control_Update(Motor_Control_t *ctrl, float dt)
{
    float pid_output;
    int16_t pwm_value;
    
    if (ctrl == NULL) return;
    
    /* Read current speed from encoder */
    ctrl->current_speed_rpm = Encoder_GetRPM(ctrl->encoder_id);
    
    /* If PID enabled, compute PID output */
    if (ctrl->control_mode == 1)
    {
        pid_output = PID_Compute(&ctrl->pid, ctrl->current_speed_rpm, dt);
        ctrl->pwm_output = pid_output;
        
        /* Convert to PWM value (-1000 to 1000) */
        pwm_value = (int16_t)(pid_output * 10.0f);
        Motor_SetSpeed(ctrl->motor_id, pwm_value);
    }
}

/* ==================== RTOS Tasks ==================== */

/**
 * @brief  Motor speed control task
 * @note   High priority task running at 100Hz
 *         Implements closed-loop speed control
 */
void Task_Motor_Speed_Control(void *p_arg)
{
    OS_ERR err;
    (void)p_arg;
    
    /* Wait for system initialization */
    OSTimeDly(100, OS_OPT_TIME_DLY, &err);
    
    printf("Motor Speed Control Task Started\r\n");
    
    while (1)
    {
        /* Update encoders */
        Encoder_UpdateAll();
        
        /* Calculate RPM (10ms sample time) */
        Encoder_Calculate_Speed(&g_encoder_a_data, 10.0f);
        Encoder_Calculate_Speed(&g_encoder_b_data, 10.0f);
        
        /* Update motor control (PID if enabled) */
        Motor_Control_Update(&g_motor_a_ctrl, 0.01f);  /* 10ms = 0.01s */
        Motor_Control_Update(&g_motor_b_ctrl, 0.01f);
        
        /* Signal monitor task */
        OSSemPost(&Motor_Ctrl_Sem, OS_OPT_POST_1, &err);
        
        /* Run at 100Hz (10ms period) */
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/**
 * @brief  Motor monitor task
 * @note   Low priority task for display and user interface
 */
void Task_Motor_Monitor(void *p_arg)
{
    OS_ERR err;
    uint8_t display_count = 0;
    (void)p_arg;
    
    /* Wait for control task */
    OSTimeDly(200, OS_OPT_TIME_DLY, &err);
    
    printf("Motor Monitor Task Started\r\n");
    printf("Commands:\r\n");
    printf("  'a' - Set Motor A speed\r\n");
    printf("  'b' - Set Motor B speed\r\n");
    printf("  'p' - Enable PID control\r\n");
    printf("  'm' - Manual mode\r\n");
    printf("  's' - Stop all motors\r\n");
    
    while (1)
    {
        /* Wait for control update signal (with timeout) */
        OSSemPend(&Motor_Ctrl_Sem, 100, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        /* Display status every 10 samples (100ms) */
        display_count++;
        if (display_count >= 10)
        {
            display_count = 0;
            
            /* Display motor status */
            printf("\r\n=== Motor Status ===\r\n");
            printf("Motor A: Target=%.1f RPM, Current=%.1f RPM, PWM=%.1f%%\r\n",
                   g_motor_a_ctrl.target_speed_rpm,
                   g_motor_a_ctrl.current_speed_rpm,
                   g_motor_a_ctrl.pwm_output);
            printf("Motor B: Target=%.1f RPM, Current=%.1f RPM, PWM=%.1f%%\r\n",
                   g_motor_b_ctrl.target_speed_rpm,
                   g_motor_b_ctrl.current_speed_rpm,
                   g_motor_b_ctrl.pwm_output);
            
            /* Display encoder data */
            printf("Encoder A: Pos=%ld, Delta=%ld\r\n",
                   Encoder_GetPosition(ENCODER_A),
                   Encoder_GetDeltaPosition(ENCODER_A));
            printf("Encoder B: Pos=%ld, Delta=%ld\r\n",
                   Encoder_GetPosition(ENCODER_B),
                   Encoder_GetDeltaPosition(ENCODER_B));
        }
    }
}

/**
 * @brief  Create motor control tasks
 * @param  None
 * @retval None
 * @note   Call this in application start task
 */
void Motor_Control_Tasks_Create(void)
{
    OS_ERR err;
    
    /* Initialize motor control system */
    Motor_Control_Init();
    
    /* Create speed control task */
    OSTaskCreate(&MotorCtrl_Task_TCB,
                 "Motor Ctrl",
                 Task_Motor_Speed_Control,
                 NULL,
                 MOTOR_CTRL_TASK_PRIO,
                 &MotorCtrl_Task_STK[0],
                 MOTOR_CTRL_TASK_STK_SIZE / 10,
                 MOTOR_CTRL_TASK_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err);
    
    /* Create monitor task */
    OSTaskCreate(&MotorMon_Task_TCB,
                 "Motor Mon",
                 Task_Motor_Monitor,
                 NULL,
                 MOTOR_MON_TASK_PRIO,
                 &MotorMon_Task_STK[0],
                 MOTOR_MON_TASK_STK_SIZE / 10,
                 MOTOR_MON_TASK_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err);
}

/* ==================== Example Usage ==================== */

#if 0  /* Example code - not compiled */

void Example_Motor_Control(void)
{
    /* Initialize motor control */
    Motor_Control_Init();
    
    /* Set Motor A to run at 100 RPM (manual mode) */
    Motor_Control_SetSpeed(MOTOR_A, 100.0f);
    
    /* Enable PID control for Motor A */
    Motor_Control_EnablePID(MOTOR_A, 1);
    
    /* Motor will now maintain 100 RPM using PID control */
    
    /* Later: change target speed */
    Motor_Control_SetSpeed(MOTOR_A, 150.0f);
    
    /* Stop motor */
    Motor_Control_SetSpeed(MOTOR_A, 0.0f);
    Motor_Stop(MOTOR_A);
}

#endif

/* End of file */

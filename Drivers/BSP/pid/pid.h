#ifndef __PID_H
#define __PID_H

#include "os.h"
#include "os_cfg.h"
#include "stdint.h"

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

/* ==================== PID Data Queue Structures ==================== */

/**
 * @brief  Encoder data packet for PID control
 * @note   Sent from task_encoder to PID task via queue
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
} PID_Encoder_Data_t;

/**
 * @brief  MPU6050 data packet for PID control
 * @note   Sent from task_mpu to PID task via queue
 */
typedef struct {
    float roll;             /* Roll angle (deg) */
    float pitch;            /* Pitch angle (deg) */
    float yaw;              /* Yaw angle (deg) */
} PID_MPU_Data_t;

/* ==================== External Queue Variables ==================== */

extern OS_Q g_pid_encoder_queue;    /* Queue for encoder data to PID */
extern OS_Q g_pid_mpu_queue;        /* Queue for MPU data to PID */

/* ==================== Function Declarations ==================== */

/**
 * @brief  Initialize PID controller
 * @param  pid: Pointer to PID controller structure
 * @param  kp: Proportional gain
 * @param  ki: Integral gain
 * @param  kd: Derivative gain
 * @retval None
 */
void PID_Init(PID_Controller_t *pid, float kp, float ki, float kd);

/**
 * @brief  Calculate PID output
 * @param  pid: Pointer to PID controller structure
 * @retval None
 * @note   Updates pid->output based on setpoint and input
 */
void PID_Calculate(PID_Controller_t *pid);

/**
 * @brief  Set PID target value
 * @param  pid: Pointer to PID controller structure
 * @param  setpoint: Target value
 * @retval None
 */
void PID_SetSetpoint(PID_Controller_t *pid, float setpoint);

/**
 * @brief  Set PID input (feedback) value
 * @param  pid: Pointer to PID controller structure
 * @param  input: Current feedback value
 * @retval None
 */
void PID_SetInput(PID_Controller_t *pid, float input);

/**
 * @brief  Reset PID controller
 * @param  pid: Pointer to PID controller structure
 * @retval None
 * @note   Clears integral and error history
 */
void PID_Reset(PID_Controller_t *pid);

/**
 * @brief  Initialize PID queues and system
 * @param  None
 * @retval 0 on success, -1 on failure
 * @note   Creates encoder and MPU queues for PID task
 */
int PID_System_Init(void);

/**
 * @brief  Send encoder data to PID queue
 * @param  encoder_data: Pointer to encoder data structure
 * @retval 0 on success, -1 on failure
 * @note   Non-blocking send from task_encoder
 */
int PID_Send_Encoder_Data(PID_Encoder_Data_t *encoder_data);

/**
 * @brief  Send MPU data to PID queue
 * @param  mpu_data: Pointer to MPU data structure
 * @retval 0 on success, -1 on failure
 * @note   Non-blocking send from task_mpu
 */
int PID_Send_MPU_Data(PID_MPU_Data_t *mpu_data);

/**
 * @brief  PID control task
 * @param  p_arg: Task argument (unused)
 * @retval None
 * @note   Receives data from encoder and MPU queues
 *         Runs at 100Hz control rate
 */
void Task_PID_Control(void *p_arg);

#endif /* __PID_H */

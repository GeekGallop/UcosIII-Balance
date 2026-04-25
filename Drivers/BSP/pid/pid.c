
/**
 ******************************************************************************
 * @file    pid.c
 * @brief   PID controller implementation with queue-based data reception
 * @note    Receives encoder and MPU data via message queues
 * @author  User
 * @date    2026-02-21
 ******************************************************************************
 * 
 * Implementation Details:
 * -----------------------
 * 
 * 1. Message Queue Communication:
 *    - Uses TWO separate OS_Q for receiving data from different tasks
 *    - g_pid_encoder_queue: for encoder data from task_encoder
 *    - g_pid_mpu_queue: for MPU6050 data from task_mpu
 *    - Non-blocking send from producers (encoder and MPU tasks)
 *    - Non-blocking receive with polling in PID task
 * 
 * 2. Control Rate:
 *    - 100Hz control loop (10ms period)
 *    - Matches encoder and MPU sampling rates
 * 
 * 3. Data Flow:
 *    - task_encoder -> g_pid_encoder_queue -> Task_PID_Control
 *    - task_mpu -> g_pid_mpu_queue -> Task_PID_Control
 ******************************************************************************/

#include "./pid/pid.h"
#include <stdio.h>
#include <string.h>
#include "./motor/motor.h"
/* ==================== Message Queues ==================== */

OS_Q g_pid_encoder_queue;       /* Queue for encoder data to PID */
OS_Q g_pid_mpu_queue;           /* Queue for MPU data to PID */

/* Static packet buffers for non-blocking send */
static PID_Encoder_Data_t encoder_packet;
static PID_MPU_Data_t mpu_packet;

/* Cached PID input data */
static struct {
    /* Encoder data */
    int32_t pos_left;
    int32_t pos_right;
    int16_t speed_left;
    int16_t speed_right;
    float rpm_left;
    float rpm_right;
    int8_t dir_left;
    int8_t dir_right;
    uint8_t encoder_valid;
    
    float roll;
    float pitch;
    uint8_t mpu_valid;
} g_pid_cached_data = {0};
extern int pos;
/* PID controllers for left and right motors */
static PID_Controller_t pid_left;
static PID_Controller_t pid_right;

/**
 * @brief  Initialize PID controller
 * @param  pid: Pointer to PID controller structure
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
    
    pid->enabled = 1;
}

/**
 * @brief  Calculate PID output
 * @param  pid: Pointer to PID controller structure
 * @retval None
 */
void PID_Calculate(PID_Controller_t *pid)
{
    float p_out, i_out, d_out;
    
    if (pid == NULL || !pid->enabled) return;
    
    /* Calculate error */
    pid->error = pid->input-pid->setpoint ;
    
    /* Proportional term */
    p_out = pid->kp * pid->error;
    
    /* Integral term with anti-windup */
    pid->integral += pid->error;
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < -pid->integral_max) {
        pid->integral = -pid->integral_max;
    }
    i_out = pid->ki * pid->integral;
    
    /* Derivative term */
    pid->derivative = pid->error - pid->last_error;
    d_out = pid->kd * pid->derivative;
    
    /* Calculate total output */
    pid->output = p_out + i_out + d_out;
    
    /* Output limiting */
    if (pid->output > pid->output_max) {
        pid->output = pid->output_max;
    } else if (pid->output < pid->output_min) {
        pid->output = pid->output_min;
    }
    
    /* Save error for next iteration */
    pid->last_error = pid->error;
}



/**
 * @brief  Initialize PID queues and system
 * @param  None
 * @retval 0 on success, -1 on failure
 */
int PID_System_Init(void)
{
    OS_ERR err;
    
    /* Create message queue for encoder data */
    OSQCreate(&g_pid_encoder_queue,
              (CPU_CHAR *)"PID Encoder Queue",
              10,  /* Maximum 10 messages in queue */
              &err);
    
    if (err != OS_ERR_NONE) {
        printf("PID Encoder Queue Create Failed!\r\n");
        return -1;
    }
    
    /* Create message queue for MPU data */
    OSQCreate(&g_pid_mpu_queue,
              (CPU_CHAR *)"PID MPU Queue",
              10,  /* Maximum 10 messages in queue */
              &err);
    
    if (err != OS_ERR_NONE) {
        printf("PID MPU Queue Create Failed!\r\n");
        return -1;
    }
    
    /* Initialize PID controllers with default parameters */
    PID_Init(&pid_left, 1.0f, 0.2f, 0.0f);
    PID_Init(&pid_right, 1.0f, 0.2f, 0.0f);
    
    printf("PID System Initialized - Dual Queue Mode\r\n");
    
    return 0;
}


/**
 * @brief  Send encoder data to PID queue
 * @param  encoder_data: Pointer to encoder data structure
 * @retval 0 on success, -1 on failure
 */
int PID_Send_Encoder_Data(PID_Encoder_Data_t *encoder_data)
{
    OS_ERR err;
    PID_Encoder_Data_t *packet;
    
    if (encoder_data == NULL) return -1;
    
    /* Use static allocation to avoid dynamic memory */
    packet = &encoder_packet;
    *packet = *encoder_data;
    
    /* Post to PID encoder queue (non-blocking) */
    OSQPost(&g_pid_encoder_queue,
            (void *)packet,
            sizeof(PID_Encoder_Data_t),
            OS_OPT_POST_FIFO | OS_OPT_POST_NO_SCHED,
            &err);
    
    return (err == OS_ERR_NONE) ? 0 : -1;
}

/**
 * @brief  Send MPU data to PID queue
 * @param  mpu_data: Pointer to MPU data structure
 * @retval 0 on success, -1 on failure
 */
int PID_Send_MPU_Data(PID_MPU_Data_t *mpu_data)
{
    OS_ERR err;
    PID_MPU_Data_t *packet;
    
    if (mpu_data == NULL) return -1;
    
    /* Use static allocation to avoid dynamic memory */
    packet = &mpu_packet;
    *packet = *mpu_data;
    
    /* Post to PID MPU queue (non-blocking) */
    OSQPost(&g_pid_mpu_queue,
            (void *)packet,
            sizeof(PID_MPU_Data_t),
            OS_OPT_POST_FIFO | OS_OPT_POST_NO_SCHED,
            &err);
    
    return (err == OS_ERR_NONE) ? 0 : -1;
}

/**
 * @brief  Process encoder data packet
 * @param  packet: Pointer to encoder data packet
 * @retval None
 */
static void PID_Process_Encoder_Data(PID_Encoder_Data_t *packet)
{
    if (packet == NULL) return;
    
    /* Cache encoder data */
    g_pid_cached_data.pos_left = packet->position_left;
    g_pid_cached_data.pos_right = packet->position_right;
    g_pid_cached_data.speed_left = packet->speed_left;
    g_pid_cached_data.speed_right = packet->speed_right;
    g_pid_cached_data.rpm_left = packet->rpm_left;
    g_pid_cached_data.rpm_right = packet->rpm_right;
    g_pid_cached_data.dir_left = packet->direction_left;
    g_pid_cached_data.dir_right = packet->direction_right;
    g_pid_cached_data.encoder_valid = 1;
}

/**
 * @brief  Process MPU data packet
 * @param  packet: Pointer to MPU data packet
 * @retval None
 */
static void PID_Process_MPU_Data(PID_MPU_Data_t *packet)
{
    if (packet == NULL) return;
    g_pid_cached_data.roll = packet->roll;
    g_pid_cached_data.pitch = packet->pitch;
    g_pid_cached_data.mpu_valid = 1;
}
/**
 * @brief  Process all pending encoder queue messages
 * @param  None
 * @retval None
 */
static void PID_Process_Encoder_Queue(void)
{
    OS_ERR err;
    OS_MSG_SIZE msg_size;
    PID_Encoder_Data_t *packet;
    CPU_TS ts;
    
    /* Process all pending encoder messages (non-blocking) */
    do {
        packet = (PID_Encoder_Data_t *)OSQPend(
            &g_pid_encoder_queue,
            0,                              /* No timeout (poll) */
            OS_OPT_PEND_NON_BLOCKING,
            &msg_size,
            &ts,
            &err
        );
        
        if (err == OS_ERR_NONE && packet != NULL) {
            PID_Process_Encoder_Data(packet);
        }
    } while (err == OS_ERR_NONE);
}
/**
 * @brief  Process all pending MPU queue messages
 * @param  None
 * @retval None
 */
static void PID_Process_MPU_Queue(void)
{
    OS_ERR err;
    OS_MSG_SIZE msg_size;
    PID_MPU_Data_t *packet;
    CPU_TS ts;
    
    /* Process all pending MPU messages (non-blocking) */
    do {
        packet = (PID_MPU_Data_t *)OSQPend(
            &g_pid_mpu_queue,
            0,                              /* No timeout (poll) */
            OS_OPT_PEND_NON_BLOCKING,
            &msg_size,
            &ts,
            &err
        );
        
        if (err == OS_ERR_NONE && packet != NULL) {
            PID_Process_MPU_Data(packet);
        }
    } while (err == OS_ERR_NONE);
}
/**
 * @brief  Execute PID control with cached data
 * @param  None
 * @retval None
 */
static void PID_Execute_Control(void)
{
    /* Use encoder speed as feedback for motor PID control */
    // if (g_pid_cached_data.encoder_valid) {
    //     /* Update PID inputs with current speed */
    //     PID_SetInput(&pid_left, (float)g_pid_cached_data.speed_left);
    //     PID_SetInput(&pid_right, (float)g_pid_cached_data.speed_right);
        
    //     /* Calculate PID outputs */
    //     PID_Calculate(&pid_left);
    //     PID_Calculate(&pid_right);
        
    //     /* Here you would apply the PID outputs to motor control */
    //     /* Example: Motor_SetPWM(LEFT, pid_left.output); */
    //     /* Example: Motor_SetPWM(RIGHT, pid_right.output); */
    // }
    
    
    if (g_pid_cached_data.mpu_valid) {
				pid_left.setpoint= pos;
        pid_left.input=g_pid_cached_data.pos_left;
        PID_Calculate(&pid_left);
				Motor_SetSpeed(MOTOR_B,pid_left.output);
        Motor_SetSpeed(MOTOR_A,pid_left.output);
    }
		static u16 loop_cnt=0;
		if (++loop_cnt >= 100) {
        loop_cnt = 0;
			  printf("PID-OUT:%.2f,PID-ACT:%.2f\r\n",pid_left.output,pid_left.setpoint);
        }
		
}

/**
 * @brief  PID control task
 * @param  p_arg: Task argument (unused)
 * @retval None
 * @note   Receives data from encoder and MPU queues
 *         Runs at 100Hz control rate
 */
void Task_PID_Control(void *p_arg)
{
    OS_ERR err;
    uint32_t loop_cnt = 0;
    
    (void)p_arg;
    
    /* Wait for system initialization */
    OSTimeDly(400, OS_OPT_TIME_DLY, &err);
    
    /* Initialize PID system */
    if (PID_System_Init() != 0) {
        printf("PID System Init Failed!\r\n");
        OSTaskDel(NULL, &err);
        return;
    }
    
    
    printf("PID Control Task Started - 100Hz control loop\r\n");
    
    while (1) {
        /* Process encoder queue messages */
        PID_Process_Encoder_Queue();
        
        /* Process MPU queue messages */
        PID_Process_MPU_Queue();
        
        /* Execute PID control with latest data */
        PID_Execute_Control();
        
        if (++loop_cnt >= 100) {
            loop_cnt = 0;
        }
        
        /* 10ms period = 100Hz control rate */
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

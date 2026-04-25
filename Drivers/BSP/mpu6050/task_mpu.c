/**
 ******************************************************************************
 * @file    task_mpu.c
 * @brief   MPU6050 task implementation
 * @note    Handles sensor reading and LCD display update
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Implementation Details:
 * -----------------------
 * 
 * 1. Sampling Rate:
 *    - 100Hz sampling (10ms period)
 *    - Processes raw data to get accel, gyro, and temperature
 * 
 * 2. Data Distribution:
 *    - Sends data to LCD display task via message queue
 *    - Maintains global variable for other tasks (with mutex protection)
 *    - Serial output for debugging
 * 
 * 3. Thread Safety:
 *    - Uses mutex to protect global data access
 *    - Minimizes mutex hold time
 ******************************************************************************/

#include "./mpu6050/task_mpu.h"
#include "./mpu6050/mpu6050.h"
#include "./iic/iic.h"
#include "./lcd/task_lcd.h"
#include "usart.h"
#include "os.h"
#include "./pid/pid.h"
/* Global MPU6050 data (protected by mutex) */
MPU6050_Data_t g_mpu6050_data = {0};

/* Mutex for data protection */
OS_MUTEX MPU6050_Mutex;


/* Local data structures */
static MPU6050_RawData_t raw_data;
static MPU6050_Data_t processed_data;
static MPU6050_Attitude_t attitude_data;

static PID_MPU_Data_t pid_mpu_data;
/**
 * @brief  MPU6050 data reading task
 * @param  p_arg: Task argument (unused)
 * @retval None
 * @note   Reads sensor data at 100Hz
 *         Sends data to LCD display task via queue
 */
void Task_MPU6050_Read(void *p_arg)
{
    OS_ERR err;
    
    (void)p_arg;
    
    /* Wait for system initialization */
    OSTimeDly(200, OS_OPT_TIME_DLY, &err);
    
    /* Initialize MPU6050 */
    if (MPU6050_Init() != 0) {
        printf("MPU6050 initialization failed!\r\n");
        OSTaskDel(NULL, &err);
        return;
    }
    
    printf("MPU6050 Task Started - 100Hz sampling\r\n");
    
    while (1) {
        /* Read raw data from sensor */
        if (MPU6050_Read_Raw_Data(&raw_data) == 0) {
            /* Process raw data to get physical values */
            MPU6050_Process_Data(&raw_data, &processed_data);
            
            /* Calculate attitude (Roll and Pitch) */
            MPU6050_Calculate_Attitude(&processed_data, &attitude_data);
            
            /* Update global data with mutex protection */
            OSMutexPend(&MPU6050_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
            g_mpu6050_data = processed_data;
            OSMutexPost(&MPU6050_Mutex, OS_OPT_POST_NONE, &err);
            
            /* Send to LCD display task (non-blocking) */
            LCD_Send_MPU_Data(&attitude_data, processed_data.temp); 

            pid_mpu_data.roll = attitude_data.roll;
            pid_mpu_data.pitch = attitude_data.pitch;
            pid_mpu_data.yaw = attitude_data.yaw;
            PID_Send_MPU_Data(&pid_mpu_data);
        }
        
        /* 10ms period = 100Hz sampling rate */
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/* End of file */

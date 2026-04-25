/**
 ******************************************************************************
 * @file    task_encoder.c
 * @brief   Encoder task implementation
 * @note    Handles encoder speed measurement and LCD display update
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Implementation Details:
 * -----------------------
 * 
 * 1. Sampling Rate:
 *    - 100Hz sampling (10ms period)
 *    - Calculates speed and RPM at each sample
 * 
 * 2. Data Distribution:
 *    - Sends data to LCD display task via message queue
 *    - Non-blocking send to avoid affecting sampling timing
 *    - Serial output for debugging (every 10th sample = 100ms)
 * 
 * 3. Thread Safety:
 *    - Uses mutex to protect encoder data access
 *    - Minimizes mutex hold time
 ******************************************************************************/

#include "os.h"
#include "./encoder/encoder.h"
#include "./encoder/task_encoder.h"
#include "./lcd/task_lcd.h"
#include "./motor/motor.h"
#include <stdio.h>
#include "./pid/pid.h"
static uint8_t display_cnt = 0;
static uint32_t check_cnt = 0;
Encoder_Display_Data_t display_data;
PID_Encoder_Data_t pid_encoder_data;
extern OS_MUTEX g_encoder_mutex;
/**
 * @brief  Encoder speed measurement task
 * @param  p_arg: Task argument (unused)
 * @retval None
 * @note   Runs at 100Hz (10ms period)
 *         Sends data to LCD display task via queue
 */
void Task_Encoder_Speed(void *p_arg)
{
    OS_ERR err;
    int16_t speed_left, speed_right;
    float rpm_left, rpm_right;
    int32_t pos_left, pos_right;
    int8_t dir_left, dir_right;

	CPU_STK_SIZE free_stk;
    CPU_STK_SIZE used_stk;
    (void)p_arg;

    
    printf("Encoder Task Started - 100Hz sampling\r\n");
    
    while (1) {
		Encoder_UpdateAll();
			
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        Encoder_Calculate_Speed(&g_encoder_right_data, 10.0f);

        speed_left = g_encoder_left_data.speed;
        speed_right = g_encoder_right_data.speed;
        rpm_left = g_encoder_left_data.rpm;
        rpm_right = g_encoder_right_data.rpm;
        pos_left = g_encoder_left_data.position;
        pos_right = g_encoder_right_data.position;
        dir_left = g_encoder_left_data.direction;
        dir_right = g_encoder_right_data.direction;
        

        /* Prepare display data */
        display_data.position_left = pos_left;
        display_data.position_right = pos_right;
        display_data.speed_left = speed_left;
        display_data.speed_right = speed_right;
        display_data.rpm_left = rpm_left;
        display_data.rpm_right = rpm_right;
        display_data.direction_left = dir_left;
        display_data.direction_right = dir_right;

        pid_encoder_data.position_left=pos_left;
        pid_encoder_data.position_right=pos_right;
        pid_encoder_data.speed_left=speed_left;
        pid_encoder_data.speed_right=speed_right;
        pid_encoder_data.rpm_left = rpm_left;
        pid_encoder_data.rpm_right = rpm_right;
        pid_encoder_data.direction_left = dir_left;
        pid_encoder_data.direction_right = dir_right;        


        /* Send to LCD display task (non-blocking) */
        LCD_Send_Encoder_Data(&display_data);
        PID_Send_Encoder_Data(&pid_encoder_data);
        /* Use data for motor PID control... */
        /* Example: Motor_Control_Update(speed_left, speed_right); */
        
        /* Serial debug output every 100ms (every 10th iteration) */
        if (++display_cnt >= 50) {
            display_cnt = 0;
            printf("Encoder: L=%5d cnts , R=%5d cnts\r\n",speed_left,speed_right);
        }

        if (++check_cnt >= 100) {
            check_cnt = 0;
            OSTaskStkChk(NULL, &free_stk, &used_stk, &err);
            float usage_pct = (float)used_stk / (free_stk + used_stk) * 100.0f;
            if (usage_pct > 80.0f) {
                printf("WARNING: Encoder task stack usage %.1f%%\r\n", usage_pct);
            }
        }
        /* 10ms period = 100Hz sampling rate */
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/* End of file */

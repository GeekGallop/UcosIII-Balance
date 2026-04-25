/**
 ******************************************************************************
 * @file    task_lcd.c
 * @brief   LCD display task implementation
 * @note    Unified display for MPU6050 and Encoder data using separate queues
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Implementation Details:
 * -----------------------
 * 
 * 1. Message Queue Communication:
 *    - Uses TWO separate OS_Q for receiving data from different tasks
 *    - g_mpu_display_queue: for MPU6050 data
 *    - g_encoder_display_queue: for Encoder data
 *    - Non-blocking send from producers (MPU6050 and Encoder tasks)
 *    - Blocking receive with timeout in LCD task
 * 
 * 2. Screen Layout:
 *    - Row 0-2:   MPU6050 Roll, Pitch, Temperature
 *    - Row 4-7:   Encoder Left Position, Speed, RPM, Direction
 *    - Row 8-11:  Encoder Right Position, Speed, RPM, Direction
 * 
 * 3. Refresh Strategy:
 *    - 20Hz refresh rate (50ms period)
 *    - Partial screen updates to reduce flicker
 *    - Data is cached and only updated when new data arrives
 ******************************************************************************/

#include "./lcd/task_lcd.h"
#include "./lcd/lcd.h"
#include <stdio.h>
#include <string.h>
#include "os.h"

/* Two separate message queues for different data types */
OS_Q g_mpu_display_queue;           /* Queue for MPU6050 data */
OS_Q g_encoder_display_queue;       /* Queue for Encoder data */

/* Static packet buffers for non-blocking send */
static MPU_Display_Data_t mpu_packet;
static Encoder_Display_Data_t enc_packet;

/* Cached display data */
static struct {
    /* MPU6050 data */
    float roll;
    float pitch;
    float temperature;
    uint8_t mpu_valid;
    
    /* Encoder Left data */
    int32_t pos_left;
    int16_t speed_left;
    float rpm_left;
    int8_t dir_left;
    uint8_t enc_left_valid;
    
    /* Encoder Right data */
    int32_t pos_right;
    int16_t speed_right;
    float rpm_right;
    int8_t dir_right;
    uint8_t enc_right_valid;
} g_cached_data = {0};

/**
 * @brief  Initialize LCD display system
 * @param  None
 * @retval 0 on success, -1 on failure
 * @note   Creates two separate message queues for MPU and Encoder data
 */
int LCD_Display_Init(void)
{
    OS_ERR err;
    
    /* Create message queue for MPU6050 data */
    OSQCreate(&g_mpu_display_queue, 
              (CPU_CHAR *)"MPU Display Queue", 
              10,  /* Maximum 10 messages in queue */
              &err);
    
    if (err != OS_ERR_NONE) {
        return -1;
    }
    
    /* Create message queue for Encoder data */
    OSQCreate(&g_encoder_display_queue, 
              (CPU_CHAR *)"Encoder Display Queue", 
              10,  /* Maximum 10 messages in queue */
              &err);
    
    if (err != OS_ERR_NONE) {
        return -1;
    }
    
    /* Clear LCD screen (use LCD_Fill instead of LCD_Clear) */
    LCD_Fill(0, 0, 240, 320, WHITE);
    
    /* Draw static labels */
    LCD_ShowString(0, LCD_MPU_START_Y, (u8*)"=== MPU6050 ===", BLACK, WHITE, 16, 0);
    LCD_ShowString(0, LCD_ENC_START_Y, (u8*)"=== Encoder===", BLACK, WHITE, 16, 0);
    
    return 0;
}

/**
 * @brief  Send MPU6050 data to MPU display queue
 * @param  attitude: Pointer to attitude data
 * @param  temp: Temperature value
 * @retval 0 on success, -1 on failure
 * @note   Posts to g_mpu_display_queue (non-blocking)
 */
int LCD_Send_MPU_Data(MPU6050_Attitude_t *attitude, float temp)
{
    OS_ERR err;
    MPU_Display_Data_t *packet;
    
    /* Use static allocation to avoid dynamic memory */
    packet = &mpu_packet;
    
    packet->attitude = *attitude;
    packet->temperature = temp;
    
    /* Post to MPU queue (non-blocking) */
    OSQPost(&g_mpu_display_queue, 
            (void *)packet, 
            sizeof(MPU_Display_Data_t), 
            OS_OPT_POST_FIFO | OS_OPT_POST_NO_SCHED, 
            &err);
    
    return (err == OS_ERR_NONE) ? 0 : -1;
}

/**
 * @brief  Send Encoder data to Encoder display queue
 * @param  encoder_data: Pointer to encoder display data
 * @retval 0 on success, -1 on failure
 * @note   Posts to g_encoder_display_queue (non-blocking)
 */
int LCD_Send_Encoder_Data(Encoder_Display_Data_t *encoder_data)
{
    OS_ERR err;
    Encoder_Display_Data_t *packet;
    
    /* Use static allocation to avoid dynamic memory */
    packet = &enc_packet;
    
    *packet = *encoder_data;
    
    /* Post to Encoder queue (non-blocking) */
    OSQPost(&g_encoder_display_queue, 
            (void *)packet, 
            sizeof(Encoder_Display_Data_t), 
            OS_OPT_POST_FIFO | OS_OPT_POST_NO_SCHED, 
            &err);
    
    return (err == OS_ERR_NONE) ? 0 : -1;
}

/**
 * @brief  Update MPU6050 display section
 * @param  None
 * @retval None
 * @note   Updates only the data values, not the labels
 */
static void LCD_Update_MPU_Display(void)
{
    char buf[32];
    
    if (!g_cached_data.mpu_valid) {
        return;
    }
    
    /* Row 1: Roll */
    sprintf(buf, "Roll: %.2f  ", g_cached_data.roll);
    LCD_ShowString(0, LCD_MPU_START_Y + LCD_MPU_ROW_HEIGHT, (u8*)buf, BLACK, WHITE, 16, 0);
    
    /* Row 2: Pitch */
    sprintf(buf, "Pitch:%.2f  ", g_cached_data.pitch);
    LCD_ShowString(0, LCD_MPU_START_Y + LCD_MPU_ROW_HEIGHT * 2, (u8*)buf, BLACK, WHITE, 16, 0);
    
    /* Row 3: Temperature */
    sprintf(buf, "Temp: %.2f C", g_cached_data.temperature);
    LCD_ShowString(0, LCD_MPU_START_Y + LCD_MPU_ROW_HEIGHT * 3, (u8*)buf, BLACK, WHITE, 16, 0);
}

/**
 * @brief  Update Encoder Left display section
 * @param  None
 * @retval None
 */
static void LCD_Update_Encoder_Left_Display(void)
{
    char buf[32];
    
    if (!g_cached_data.enc_left_valid) {
        return;
    }
    
    /* Row 1: Position */
    sprintf(buf, "Pos:%d  ", g_cached_data.pos_left);
    LCD_ShowString(0, LCD_ENC_START_Y + LCD_ENC_ROW_HEIGHT, (u8*)buf, BLACK, WHITE, 12, 0);
    
    /* Row 2: Speed */
    sprintf(buf, "Spd:%d   ", g_cached_data.speed_left);
    LCD_ShowString(0, LCD_ENC_START_Y + LCD_ENC_ROW_HEIGHT * 2, (u8*)buf, BLACK, WHITE, 12, 0);
    
    /* Row 3: RPM */
    sprintf(buf, "RPM:%.1f   ", g_cached_data.rpm_left);
    LCD_ShowString(0, LCD_ENC_START_Y + LCD_ENC_ROW_HEIGHT * 3, (u8*)buf, BLACK, WHITE, 12, 0);
}

/**
 * @brief  Update Encoder Right display section
 * @param  None
 * @retval None
 */
static void LCD_Update_Encoder_Right_Display(void)
{
    char buf[32];
    
    if (!g_cached_data.enc_right_valid) {
        return;
    }
    
    /* Row 1: Position */
    sprintf(buf, "Pos:%d  ", g_cached_data.pos_right);
    LCD_ShowString(60, LCD_ENC_START_Y + LCD_ENC_ROW_HEIGHT, (u8*)buf, BLACK, WHITE, 12, 0);
    
    /* Row 2: Speed */
    sprintf(buf, "Spd:%d   ", g_cached_data.speed_right);
    LCD_ShowString(60, LCD_ENC_START_Y + LCD_ENC_ROW_HEIGHT * 2, (u8*)buf, BLACK, WHITE, 12, 0);
    
    /* Row 3: RPM */
    sprintf(buf, "RPM:%.1f   ", g_cached_data.rpm_right);
    LCD_ShowString(60, LCD_ENC_START_Y + LCD_ENC_ROW_HEIGHT * 3, (u8*)buf, BLACK, WHITE, 12, 0);
}

/**
 * @brief  Process MPU display data packet
 * @param  packet: Pointer to MPU display data packet
 * @retval None
 */
static void LCD_Process_MPU_Data(MPU_Display_Data_t *packet)
{
    if (packet == NULL) {
        return;
    }
    
    /* Cache MPU6050 data */
    g_cached_data.roll = packet->attitude.roll;
    g_cached_data.pitch = packet->attitude.pitch;
    g_cached_data.temperature = packet->temperature;
    g_cached_data.mpu_valid = 1;
}

/**
 * @brief  Process Encoder display data packet
 * @param  packet: Pointer to Encoder display data packet
 * @retval None
 */
static void LCD_Process_Encoder_Data(Encoder_Display_Data_t *packet)
{
    if (packet == NULL) {
        return;
    }
    
    /* Cache Encoder Left data */
    g_cached_data.pos_left = packet->position_left;
    g_cached_data.speed_left = packet->speed_left;
    g_cached_data.rpm_left = packet->rpm_left;
    g_cached_data.dir_left = packet->direction_left;
    g_cached_data.enc_left_valid = 1;
    
    /* Cache Encoder Right data */
    g_cached_data.pos_right = packet->position_right;
    g_cached_data.speed_right = packet->speed_right;
    g_cached_data.rpm_right = packet->rpm_right;
    g_cached_data.dir_right = packet->direction_right;
    g_cached_data.enc_right_valid = 1;
}

/**
 * @brief  Process all pending MPU queue messages
 * @param  None
 * @retval None
 */
static void LCD_Process_MPU_Queue(void)
{
    OS_ERR err;
    OS_MSG_SIZE msg_size;
    MPU_Display_Data_t *packet;
    CPU_TS ts;
    
    /* Process all pending MPU messages (non-blocking) */
    do {
        packet = (MPU_Display_Data_t *)OSQPend(
            &g_mpu_display_queue,
            0,                              /* No timeout (poll) */
            OS_OPT_PEND_NON_BLOCKING,
            &msg_size,
            &ts,
            &err
        );
        
        if (err == OS_ERR_NONE && packet != NULL) {
            LCD_Process_MPU_Data(packet);
        }
    } while (err == OS_ERR_NONE);
}

/**
 * @brief  Process all pending Encoder queue messages
 * @param  None
 * @retval None
 */
static void LCD_Process_Encoder_Queue(void)
{
    OS_ERR err;
    OS_MSG_SIZE msg_size;
    Encoder_Display_Data_t *packet;
    CPU_TS ts;
    
    /* Process all pending Encoder messages (non-blocking) */
    do {
        packet = (Encoder_Display_Data_t *)OSQPend(
            &g_encoder_display_queue,
            0,                              /* No timeout (poll) */
            OS_OPT_PEND_NON_BLOCKING,
            &msg_size,
            &ts,
            &err
        );
        
        if (err == OS_ERR_NONE && packet != NULL) {
            LCD_Process_Encoder_Data(packet);
        }
    } while (err == OS_ERR_NONE);
}

/**
 * @brief  LCD display task
 * @param  p_arg: Task argument (unused)
 * @retval None
 * @note   Receives data from two separate queues and updates display
 *         Runs at 20Hz refresh rate
 */
void Task_LCD_Display(void *p_arg)
{
    OS_ERR err;
    
    (void)p_arg;
    
    /* Wait for system initialization */
    OSTimeDly(300, OS_OPT_TIME_DLY, &err);
    
    /* Initialize LCD display system */
    if (LCD_Display_Init() != 0) {
        printf("LCD Display Init Failed!\r\n");
        OSTaskDel(NULL, &err);
        return;
    }
    
    printf("LCD Display Task Started - 20Hz refresh (Dual Queue Mode)\r\n");
    
    while (1) {
        /* Process MPU queue messages */
        LCD_Process_MPU_Queue();
        
        /* Process Encoder queue messages */
        LCD_Process_Encoder_Queue();
        
        /* Update display with cached data */
        LCD_Update_MPU_Display();
        LCD_Update_Encoder_Left_Display();
        //LCD_Update_Encoder_Right_Display();
        
        /* Fixed refresh period (20Hz) */
        OSTimeDly(LCD_REFRESH_PERIOD_MS, OS_OPT_TIME_DLY, &err);
    }
}

/* End of file */

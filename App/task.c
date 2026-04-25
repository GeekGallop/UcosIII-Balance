/**
 ******************************************************************************
 * @file    task.c
 * @brief   Task code with English comments
 * @note    Fixed Protocol_Task delay issue causing data loss
 * @author  User
 * @date    2026-02-13
 ******************************************************************************
 */

#include "task.h"
#include "os.h"
#include "./LED/led.h"
#include "./key/key.h"
#include "./usart/usart.h"
#include "./mpu6050/mpu6050.h"
#include "./lcd/lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "sys_monitor.h"
#include "./motor/motor.h"
#include "./mpu6050/task_mpu.h"
#include "./usart/task_usart.h"
#include "./encoder/encoder.h"
#include "./encoder/task_encoder.h"
#include "./lcd/task_lcd.h"
#include "./tim/tim.h"
#include "./pid/pid.h"
/* ========== Task Definitions ========== */

/* START TASK */
#define START_TASK_PRIO     2
#define START_STK_SIZE      512
OS_TCB      StartTask_TCB;
CPU_STK     StartTask_STK[START_STK_SIZE];
void start_task(void *p_arg);


/* LCD TASK - LCD Display Task */
#define LCD_TASK_PRIO       10  
#define LCD_TASK_STK_SIZE   2048
OS_TCB      LCD_Task_TCB;
CPU_STK     LCD_Task_STK[LCD_TASK_STK_SIZE];
void Task_LCD_Display(void *p_arg);

/* MPU6050 TASK - MPU6050 Display Task */
#define MPU6050_TASK_READ_PRIO       10  
#define MPU6050_TASK_READ_STK_SIZE   1024
OS_TCB      MPU6050_Task_READ_TCB;
CPU_STK     MPU6050_Task_READ_STK[MPU6050_TASK_READ_STK_SIZE];
void Task_MPU6050_Read(void *p_arg);

/* MPU6050 TASK - MPU6050 Display Task */
#define MPU6050_TASK_PRIO       8  
#define MPU6050_TASK_STK_SIZE   1024
OS_TCB      MPU6050_Task_TCB;
CPU_STK     MPU6050_Task_STK[MPU6050_TASK_STK_SIZE];
void Task_MPU6050_Display(void *p_arg);

/* LCD TASK - Display Task */


/* PROTOCOL TASK - UART Protocol Parsing Task */
#define PROTOCOL_PRIO       7  // High priority for fast UART data processing
#define PROTOCOL_STK_SIZE   2048
OS_TCB      Protocol_Task_TCB;
CPU_STK     Protocol_Task_STK[PROTOCOL_STK_SIZE];

#define ENCODER_TASK_PRIO       4  
#define ENCODER_TASK_STK_SIZE   2048
OS_TCB      Encoder_Task_TCB;
CPU_STK     Encoder_Task_STK[ENCODER_TASK_STK_SIZE];

#define PID_TASK_PRIO       5  
#define PID_TASK_STK_SIZE   2048
OS_TCB      PID_Task_TCB;
CPU_STK     PID_Task_STK[PID_TASK_STK_SIZE];
void Task_PID_Control(void *p_arg);

/* ========== Application Start Function ========== */

void app_start(void)
{
		OS_ERR err;
    /* Create Start Task */
    OSTaskCreate(&StartTask_TCB,
                 "start_task",
                 start_task,
                 NULL,
                 START_TASK_PRIO,
                 &StartTask_STK[0],
                 START_STK_SIZE / 10,
                 START_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err);
    
    /* Start task scheduler */
    OSStart(&err);
    
    for (;;)
    {
        /* Should never reach here */
    }
}

/* ========== START TASK ========== */

void start_task(void *p_arg)
{
    OS_ERR err;
    CPU_INT32U cnts;
    RCC_ClocksTypeDef rcc_clocks;
    
    (void)p_arg;
    
    /* Initialize CPU library */
    CPU_Init();
    
    /* Configure SysTick based on configured tick rate */
    RCC_GetClocksFreq(&rcc_clocks);
    cnts = ((CPU_INT32U)rcc_clocks.HCLK_Frequency) / OSCfg_TickRate_Hz;
    OS_CPU_SysTickInit(cnts);
    
    /* Enable time slice scheduling */
    OSSchedRoundRobinCfg(OS_TRUE, 10, &err);
    
    /* Create UART protocol parsing task (fixed version) */
    OSTaskCreate(&Protocol_Task_TCB, "Protocol_Task", Protocol_Task, NULL,
                 PROTOCOL_PRIO,
                 &Protocol_Task_STK[0],
                 PROTOCOL_STK_SIZE / 10,
                 PROTOCOL_STK_SIZE,
                 0, 0, NULL,  // No time slice
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR|OS_OPT_TASK_SAVE_FP,
                 &err);
    
    /* Create LCD display task */
    OSTaskCreate(&LCD_Task_TCB, "LCD_Task", Task_LCD_Display, NULL,
                 MPU6050_TASK_PRIO,  // Priority
                 &LCD_Task_STK[0],
                 LCD_TASK_STK_SIZE / 10,
                 LCD_TASK_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR|OS_OPT_TASK_SAVE_FP,
                 &err);
        /* Create MPU6050 display task */
    OSTaskCreate(&MPU6050_Task_TCB, "MPU6050_Task 0", Task_MPU6050_Read, NULL,
                 8,  // Priority
                 &MPU6050_Task_STK[0],
                 MPU6050_TASK_STK_SIZE / 10,
                 MPU6050_TASK_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR|OS_OPT_TASK_SAVE_FP,
                 &err);
								 
								 
	OSTaskCreate(&Encoder_Task_TCB,"Encoder",Task_Encoder_Speed,NULL,
                 ENCODER_TASK_PRIO,
                 &Encoder_Task_STK[0],
                 ENCODER_TASK_STK_SIZE / 10,
                 ENCODER_TASK_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP, 
                 &err);
		OSStatTaskCPUUsageInit(&err);

    OSTaskCreate(&PID_Task_TCB,"pid control",Task_PID_Control,NULL,
                 PID_TASK_PRIO,
                 &PID_Task_STK[0],
                 PID_TASK_STK_SIZE / 10,
                 PID_TASK_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR | OS_OPT_TASK_SAVE_FP, 
                 &err);
	OSStatTaskCPUUsageInit(&err);
//		TIM8_Int_Init();
		
    /* Delte startup task */
    OSTaskDel(NULL, &err);
}




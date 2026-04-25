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
/* ========== PROTOCOL TASK ========== */
/**
 * @brief  UART protocol parsing task (fixed version)
 * @note   Fix points:
 *         1. Remove delay at end of loop
 *         2. Fix buffer index error
 *         3. Add buffer clear and null terminator
 *         4. Optimize state machine logic
 */
 float p=0.0;
 float i=0.0;
 float d=0.0;

 int pos=0;
void Protocol_Task(void *p_arg)
{
    (void)p_arg;
    
    OS_ERR err;
    void *p_msg;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    
    /* State machine variables */
    uint8_t rx_state = 0;      // 0=Wait for packet header '[', 1=Receive data
    char rx_buffer[50];        // Receive buffer
    uint8_t rx_index = 0;      // Receive position
    
    /* Initialize buffer */
    memset(rx_buffer, 0, sizeof(rx_buffer));  
    
    while(1)
    {
        /* Block waiting for message (infinite wait) */
        p_msg = OSQPend(&USART_Rx_Queue, 
                        0,                      // Infinite wait
                        OS_OPT_PEND_BLOCKING, 
                        &msg_size, 
                        &ts, 
                        &err);
        
        if(err == OS_ERR_NONE)
        {
            /* Retrieve byte data from pointer */
            uint8_t rx_data = (uint8_t)(uintptr_t)p_msg;
            
            /* State machine processing */
            switch(rx_state)
            {
                case 0:  /* Wait for packet header '[' */
                {
                    if(rx_data == '[')
                    {
                        rx_state = 1;
                        rx_index = 0;
                        memset(rx_buffer, 0, sizeof(rx_buffer));  // Clear buffer
                        printf("Packet Start\r\n");
                    }
                    break;
                }
                
                case 1:  /* Receive data */
                {
                    if(rx_data == ']')
                    {
                        /* Packet end */
                        rx_state = 0;
                        rx_buffer[rx_index] = '\0';  // Add string terminator
                        
                        printf("Received Packet: [%s]\r\n", rx_buffer);
                        
                        /* Parse protocol */
                        char *str1 = strtok(rx_buffer, ",");
                        char *str2 = strtok(NULL, ",");
                        char *str3 = strtok(NULL, ",");
                        char *str4 = strtok(NULL, ",");
                        
                        if(str1 != NULL && strcmp(str1, "PID") == 0)
                        {
                            if(str2 && str3 && str4)
                            {
                                p = atof(str2);
                                i = atof(str3);
                                d = atof(str4);
                                printf("PID Parameters: P=%.3f, I=%.3f, D=%.3f\r\n", p, i, d);
                            }
                            else
                            {
                                printf("PID Parse Error: Missing parameters\r\n");
                            }
                        }
                        else if(str1 != NULL && strcmp(str1, "LED") == 0)
                        {
                            if(str2 && strcmp(str2, "ON") == 0)
                            {
                                LED0 = 0;
                                printf("LED ON\r\n");
                            }
                            else if(str2 && strcmp(str2, "OFF") == 0)
                            {
                                LED0 = 1;
                                printf("LED OFF\r\n");
                            }
                        }
						else if(str1 != NULL && strcmp(str1, "Speed") == 0)
                        {
							if(str2 && strcmp(str2, "M1") == 0)
							{
								int16_t speed=atoi(str3);
								Motor_SetSpeed(MOTOR_A,speed);
                            }
								
							else if(str2 && strcmp(str2, "M2") == 0)
							{
							    int16_t speed=atoi(str3);
							    Motor_SetSpeed(MOTOR_B,speed);
							}
														
                        }
												
						 else if(str1 != NULL && strcmp(str1, "Pos") == 0)
                {
											pos=atoi(str 2);
									printf("pos:%d",pos);
								}
                        else
                        {
                            printf("Unknown Command: %s\r\n", str1 ? str1 : "NULL");
                        }
                        
                        /* Reset index */
                        rx_index = 0;
                    }
                    else
                    {
                        /* Store in buffer */
                        if(rx_index < sizeof(rx_buffer) - 1)
                        {
                            rx_buffer[rx_index++] = rx_data;  // Fix: assign then increment
                        }
                        else
                        {
                            /* Buffer overflow, reset state */
                            printf("Buffer Overflow! Reset.\r\n");
                            rx_state = 0;
                            rx_index = 0;
                        }
                    }
                    break;
                }
                
                default:
                    rx_state = 0;
                    break;
            }
        }
        else
        {
            /* Queue receive error */
            printf("OSQPend Error: %d\r\n", err);
        }
    }
}

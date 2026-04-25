/**
 ******************************************************************************
 * @file    encoder.h
 * @brief   Rotary encoder driver for speed measurement using STM32F4 hardware encoder interface
 * @note    Uses TIM2 and TIM3 in encoder interface mode (4x quadrature decoding)
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Hardware Configuration:
 * -----------------------
 * Encoder Left (Motor A):
 *   - Timer: TIM2
 *   - Channel A: PA0 (TIM2_CH1)
 *   - Channel B: PA1 (TIM2_CH2)
 * 
 * Encoder Right (Motor B):
 *   - Timer: TIM3
 *   - Channel A: PA6 (TIM3_CH1)
 *   - Channel B: PA7 (TIM3_CH2)
 * 
 * Features:
 * ---------
 * - Hardware quadrature decoding (4x mode)
 * - Automatic direction detection
 * - 16-bit hardware counter with overflow handling
 * - 32-bit software position accumulator
 * - Digital noise filter
 * - Mutex protection for thread-safe access
 * 
 * Encoder Resolution:
 * -------------------
 * - Encoder PPR: 20 pulses per revolution
 * - Gear Ratio: 1:30 (motor to wheel)
 * - 4x mode: 4 counts per pulse
 * - Effective resolution: 20 * 4 * 30 = 2400 counts per wheel revolution
 * 
 * Thread Safety:
 * --------------
 * - All data access functions are protected by mutex
 * - Encoder_Update() is ISR-safe (non-blocking)
 * - Initialize mutex before using any encoder functions
 ******************************************************************************/

#ifndef __ENCODER_H
#define __ENCODER_H

#include "sys.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"
#include "os.h"

/* ==================== Encoder Selection ==================== */

#define ENCODER_LEFT        0   /* Left encoder - TIM2 */
#define ENCODER_RIGHT       1   /* Right encoder - TIM3 */
#define ENCODER_BOTH        2   /* Both encoders */

/* ==================== Hardware Configuration ==================== */

/* Left Encoder - TIM2 (PA0, PA1) */
#define ENC_L_TIM               TIM2
#define ENC_L_TIM_RCC           RCC_APB1Periph_TIM2
#define ENC_L_GPIO              GPIOA
#define ENC_L_GPIO_RCC          RCC_AHB1Periph_GPIOA
#define ENC_L_PIN_A             GPIO_Pin_0      /* TIM2_CH1 */
#define ENC_L_PIN_B             GPIO_Pin_1      /* TIM2_CH2 */
#define ENC_L_AF                GPIO_AF_TIM2
#define ENC_L_SOURCE_A          GPIO_PinSource0
#define ENC_L_SOURCE_B          GPIO_PinSource1

/* Right Encoder - TIM3 (PA6, PA7) */
#define ENC_R_TIM               TIM3
#define ENC_R_TIM_RCC           RCC_APB1Periph_TIM3
#define ENC_R_GPIO              GPIOA
#define ENC_R_GPIO_RCC          RCC_AHB1Periph_GPIOA
#define ENC_R_PIN_A             GPIO_Pin_6      /* TIM3_CH1 */
#define ENC_R_PIN_B             GPIO_Pin_7      /* TIM3_CH2 */
#define ENC_R_AF                GPIO_AF_TIM3
#define ENC_R_SOURCE_A          GPIO_PinSource6
#define ENC_R_SOURCE_B          GPIO_PinSource7

/* ==================== Encoder Parameters ==================== */

#define ENCODER_PPR             20          /* Pulses Per Revolution (encoder) */
#define ENCODER_GEAR_RATIO      30          /* Gear ratio: motor to wheel */
#define ENCODER_COUNTS_PER_REV  (ENCODER_PPR * 4 * ENCODER_GEAR_RATIO)
                                        /* 20 * 4 * 30 = 2400 counts/rev */
                                        /* 4x mode: count on both edges of both channels */

#define ENCODER_TIM_PERIOD      65535       /* Maximum 16-bit counter value */
#define ENCODER_MODE            TIM_EncoderMode_TI12  /* 4x quadrature mode */

/* ==================== Data Structures ==================== */

/**
 * @brief  Encoder data structure
 * @note   Stores position, speed, and direction information
 */
typedef struct {
    int32_t position;           /* Total position in encoder counts (accumulated, 32-bit) */
    int32_t last_position;      /* Position at last sample (for delta calculation) */
    int32_t delta_position;     /* Position change since last sample */
    int16_t speed;              /* Speed in counts per sample period */
    float rpm;                  /* Speed in RPM (Revolutions Per Minute) */
    float velocity;             /* Linear velocity in m/s or revolutions per second */
    int8_t direction;           /* Direction: 1=forward, -1=backward, 0=stopped */
    uint16_t last_counter;      /* Last timer counter value (for overflow handling) */
} Encoder_Data_t;

/* ==================== External Variables ==================== */

extern Encoder_Data_t g_encoder_left_data;
extern Encoder_Data_t g_encoder_right_data;

/* ==================== Function Declarations ==================== */

/* Initialization functions */
void encoder_config(void);
void Encoder_Left_Init(void);
void Encoder_Right_Init(void);

/* Data acquisition functions - thread safe with mutex protection */
int32_t Encoder_GetPosition(uint8_t encoder);
int32_t Encoder_GetDeltaPosition(uint8_t encoder);
int16_t Encoder_GetSpeed(uint8_t encoder);
float Encoder_GetRPM(uint8_t encoder);
float Encoder_GetVelocity(uint8_t encoder);
int8_t Encoder_GetDirection(uint8_t encoder);

/* Update functions - call periodically at fixed interval, ISR safe */
void Encoder_Update(uint8_t encoder);
void Encoder_UpdateAll(void);
void Encoder_Calculate_Speed(Encoder_Data_t *data, float sample_time_ms);

/* Control functions */
void Encoder_Reset(uint8_t encoder);
void Encoder_ResetAll(void);

/* Hardware access functions - no mutex protection needed */
uint16_t Encoder_GetCounter(uint8_t encoder);
void Encoder_SetCounter(uint8_t encoder, uint16_t value);

#endif /* __ENCODER_H */

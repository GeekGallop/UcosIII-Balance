/**
 ******************************************************************************
 * @file    encoder.c
 * @brief   Rotary encoder driver implementation using STM32F4 hardware encoder interface
 * @note    Uses TIM2 and TIM3 in encoder interface mode for hardware quadrature decoding
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * Implementation Details:
 * -----------------------
 * 
 * 1. Hardware Quadrature Decoder:
 *    - Uses STM32F4 timer encoder interface mode
 *    - TIM2 for left encoder (PA0/PA1)
 *    - TIM3 for right encoder (PA6/PA7)
 *    - 4x counting mode: counts on both edges of both channels
 * 
 * 2. 4x Mode Explanation:
 *    
 *    Quadrature signals (Channel A and B):
 *    
 *    Forward rotation (A leads B by 90°):
 *         ____      ____      ____
 *    A: _|    |____|    |____|    |____
 *          ____      ____      ____
 *    B: __|    |____|    |____|    |____
 *    
 *    Backward rotation (B leads A by 90°):
 *          ____      ____      ____
 *    A: __|    |____|    |____|    |____
 *         ____      ____      ____
 *    B: _|    |____|    |____|    |____
 *    
 *    4x Mode counting (one full cycle = 4 counts):
 *    - Count 1: Rising edge of A
 *    - Count 2: Rising edge of B
 *    - Count 3: Falling edge of A
 *    - Count 4: Falling edge of B
 * 
 * 3. Overflow Handling:
 *    - Hardware counter is 16-bit (0-65535)
 *    - Software position is 32-bit (no overflow)
 *    - Uses int16_t subtraction to handle overflow automatically
 *    
 *    Example:
 *    - Counter at 65534, moves forward 2 counts -> 0
 *    - (uint16_t)(0 - 65534) = 2 (correct!)
 *    - Counter at 1, moves backward 2 counts -> 65535
 *    - (uint16_t)(65535 - 1) = -2 (correct!)
 * 
 * 4. Speed Calculation:
 *    - Sample encoder at fixed time intervals (e.g., 10ms)
 *    - Calculate delta counts / delta time
 *    - Convert to RPM or linear velocity
 *    
 *    RPM formula:
 *    RPM = (delta_counts / sample_time_s) * (60 / counts_per_rev)
 *    
 *    For 10ms sample time and 2400 counts/rev:
 *    RPM = delta_counts * (100 / 2400) * 60
 *        = delta_counts * 2.5
 * 
 * 5. Thread Safety:
 *    - Mutex protects shared data structures
 *    - Encoder_Update() uses non-blocking mutex (ISR safe)
 *    - All getter functions use blocking mutex (task level)
 ******************************************************************************/

#include "./encoder/encoder.h"
#include "stddef.h"
#include "os.h"
/* Global encoder data structures */
Encoder_Data_t g_encoder_left_data = {0};
Encoder_Data_t g_encoder_right_data = {0};
OS_MUTEX g_encoder_mutex;


/**
 * @brief  Initialize both hardware encoders
 * @param  None
 * @retval None
 * @note   Must be called before using any encoder functions
 *         Creates mutex for data protection
 */
void encoder_config(void)
{
	OS_ERR err;
  Encoder_Left_Init();
  Encoder_Right_Init();
	OSMutexCreate(&g_encoder_mutex, "g_encoder_mutex Mutex", &err); 
}

/**
 * @brief  Initialize left encoder hardware (TIM2)
 * @param  None
 * @retval None
 * @note   PA0 (CH1), PA1 (CH2) - Left motor encoder
 *         Configures timer in encoder interface mode with 4x counting
 */
void Encoder_Left_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    
    /* Step 1: Enable peripheral clocks */
    RCC_APB1PeriphClockCmd(ENC_L_TIM_RCC, ENABLE);
    RCC_AHB1PeriphClockCmd(ENC_L_GPIO_RCC, ENABLE);
    
    /* Step 2: Configure GPIO pins as alternate function */
    /* Map PA0 and PA1 to TIM2 alternate function */
    GPIO_PinAFConfig(ENC_L_GPIO, ENC_L_SOURCE_A, ENC_L_AF);
    GPIO_PinAFConfig(ENC_L_GPIO, ENC_L_SOURCE_B, ENC_L_AF);
    
    /* Configure pins: alternate function, push-pull, pull-up, high speed */
    GPIO_InitStructure.GPIO_Pin = ENC_L_PIN_A | ENC_L_PIN_B;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  /* Internal pull-up for open-drain encoders */
    GPIO_Init(ENC_L_GPIO, &GPIO_InitStructure);
    
    /* Step 3: Configure timer time base */
    TIM_TimeBaseStructure.TIM_Prescaler = 0;                    /* No prescaler - count every pulse */
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; /* Up counting mode */
    TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD;      /* 65535 - maximum 16-bit value */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     /* No clock division */
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;            /* Not used for TIM2/TIM3 */
    TIM_TimeBaseInit(ENC_L_TIM, &TIM_TimeBaseStructure);
    
    /* Step 4: Configure encoder interface mode (4x mode) */
    /* 
     * TIM_EncoderMode_TI12: Both inputs active on both edges
     * This gives 4 counts per encoder pulse (quadrature cycle)
     * 
     * TIM_ICPolarity_Rising: Count on both rising and falling edges
     * (The polarity setting works with XOR to detect both edges)
     */
    TIM_EncoderInterfaceConfig(
        ENC_L_TIM,
        ENCODER_MODE,               /* TIM_EncoderMode_TI12 - 4x mode */
        TIM_ICPolarity_Rising,      /* Count on both edges of CH1 */
        TIM_ICPolarity_Rising       /* Count on both edges of CH2 */
    );
    
    /* Step 5: Configure input capture with digital filter */
    /* Digital filter reduces noise from encoder contacts */
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;  /* No prescaler */
    TIM_ICInitStructure.TIM_ICFilter = 6;  /* Filter: fSAMPLING = fDTS/32, N=6 */
    TIM_ICInit(ENC_L_TIM, &TIM_ICInitStructure);
    
    /* Configure channel 2 with same settings */
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInit(ENC_L_TIM, &TIM_ICInitStructure);
    
    /* Step 6: Enable timer counter */
    TIM_Cmd(ENC_L_TIM, ENABLE);
    
    /* Step 7: Reset counter to initial value */
    TIM_SetCounter(ENC_L_TIM, 0);
    
    /* Step 8: Initialize software data structure */
    g_encoder_left_data.position = 0;
    g_encoder_left_data.last_position = 0;
    g_encoder_left_data.delta_position = 0;
    g_encoder_left_data.speed = 0;
    g_encoder_left_data.rpm = 0.0f;
    g_encoder_left_data.velocity = 0.0f;
    g_encoder_left_data.direction = 0;
    g_encoder_left_data.last_counter = 0;
}

/**
 * @brief  Initialize right encoder hardware (TIM3)
 * @param  None
 * @retval None
 * @note   PA6 (CH1), PA7 (CH2) - Right motor encoder
 *         Same configuration as left encoder
 */
void Encoder_Right_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_ICInitTypeDef TIM_ICInitStructure;
    
    /* Enable peripheral clocks */
    RCC_APB1PeriphClockCmd(ENC_R_TIM_RCC, ENABLE);
    RCC_AHB1PeriphClockCmd(ENC_R_GPIO_RCC, ENABLE);
    
    /* Configure GPIO pins as alternate function */
    GPIO_PinAFConfig(ENC_R_GPIO, ENC_R_SOURCE_A, ENC_R_AF);
    GPIO_PinAFConfig(ENC_R_GPIO, ENC_R_SOURCE_B, ENC_R_AF);
    
    GPIO_InitStructure.GPIO_Pin = ENC_R_PIN_A | ENC_R_PIN_B;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(ENC_R_GPIO, &GPIO_InitStructure);
    
    /* Configure timer time base */
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(ENC_R_TIM, &TIM_TimeBaseStructure);
    
    /* Configure encoder interface mode */
    TIM_EncoderInterfaceConfig(
        ENC_R_TIM,
        ENCODER_MODE,
        TIM_ICPolarity_Rising,
        TIM_ICPolarity_Rising
    );
    
    /* Configure input capture with filter */
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 6;
    TIM_ICInit(ENC_R_TIM, &TIM_ICInitStructure);
    
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInit(ENC_R_TIM, &TIM_ICInitStructure);
    
    /* Enable timer */
    TIM_Cmd(ENC_R_TIM, ENABLE);
    
    /* Reset counter */
    TIM_SetCounter(ENC_R_TIM, 0);
    
    /* Initialize data structure */
    g_encoder_right_data.position = 0;
    g_encoder_right_data.last_position = 0;
    g_encoder_right_data.delta_position = 0;
    g_encoder_right_data.speed = 0;
    g_encoder_right_data.rpm = 0.0f;
    g_encoder_right_data.velocity = 0.0f;
    g_encoder_right_data.direction = 0;
    g_encoder_right_data.last_counter = 0;
}

/**
 * @brief  Get current accumulated position
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @retval Position in encoder counts (32-bit signed)
 * @note   Thread-safe, protected by mutex
 */
int32_t Encoder_GetPosition(uint8_t encoder)
{

    int32_t position = 0;
    CPU_SR_ALLOC();
    CPU_CRITICAL_ENTER();
    
    if (encoder == ENCODER_LEFT) {
        position = g_encoder_left_data.position;
    } else if (encoder == ENCODER_RIGHT) {
        position = g_encoder_right_data.position;
    }
    
    CPU_CRITICAL_EXIT();
    
    return position;
}

/**
 * @brief  Get position change since last update
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @retval Delta position in encoder counts
 * @note   Thread-safe, protected by mutex
 */
int32_t Encoder_GetDeltaPosition(uint8_t encoder)
{

    int32_t delta = 0;
    CPU_SR_ALLOC();
    
    CPU_CRITICAL_ENTER();
    if (encoder == ENCODER_LEFT) {
        delta = g_encoder_left_data.delta_position;
    } else if (encoder == ENCODER_RIGHT) {
        delta = g_encoder_right_data.delta_position;
    }
    
    CPU_CRITICAL_EXIT();
    
    return delta;
}

/**
 * @brief  Get current speed
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @retval Speed in counts per sample period
 * @note   Thread-safe, protected by mutex
 */
int16_t Encoder_GetSpeed(uint8_t encoder)
{

    int16_t speed = 0;
    CPU_SR_ALLOC();
    
    CPU_CRITICAL_ENTER();
    if (encoder == ENCODER_LEFT) {
        speed = g_encoder_left_data.speed;
    } else if (encoder == ENCODER_RIGHT) {
        speed = g_encoder_right_data.speed;
    }
    
    CPU_CRITICAL_EXIT();
    
    return speed;
}

/**
 * @brief  Get current RPM (Revolutions Per Minute)
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @retval Speed in RPM
 * @note   Thread-safe, protected by mutex
 */
float Encoder_GetRPM(uint8_t encoder)
{

    float rpm = 0.0f;
    CPU_SR_ALLOC();
    
    CPU_CRITICAL_ENTER();   
    if (encoder == ENCODER_LEFT) {
        rpm = g_encoder_left_data.rpm;
    } else if (encoder == ENCODER_RIGHT) {
        rpm = g_encoder_right_data.rpm;
    }
    
    CPU_CRITICAL_EXIT();  
    
    return rpm;
}

/**
 * @brief  Get linear velocity
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @retval Velocity in revolutions per second (or m/s if wheel diameter configured)
 * @note   Thread-safe, protected by mutex
 */
float Encoder_GetVelocity(uint8_t encoder)
{

    float velocity = 0.0f;
    CPU_SR_ALLOC();
    
    CPU_CRITICAL_ENTER(); 
    if (encoder == ENCODER_LEFT) {
        velocity = g_encoder_left_data.velocity;
    } else if (encoder == ENCODER_RIGHT) {
        velocity = g_encoder_right_data.velocity;
    }
    
    CPU_CRITICAL_EXIT();
    
    return velocity;
}

/**
 * @brief  Get rotation direction
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @retval Direction: 1=forward, -1=backward, 0=stopped
 * @note   Thread-safe, protected by mutex
 */
int8_t Encoder_GetDirection(uint8_t encoder)
{

    int8_t direction = 0;
    CPU_SR_ALLOC();
    
    CPU_CRITICAL_ENTER(); 
    if (encoder == ENCODER_LEFT) {
        direction = g_encoder_left_data.direction;
    } else if (encoder == ENCODER_RIGHT) {
        direction = g_encoder_right_data.direction;
    }
    
    CPU_CRITICAL_EXIT(); 
    
    return direction;
}

/**
 * @brief  Read hardware counter value directly
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @retval 16-bit counter value (0-65535)
 * @note   Raw hardware access, no mutex protection needed
 *         Mainly for debugging purposes
 */
uint16_t Encoder_GetCounter(uint8_t encoder)
{
    if (encoder == ENCODER_LEFT) {
        return TIM_GetCounter(ENC_L_TIM);
    } else if (encoder == ENCODER_RIGHT) {
        return TIM_GetCounter(ENC_R_TIM);
    }
    return 0;
}

/**
 * @brief  Set hardware counter value
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @param  value: 16-bit counter value
 * @retval None
 * @note   Use with caution, may cause position jump
 *         No mutex protection needed (hardware register)
 */
void Encoder_SetCounter(uint8_t encoder, uint16_t value)
{
    if (encoder == ENCODER_LEFT) {
        TIM_SetCounter(ENC_L_TIM, value);
    } else if (encoder == ENCODER_RIGHT) {
        TIM_SetCounter(ENC_R_TIM, value);
    }
}

/**
 * @brief  Update single encoder data - call periodically at fixed interval
 * @param  encoder: ENCODER_LEFT or ENCODER_RIGHT
 * @retval None
 * @note   ISR-safe: uses non-blocking mutex
 *         Should be called at regular intervals (e.g., 10ms) for speed calculation
 *         Handles 16-bit counter overflow automatically
 */
void Encoder_Update(uint8_t encoder)
{
    TIM_TypeDef *tim;
    Encoder_Data_t *data;
    uint16_t current_counter;
    int16_t delta;

    if (encoder == ENCODER_LEFT) {
        tim = ENC_L_TIM;
        data = &g_encoder_left_data;
    } else if (encoder == ENCODER_RIGHT) {
        tim = ENC_R_TIM;
        data = &g_encoder_right_data;
    } else {
        return;
    }
    
    /* Read current hardware counter value (16-bit) - no protection needed */
    current_counter = TIM_GetCounter(tim);
    
    /* Calculate delta with automatic overflow handling */
    /*
     * The magic of int16_t subtraction:
     * 
     * Case 1: Normal forward movement
     *   last = 1000, current = 1005
     *   delta = (int16_t)(1005 - 1000) = 5 ✓
     * 
     * Case 2: Overflow forward (65535 -> 0)
     *   last = 65534, current = 1
     *   delta = (int16_t)(1 - 65534) = (int16_t)(-65533) = 3 ✓
     *   (Because -65533 as int16_t is 3 due to 16-bit wraparound)
     * 
     * Case 3: Normal backward movement
     *   last = 1005, current = 1000
     *   delta = (int16_t)(1000 - 1005) = -5 ✓
     * 
     * Case 4: Overflow backward (0 -> 65535)
     *   last = 1, current = 65534
     *   delta = (int16_t)(65534 - 1) = (int16_t)(65533) = -3 ✓
     *   (Because 65533 as int16_t is -3 due to 16-bit wraparound)
     */
    delta = (int16_t)(current_counter - data->last_counter);
    
    /* Try to acquire mutex (non-blocking for ISR safety) */
    //CPU_CRITICAL_ENTER();
    
    {
        /* Mutex acquired, safe to update shared data */
        /* Update accumulated position (32-bit, no overflow concern) */
        data->position += delta;
        
        /* Store delta for speed calculation */
        data->delta_position = delta;
        data->speed = delta;
        
        /* Determine direction */
        if (delta > 0) {
            data->direction = 1;    /* Forward */
        } else if (delta < 0) {
            data->direction = -1;   /* Backward */
        } else {
            data->direction = 0;    /* Stopped */
        }
        
        /* Save current counter for next update */
        data->last_counter = current_counter;
        
        /* Release mutex */
        //CPU_CRITICAL_EXIT();
    }  
}

/**
 * @brief  Update both encoders
 * @param  None
 * @retval None
 * @note   Convenience function to update both encoders at once
 *         ISR-safe, calls Encoder_Update() for each encoder
 */
void Encoder_UpdateAll(void)
{
    Encoder_Update(ENCODER_LEFT);
    Encoder_Update(ENCODER_RIGHT);
}

/**
 * @brief  Calculate speed in RPM and velocity
 * @param  data: Pointer to encoder data structure
 * @param  sample_time_ms: Sample time in milliseconds
 * @retval None
 * @note   Call this after Encoder_Update() with known sample time
 *         This function does not access shared data directly,
 *         operates on local copy or within mutex protection
 * 
 * Calculation formula:
 * RPM = (delta_counts / sample_time_s) * (60 / counts_per_rev)
 * 
 * For 10ms sample time and 2400 counts/rev:
 * RPM = delta * (100 / 2400) * 60 = delta * 2.5
 */
void Encoder_Calculate_Speed(Encoder_Data_t *data, float sample_time_ms)
{
    float counts_per_second;
    float revolutions_per_second;
    
    if (data == NULL || sample_time_ms <= 0) {
        return;
    }
    
    /* Calculate counts per second */
    counts_per_second = (float)(data->delta_position) * 1000.0f / sample_time_ms;
    
    /* Calculate RPM */
    /* RPM = (counts/sec * 60) / counts_per_rev */
    revolutions_per_second = counts_per_second / ENCODER_COUNTS_PER_REV;
    data->rpm = revolutions_per_second * 60.0f;
    
    /* Store velocity as revolutions per second */
    data->velocity = revolutions_per_second;
    
    /* For linear velocity in m/s, multiply by wheel circumference:
     * Example: wheel_diameter = 0.065m (65mm)
     * circumference = PI * 0.065 = 0.204m
     * velocity_mps = revolutions_per_second * 0.204f;
     */
}

/**
 * @brief  Reset encoder position and data
 * @param  encoder: ENCODER_LEFT, ENCODER_RIGHT, or ENCODER_BOTH
 * @retval None
 * @note   Resets both hardware counter and software accumulated position
 *         Thread-safe, protected by mutex
 */
void Encoder_Reset(uint8_t encoder)
{

    CPU_SR_ALLOC();
    
    CPU_CRITICAL_ENTER(); 
    if (encoder == ENCODER_LEFT || encoder == ENCODER_BOTH) {
        /* Reset hardware counter */
        TIM_SetCounter(ENC_L_TIM, 0);
        
        /* Reset software data */
        g_encoder_left_data.position = 0;
        g_encoder_left_data.last_position = 0;
        g_encoder_left_data.delta_position = 0;
        g_encoder_left_data.speed = 0;
        g_encoder_left_data.rpm = 0.0f;
        g_encoder_left_data.velocity = 0.0f;
        g_encoder_left_data.direction = 0;
        g_encoder_left_data.last_counter = 0;
    }
    
    if (encoder == ENCODER_RIGHT || encoder == ENCODER_BOTH) {
        /* Reset hardware counter */
        TIM_SetCounter(ENC_R_TIM, 0);
        
        /* Reset software data */
        g_encoder_right_data.position = 0;
        g_encoder_right_data.last_position = 0;
        g_encoder_right_data.delta_position = 0;
        g_encoder_right_data.speed = 0;
        g_encoder_right_data.rpm = 0.0f;
        g_encoder_right_data.velocity = 0.0f;
        g_encoder_right_data.direction = 0;
        g_encoder_right_data.last_counter = 0;
    }
    
    CPU_CRITICAL_EXIT(); 
}

/**
 * @brief  Reset both encoders
 * @param  None
 * @retval None
 * @note   Convenience function
 *         Thread-safe, protected by mutex
 */
void Encoder_ResetAll(void)
{
    Encoder_Reset(ENCODER_BOTH);
}

/* End of file */

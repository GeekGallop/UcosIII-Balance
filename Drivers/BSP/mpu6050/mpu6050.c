#include "os.h"
#include "./mpu6050/mpu6050.h"
#include "./iic/iic.h"
#include "usart.h"
extern MPU6050_Data_t g_mpu6050_data;   // Protected by mutex

extern OS_MUTEX MPU6050_Mutex;                 // Protect MPU6050 data
OS_Q     MPU6050_Data_Queue;            // Data queue for inter-task communication
OS_SEM   MPU6050_Data_Ready_Sem;        // Signal new data available



/**
 * @brief  Write single byte to MPU6050 register
 * @param  reg: Register address
 * @param  data: Data to write
 * @retval 0=Success, 1=Failed
 */
static uint8_t MPU6050_Write_Reg(uint8_t reg, uint8_t data)
{
    return IIC_Write_Byte(MPU6050_ADDR, reg, data);
}

/**
 * @brief  Read single byte from MPU6050 register
 * @param  reg: Register address
 * @param  data: Pointer to store data
 * @retval 0=Success, 1=Failed
 */
uint8_t MPU6050_Read_Reg(uint8_t reg, uint8_t *data)
{
	return IIC_Read_Byte(MPU6050_ADDR,reg,data);
}


/**
 * @brief  Read multiple bytes from MPU6050
 * @param  reg: Start register address
 * @param  buf: Buffer to store data
 * @param  len: Number of bytes to read
 * @retval 0=Success, 1=Failed
 */
static uint8_t MPU6050_Read_Bytes(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return IIC_Read_Bytes(MPU6050_ADDR, reg, buf, len);
}

/* ==================== MPU6050初始化 ==================== */

/**
 * @brief  Initialize MPU6050 (RTOS version)
 * @note   Must be called in a task, not in main() or interrupt
 * @retval 0=Success, 1=Failed
 */
uint8_t MPU6050_Init(void)
{
    OS_ERR err;
    uint8_t who_am_i;
    IIC_Init();
		u8 config;
    /* Create mutex for data protection */
    OSMutexCreate(&MPU6050_Mutex, "MPU6050 Mutex", &err);
    if(err != OS_ERR_NONE)
        return 1;
    
    /* Create data queue (10 messages, each is a pointer to MPU6050_Data_t) */
    OSQCreate(&MPU6050_Data_Queue, "MPU6050 Queue", 10, &err);
    if(err != OS_ERR_NONE)
        return 1;
    
    /* Create semaphore for data ready signal */
    OSSemCreate(&MPU6050_Data_Ready_Sem, "MPU6050 Data Ready", 0, &err);
    if(err != OS_ERR_NONE)
        return 1;
    
		MPU6050_Write_Reg(MPU6050_CONFIG,0x12);
		
		
    /* Check device ID */
    if(MPU6050_Read_Reg(MPU6050_WHO_AM_I, &who_am_i) != 0)
        return 1;
    
    if(who_am_i != 0x70)
    {
        //printf("MPU6050 not found! WHO_AM_I = 0x%02X\r\n", who_am_i);
        return 1;
    }
    
    /* Reset device */
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_1, 0x80);
    OSTimeDly(200, OS_OPT_TIME_DLY, &err);  // Wait for reset
    
    /* Wake up device */
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_1, 0x00);
    OSTimeDly(10, OS_OPT_TIME_DLY, &err);
		
		
    
    /* Configure sample rate: 1kHz / (1 + 9) = 100Hz */
    MPU6050_Write_Reg(MPU6050_SMPLRT_DIV, 9);
    
    /* Configure low-pass filter: 94Hz */
    MPU6050_Write_Reg(MPU6050_CONFIG, 0x02);
    
		MPU6050_Read_Reg(MPU6050_CONFIG, &config);
		printf("MPU6050 config register:%d\r",config);
		MPU6050_Read_Reg(MPU6050_PWR_MGMT_1, &config);
		printf("MPU6050 config register:%d\r\n",config);
		
    /* Configure gyroscope: ±2000°/s */
    MPU6050_Write_Reg(MPU6050_GYRO_CONFIG, 0x18);
    
    /* Configure accelerometer: ±2g */
    MPU6050_Write_Reg(MPU6050_ACCEL_CONFIG, 0x00);
		
		MPU6050_Write_Reg(MPU_INT_EN_REG,0X00);	
    MPU6050_Write_Reg(MPU_USER_CTRL_REG,0X00);	
    MPU6050_Write_Reg(MPU_FIFO_EN_REG,0X00);	
    MPU6050_Write_Reg(MPU_INTBP_CFG_REG,0X80);	
    MPU6050_Write_Reg(MPU_PWR_MGMT1_REG,0X01);	
    MPU6050_Write_Reg(MPU_PWR_MGMT2_REG,0X00);	
    
    printf("MPU6050 initialized successfully\r\n");
    
    return 0;
}

/* ==================== 数据读取函数 ==================== */

/**
 * @brief  Read raw data from MPU6050
 * @param  raw: Pointer to store raw data
 * @retval 0=Success, 1=Failed
 */
uint8_t MPU6050_Read_Raw_Data(MPU6050_RawData_t *raw)
{
    uint8_t buf[14];
    
    /* Read 14 bytes starting from ACCEL_XOUT_H */
    if(MPU6050_Read_Bytes(MPU6050_ACCEL_XOUT_H, buf, 14) != 0)
        return 1;
    
    /* Parse data (big-endian) */
    raw->accel_x = (int16_t)((buf[0] << 8) | buf[1]);
    raw->accel_y = (int16_t)((buf[2] << 8) | buf[3]);
    raw->accel_z = (int16_t)((buf[4] << 8) | buf[5]);
    raw->temp    = (int16_t)((buf[6] << 8) | buf[7]);
    raw->gyro_x  = (int16_t)((buf[8] << 8) | buf[9]);
    raw->gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    raw->gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);
    
    return 0;
}

/**
 * @brief  Convert raw data to physical units
 * @param  raw: Raw data
 * @param  data: Pointer to store processed data
 */
void MPU6050_Process_Data(MPU6050_RawData_t *raw, MPU6050_Data_t *data)
{
    /* Accelerometer: ±2g, 16-bit ADC, LSB = 2g / 32768 = 0.000061g */
    data->accel_x = raw->accel_x / 16384.0f;
    data->accel_y = raw->accel_y / 16384.0f;
    data->accel_z = raw->accel_z / 16384.0f;
    
    /* Gyroscope: ±2000°/s, 16-bit ADC, LSB = 2000 / 32768 = 0.061°/s */
    data->gyro_x = raw->gyro_x / 16.4f;
    data->gyro_y = raw->gyro_y / 16.4f;
    data->gyro_z = raw->gyro_z / 16.4f;
    
    /* Temperature: (TEMP_OUT / 340) + 36.53 °C */
    data->temp = (raw->temp / 340.0f) + 36.53f;
}

/**
 * @brief  Get MPU6050 data (Thread-safe)
 * @param  data: Pointer to store data
 * @retval 0=Success, 1=Failed
 */
uint8_t MPU6050_Get_Data(MPU6050_Data_t *data)
{
    OS_ERR err;
    
    /* Acquire mutex */
    OSMutexPend(&MPU6050_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if(err != OS_ERR_NONE)
        return 1;
    
    /* Copy data */
    *data = g_mpu6050_data;
    
    /* Release mutex */
    OSMutexPost(&MPU6050_Mutex, OS_OPT_POST_NONE, &err);
    
    return 0;
}

/* ==================== 姿态解算（简化版） ==================== */

/**
 * @brief  Calculate attitude using accelerometer data
 * @param  data: MPU6050 data
 * @param  attitude: Pointer to store attitude
 * @note   This is a simplified version, only uses accelerometer
 *         For better results, use complementary filter or Kalman filter
 */
void MPU6050_Calculate_Attitude(MPU6050_Data_t *data, MPU6050_Attitude_t *attitude)
{
    /* Calculate roll and pitch from accelerometer */
    attitude->roll = atan2(data->accel_y, data->accel_z) * 57.3f;  // Convert to degrees
    attitude->pitch = atan2(-data->accel_x, sqrt(data->accel_y * data->accel_y + 
                                                   data->accel_z * data->accel_z)) * 57.3f;
    
    /* Yaw cannot be calculated from accelerometer alone */
    attitude->yaw = 0.0f;
}


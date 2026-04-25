/**
 ******************************************************************************
 * @file    mpu6050_rtos.c
 * @brief   MPU6050 driver for uC/OS-III with complete porting guide
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * MPU6050移植到RTOS的完整指南
 * ===========================
 * 
 * 一、MPU6050简介
 * --------------
 * MPU6050是6轴运动传感器（3轴加速度计 + 3轴陀螺仪）
 * - IIC地址：0x68（AD0=0）或 0x69（AD0=1）
 * - 寄存器：WHO_AM_I = 0x75，返回值应为0x68
 * - 数据寄存器：0x3B~0x48（14字节）
 * 
 * 二、移植难点与解决方案
 * ---------------------
 * 
 * 难点1：初始化时序
 * ----------------
 * 裸机：main()中直接调用MPU6050_Init()
 * RTOS：必须在任务中初始化，且要等待IIC初始化完成
 * 
 * 解决方案：
 * void Task_MPU6050_Init(void *p_arg)
 * {
 *     OS_ERR err;
 *     OSTimeDly(100, OS_OPT_TIME_DLY, &err);  // 等待IIC初始化
 *     MPU6050_Init();
 *     OSTaskDel(NULL, &err);  // 初始化完成后删除自己
 * }
 * 
 * 难点2：数据读取周期
 * ------------------
 * 裸机：while(1) { MPU6050_Read(); delay_ms(10); }
 * RTOS：使用OSTimeDly()替代delay_ms()
 * 
 * 解决方案：
 * while(1)
 * {
 *     MPU6050_Get_Data(&ax, &ay, &az, &gx, &gy, &gz);
 *     OSTimeDly(10, OS_OPT_TIME_DLY, &err);  // 10ms周期
 * }
 * 
 * 难点3：数据共享
 * --------------
 * 多个任务需要读取MPU6050数据时，如何安全共享？
 * 
 * 方案A：使用互斥锁保护全局变量
 * 方案B：使用消息队列传递数据（推荐）
 * 方案C：使用信号量通知数据更新
 * 
 * 难点4：DMP固件加载
 * -----------------
 * DMP固件加载需要较长时间（~100ms），会阻塞任务
 * 
 * 解决方案：
 * - 在专门的初始化任务中加载
 * - 加载过程中定期调用OSTimeDly(1)让出CPU
 * 
 * 三、推荐的任务架构
 * -----------------
 * 
 * 架构1：单任务模式（简单应用）
 * ---------------------------
 * Task_MPU6050:
 *   - 读取数据
 *   - 处理数据
 *   - 显示/发送数据
 * 
 * 架构2：双任务模式（推荐）
 * ------------------------
 * Task_MPU6050_Read:
 *   - 优先级：中（10）
 *   - 周期：10ms
 *   - 功能：读取原始数据，发送到队列
 * 
 * Task_Data_Process:
 *   - 优先级：低（15）
 *   - 触发：接收队列数据
 *   - 功能：姿态解算、滤波
 * 
 * 架构3：三任务模式（复杂应用）
 * ---------------------------
 * Task_MPU6050_Read: 读取数据
 * Task_Attitude_Calc: 姿态解算
 * Task_Display: 显示数据
 * 
 * 四、性能优化建议
 * ---------------
 * 1. 使用DMA读取IIC数据（硬件IIC）
 * 2. 数据处理使用定点运算代替浮点
 * 3. 合理设置任务优先级和周期
 * 4. 使用数据缓冲减少IIC访问次数
 * 
 ******************************************************************************
 */

#include "os.h"
#include "iic_rtos_guide.c"  // 使用前面定义的IIC驱动
#include <stdio.h>
#include <math.h>

/* ==================== MPU6050寄存器定义 ==================== */

#define MPU6050_ADDR            0x68    // MPU6050 IIC address (AD0=0)

/* Register addresses */
#define MPU6050_SMPLRT_DIV      0x19    // Sample rate divider
#define MPU6050_CONFIG          0x1A    // Configuration
#define MPU6050_GYRO_CONFIG     0x1B    // Gyroscope configuration
#define MPU6050_ACCEL_CONFIG    0x1C    // Accelerometer configuration
#define MPU6050_INT_ENABLE      0x38    // Interrupt enable
#define MPU6050_ACCEL_XOUT_H    0x3B    // Accelerometer X-axis high byte
#define MPU6050_GYRO_XOUT_H     0x43    // Gyroscope X-axis high byte
#define MPU6050_PWR_MGMT_1      0x6B    // Power management 1
#define MPU6050_WHO_AM_I        0x75    // Device ID

/* ==================== 数据结构定义 ==================== */

/**
 * @brief  MPU6050 raw data structure
 */
typedef struct
{
    int16_t accel_x;    // Accelerometer X-axis
    int16_t accel_y;    // Accelerometer Y-axis
    int16_t accel_z;    // Accelerometer Z-axis
    int16_t temp;       // Temperature
    int16_t gyro_x;     // Gyroscope X-axis
    int16_t gyro_y;     // Gyroscope Y-axis
    int16_t gyro_z;     // Gyroscope Z-axis
} MPU6050_RawData_t;

/**
 * @brief  MPU6050 processed data structure
 */
typedef struct
{
    float accel_x;      // Accelerometer X-axis (g)
    float accel_y;      // Accelerometer Y-axis (g)
    float accel_z;      // Accelerometer Z-axis (g)
    float temp;         // Temperature (°C)
    float gyro_x;       // Gyroscope X-axis (°/s)
    float gyro_y;       // Gyroscope Y-axis (°/s)
    float gyro_z;       // Gyroscope Z-axis (°/s)
} MPU6050_Data_t;

/**
 * @brief  Attitude data structure
 */
typedef struct
{
    float roll;         // Roll angle (°)
    float pitch;        // Pitch angle (°)
    float yaw;          // Yaw angle (°)
} MPU6050_Attitude_t;

/* ==================== RTOS同步对象 ==================== */

OS_MUTEX MPU6050_Mutex;                 // Protect MPU6050 data
OS_Q     MPU6050_Data_Queue;            // Data queue for inter-task communication
OS_SEM   MPU6050_Data_Ready_Sem;        // Signal new data available

/* ==================== 全局变量 ==================== */

static MPU6050_Data_t g_mpu6050_data;   // Protected by mutex

/* ==================== MPU6050基础驱动 ==================== */

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
static uint8_t MPU6050_Read_Reg(uint8_t reg, uint8_t *data)
{
    return IIC_Read_Byte(MPU6050_ADDR, reg, data);
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
    
    /* Check device ID */
    if(MPU6050_Read_Reg(MPU6050_WHO_AM_I, &who_am_i) != 0)
        return 1;
    
    if(who_am_i != 0x68)
    {
        printf("MPU6050 not found! WHO_AM_I = 0x%02X\r\n", who_am_i);
        return 1;
    }
    
    printf("MPU6050 found! WHO_AM_I = 0x%02X\r\n", who_am_i);
    
    /* Reset device */
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_1, 0x80);
    OSTimeDly(100, OS_OPT_TIME_DLY, &err);  // Wait for reset
    
    /* Wake up device */
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_1, 0x00);
    OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    
    /* Configure sample rate: 1kHz / (1 + 9) = 100Hz */
    MPU6050_Write_Reg(MPU6050_SMPLRT_DIV, 9);
    
    /* Configure low-pass filter: 94Hz */
    MPU6050_Write_Reg(MPU6050_CONFIG, 0x02);
    
    /* Configure gyroscope: ±2000°/s */
    MPU6050_Write_Reg(MPU6050_GYRO_CONFIG, 0x18);
    
    /* Configure accelerometer: ±2g */
    MPU6050_Write_Reg(MPU6050_ACCEL_CONFIG, 0x00);
    
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

/* ==================== RTOS任务示例 ==================== */

/**
 * @brief  MPU6050 data reading task
 * @note   Reads data periodically and updates global variable
 */
void Task_MPU6050_Read(void *p_arg)
{
    OS_ERR err;
    MPU6050_RawData_t raw;
    MPU6050_Data_t data;
    
    (void)p_arg;
    
    /* Wait for system initialization */
    OSTimeDly(200, OS_OPT_TIME_DLY, &err);
    
    /* Initialize MPU6050 */
    if(MPU6050_Init() != 0)
    {
        printf("MPU6050 initialization failed!\r\n");
        OSTaskDel(NULL, &err);  // Delete task if init failed
        return;
    }
    
    while(1)
    {
        /* Read raw data */
        if(MPU6050_Read_Raw_Data(&raw) == 0)
        {
            /* Process data */
            MPU6050_Process_Data(&raw, &data);
            
            /* Update global variable (protected by mutex) */
            OSMutexPend(&MPU6050_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
            g_mpu6050_data = data;
            OSMutexPost(&MPU6050_Mutex, OS_OPT_POST_NONE, &err);
            
            /* Signal data ready */
            OSSemPost(&MPU6050_Data_Ready_Sem, OS_OPT_POST_1, &err);
            
            /* Optional: Send to queue for other tasks */
            // OSQPost(&MPU6050_Data_Queue, &data, sizeof(data), 
            //         OS_OPT_POST_FIFO, &err);
        }
        
        /* Read every 10ms (100Hz) */
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/**
 * @brief  MPU6050 data display task
 * @note   Waits for new data and displays it
 */
void Task_MPU6050_Display(void *p_arg)
{
    OS_ERR err;
    MPU6050_Data_t data;
    MPU6050_Attitude_t attitude;
    
    (void)p_arg;
    
    while(1)
    {
        /* Wait for new data (with timeout) */
        OSSemPend(&MPU6050_Data_Ready_Sem, 1000, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        if(err == OS_ERR_NONE)
        {
            /* Get data */
            if(MPU6050_Get_Data(&data) == 0)
            {
                /* Calculate attitude */
                MPU6050_Calculate_Attitude(&data, &attitude);
                
                /* Display data */
                printf("Accel: X=%.2f Y=%.2f Z=%.2f g\r\n", 
                       data.accel_x, data.accel_y, data.accel_z);
                printf("Gyro:  X=%.2f Y=%.2f Z=%.2f °/s\r\n", 
                       data.gyro_x, data.gyro_y, data.gyro_z);
                printf("Temp:  %.2f °C\r\n", data.temp);
                printf("Attitude: Roll=%.2f Pitch=%.2f\r\n\r\n", 
                       attitude.roll, attitude.pitch);
            }
        }
        else
        {
            printf("MPU6050 data timeout!\r\n");
        }
    }
}

/**
 * @brief  Example: Integrate with LCD display task
 */
void Task_MPU6050_LCD_Display(void *p_arg)
{
    OS_ERR err;
    MPU6050_Data_t data;
    MPU6050_Attitude_t attitude;
    char buf[32];
    
    (void)p_arg;
    
    /* Wait for LCD initialization */
    OSTimeDly(300, OS_OPT_TIME_DLY, &err);
    
    while(1)
    {
        /* Wait for new data */
        OSSemPend(&MPU6050_Data_Ready_Sem, 1000, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        if(err == OS_ERR_NONE)
        {
            /* Get data */
            if(MPU6050_Get_Data(&data) == 0)
            {
                /* Calculate attitude */
                MPU6050_Calculate_Attitude(&data, &attitude);
                
                /* Display on LCD */
                sprintf(buf, "Roll: %.1f", attitude.roll);
                LCD_ShowString(0, 0, (u8*)buf, BLACK, WHITE, 16, 0);
                
                sprintf(buf, "Pitch:%.1f", attitude.pitch);
                LCD_ShowString(0, 20, (u8*)buf, BLACK, WHITE, 16, 0);
                
                sprintf(buf, "Temp: %.1fC", data.temp);
                LCD_ShowString(0, 40, (u8*)buf, BLACK, WHITE, 16, 0);
            }
        }
        
        /* Update every 50ms */
        OSTimeDly(50, OS_OPT_TIME_DLY, &err);
    }
}

/**
 ******************************************************************************
 * 任务创建示例
 * ============
 * 
 * 在 start_task() 中添加以下代码：
 * 
 * // MPU6050读取任务
 * OSTaskCreate(&MPU6050_Read_TCB, "MPU6050_Read", Task_MPU6050_Read, NULL,
 *              10,  // 优先级：中等
 *              &MPU6050_Read_STK[0],
 *              MPU6050_READ_STK_SIZE / 10,
 *              MPU6050_READ_STK_SIZE,
 *              0, 0, NULL,
 *              OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
 *              &err);
 * 
 * // MPU6050显示任务
 * OSTaskCreate(&MPU6050_Display_TCB, "MPU6050_Display", Task_MPU6050_Display, NULL,
 *              15,  // 优先级：低
 *              &MPU6050_Display_STK[0],
 *              MPU6050_DISPLAY_STK_SIZE / 10,
 *              MPU6050_DISPLAY_STK_SIZE,
 *              0, 0, NULL,
 *              OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
 *              &err);
 * 
 ******************************************************************************
 */

/**
 ******************************************************************************
 * 移植检查清单
 * ============
 * 
 * ✓ 1. IIC驱动是否已移植并测试通过？
 * ✓ 2. MPU6050_Init()是否在任务中调用？
 * ✓ 3. 是否创建了必要的同步对象（互斥锁/信号量/队列）？
 * ✓ 4. 任务栈大小是否足够？（建议≥1024字节）
 * ✓ 5. 任务优先级是否合理？
 * ✓ 6. 数据共享是否使用了互斥锁保护？
 * ✓ 7. 是否处理了IIC通信失败的情况？
 * ✓ 8. 浮点运算是否会影响性能？（考虑使用定点运算）
 * 
 * 常见问题排查
 * ===========
 * 
 * 问题1：WHO_AM_I读取失败
 * 解决：检查IIC引脚、上拉电阻、MPU6050供电
 * 
 * 问题2：数据全为0或异常
 * 解决：检查寄存器配置、量程设置
 * 
 * 问题3：任务卡死
 * 解决：检查互斥锁是否正确释放、IIC超时处理
 * 
 * 问题4：数据更新不及时
 * 解决：检查任务优先级、读取周期
 * 
 ******************************************************************************
 */

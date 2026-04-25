/**
 ******************************************************************************
 * @file    mpu6050_zero_data_analysis.c
 * @brief   MPU6050数据全为零问题分析与解决方案
 * @note    问题：MPU6050初始化成功，但读取数据全为0
 * @author  分析者
 * @date    2026-02-14
 ******************************************************************************
 * 
 * 问题描述：
 * =========
 * 用户反馈：
 * 1. MPU6050初始化成功（WHO_AM_I读取正确）
 * 2. 但读取传感器数据时，所有值都是0
 * 3. 数据不变化，始终为0
 * 
 * 典型现象：
 * ----------
 * Accel: 0.00, 0.00, 0.00 g
 * Gyro:  0.00, 0.00, 0.00 dps
 * Temp:  0.00 C
 * 
 * 或
 * 
 * Accel: 0, 0, 0
 * Gyro:  0, 0, 0
 * 
 ******************************************************************************/

/* =============================================================================
 * 第一部分：问题分析
 * ============================================================================= */

/**
 * 1.1 可能原因列表（按概率排序）
 * =================================
 * 
 * 原因1：MPU6050处于睡眠模式（概率最高⭐⭐⭐⭐⭐）
 * ------------------------------------------------
 * MPU6050上电默认进入睡眠模式（PWR_MGMT_1寄存器bit6=1）
 * 睡眠模式下，传感器不工作，读取数据为0或保持不变
 * 
 * 验证方法：
 * 读取PWR_MGMT_1寄存器(0x6B)，检查bit6是否为1
 * 
 * 解决方案：
 * 写入0x00到PWR_MGMT_1，唤醒MPU6050
 * 或写入0x01，使用PLL时钟源并唤醒
 * 
 * 
 * 原因2：传感器未使能（概率⭐⭐⭐⭐）
 * ----------------------------------
 * PWR_MGMT_2寄存器(0x6C)可以单独关闭各轴传感器
 * 如果某些轴被关闭，对应数据为0
 * 
 * 验证方法：
 * 读取PWR_MGMT_2寄存器，检查bit5:0
 * 
 * 解决方案：
 * 写入0x00到PWR_MGMT_2，使能所有轴
 * 
 * 
 * 原因3：IIC读取时序错误（概率⭐⭐⭐）
 * ------------------------------------
 * 虽然初始化成功，但批量读取数据时可能有问题
 * 特别是重复起始条件(Repeated Start)处理
 * 
 * 验证方法：
 * 用逻辑分析仪抓取IIC波形
 * 检查14字节burst read时序
 * 
 * 解决方案：
 * 修复IIC_Read_Bytes函数
 * 
 * 
 * 原因4：数据未正确合并（概率⭐⭐⭐）
 * ------------------------------------
 * 高8位和低8位合并时出错
 * 导致数据变成0
 * 
 * 常见错误：
 * - 使用uint8_t而不是int16_t
 * - 移位操作错误
 * - 符号扩展问题
 * 
 * 验证方法：
 * 打印原始字节数据（buf[0]~buf[13]）
 * 
 * 
 * 原因5：FIFO未使能或为空（概率⭐⭐）
 * ------------------------------------
 * 如果使用FIFO模式读取
 * 但FIFO未使能或为空
 * 读取的数据为0
 * 
 * 验证方法：
 * 检查FIFO_EN寄存器(0x23)
 * 检查FIFO_COUNT寄存器
 * 
 * 
 * 原因6：IIC地址错误（概率⭐⭐）
 * ------------------------------
 * 虽然WHO_AM_I读取成功
 * 但批量读取时可能地址错误
 * 
 * 验证方法：
 * 检查IIC_Read_Bytes中的设备地址
 * 
 * 
 * 原因7：硬件连接问题（概率⭐）
 * ------------------------------
 * - 电源不稳定
 * - 上拉电阻过大
 * - 地线不稳
 * 
 * 验证方法：
 * 用示波器检查IIC波形质量
 * 检查电源电压
 */

/* =============================================================================
 * 第二部分：最常见原因详解（睡眠模式）
 * ============================================================================= */

/**
 * 2.1 为什么MPU6050默认是睡眠模式？
 * ===================================
 * 
 * 上电复位后，PWR_MGMT_1寄存器(0x6B)的值为0x40：
 * 
 * PWR_MGMT_1寄存器位定义：
 * ┌────┬────┬────┬────┬────┬────┬────┬────┐
 * │ b7 │ b6 │ b5 │ b4 │ b3 │ b2 │ b1 │ b0 │
 * ├────┼────┼────┼────┼────┼────┼────┼────┤
 * │H_RST│SLEEP│CYCLE│  - │TEMP_DIS│   CLKSEL   │
 * └────┴────┴────┴────┴────┴────┴────┴────┘
 * 
 * 上电默认值：
 * - H_RESET (b7): 0
 * - SLEEP (b6): 1  ← 睡眠模式使能！
 * - CYCLE (b5): 0
 * - TEMP_DIS (b3): 0
 * - CLKSEL (b2:0): 000
 * 
 * 值 = 0b01000000 = 0x40
 * 
 * 在睡眠模式下：
 * - 加速度计停止工作
 * - 陀螺仪停止工作
 * - 温度传感器停止工作
 * - 数字滤波器停止工作
 * - 只有IIC接口保持活动（用于配置和读取）
 * 
 * 这就是为什么WHO_AM_I能读取（IIC工作），
 * 但传感器数据为0（传感器停止）！
 */

/**
 * 2.2 如何唤醒MPU6050？
 * =====================
 * 
 * 步骤1：清除SLEEP位
 * ------------------
 * 写入0x00到PWR_MGMT_1：
 * - SLEEP = 0（唤醒）
 * - CLKSEL = 000（内部振荡器）
 * 
 * 或写入0x01：
 * - SLEEP = 0（唤醒）
 * - CLKSEL = 001（PLL，X轴陀螺参考）
 *   使用PLL更稳定，推荐！
 * 
 * 步骤2：使能所有轴（如果需要）
 * -----------------------------
 * 写入0x00到PWR_MGMT_2(0x6C)
 * 
 * PWR_MGMT_2寄存器：
 * - bit5: DIS_XA（关闭X轴加速度）
 * - bit4: DIS_YA（关闭Y轴加速度）
 * - bit3: DIS_ZA（关闭Z轴加速度）
 * - bit2: DIS_XG（关闭X轴陀螺）
 * - bit1: DIS_YG（关闭Y轴陀螺）
 * - bit0: DIS_ZG（关闭Z轴陀螺）
 * 
 * 0x00 = 所有位为0 = 所有轴使能
 */

/* =============================================================================
 * 第三部分：正确的初始化代码
 * ============================================================================= */

/**
 * @brief  Correct MPU6050 initialization (with wake-up sequence)
 * @retval 0=Success, 1=Failed
 * 
 * @note   Critical steps:
 *         1. Reset device
 *         2. Wake up from sleep
 *         3. Set clock source
 *         4. Enable all sensors
 *         5. Configure sample rate
 *         6. Configure DLPF
 *         7. Configure full scale ranges
 */
uint8_t MPU6050_Init_Correct(void)
{
    OS_ERR err;
    uint8_t who_am_i;
    uint8_t pwr_mgmt_1;
    
    /* Initialize IIC first */
    IIC_Init();
    
    /* Create mutex for data protection */
    OSMutexCreate(&MPU6050_Mutex, "MPU6050 Mutex", &err);
    if(err != OS_ERR_NONE)
        return 1;
    
    /* Step 1: Reset MPU6050 */
    /* Write 0x80 to PWR_MGMT_1 (H_RESET bit) */
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_1, 0x80);
    delay_ms(100);  /* Wait for reset to complete (max 100ms) */
    
    /* Step 2: Wake up from sleep and set clock source */
    /* 
     * PWR_MGMT_1 = 0x01:
     * - SLEEP = 0 (wake up) ✓
     * - CLKSEL = 001 (PLL with X gyro reference) ✓
     * 
     * Why PLL?
     * More stable than internal oscillator
     */
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_1, 0x01);
    delay_ms(10);  /* Wait for PLL to stabilize */
    
    /* Step 3: Enable all sensors */
    /* PWR_MGMT_2 = 0x00: All axes enabled */
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_2, 0x00);
    
    /* Step 4: Set sample rate */
    /* Sample Rate = Gyro Output Rate / (1 + SMPLRT_DIV) */
    /* With DLPF enabled: Gyro Output Rate = 1kHz */
    /* SMPLRT_DIV = 0: Sample Rate = 1kHz */
    MPU6050_Write_Reg(MPU6050_SMPLRT_DIV, 0x00);
    
    /* Step 5: Configure DLPF (Digital Low Pass Filter) */
    /* CONFIG = 0x03: 
     * - DLPF_CFG = 3
     * - Accel BW = 44Hz
     * - Gyro BW = 42Hz
     */
    MPU6050_Write_Reg(MPU6050_CONFIG, 0x03);
    
    /* Step 6: Configure gyroscope full scale range */
    /* GYRO_CONFIG = 0x18:
     * - FS_SEL = 3 (±2000°/s)
     * - Self-test disabled
     */
    MPU6050_Write_Reg(MPU6050_GYRO_CONFIG, 0x18);
    
    /* Step 7: Configure accelerometer full scale range */
    /* ACCEL_CONFIG = 0x01:
     * - AFS_SEL = 1 (±4g)
     * - Self-test disabled
     */
    MPU6050_Write_Reg(MPU6050_ACCEL_CONFIG, 0x01);
    
    /* Step 8: Verify device ID */
    MPU6050_Read_Reg(MPU6050_WHO_AM_I, &who_am_i);
    if(who_am_i != 0x68 && who_am_i != 0x70)
    {
        printf("MPU6050/6500 not found! WHO_AM_I = 0x%02X\r\n", who_am_i);
        return 1;
    }
    
    /* Step 9: Verify wake-up status */
    MPU6050_Read_Reg(MPU6050_PWR_MGMT_1, &pwr_mgmt_1);
    if(pwr_mgmt_1 & 0x40)
    {
        printf("Warning: MPU6050 still in sleep mode! PWR_MGMT_1 = 0x%02X\r\n", pwr_mgmt_1);
        return 1;
    }
    
    printf("MPU6050 initialized successfully! WHO_AM_I = 0x%02X\r\n", who_am_i);
    printf("PWR_MGMT_1 = 0x%02X (SLEEP bit = %d)\r\n", 
           pwr_mgmt_1, (pwr_mgmt_1 >> 6) & 0x01);
    
    return 0;
}

/* =============================================================================
 * 第四部分：数据读取代码检查
 * ============================================================================= */

/**
 * 4.1 正确的数据读取函数
 * =======================
 * 
 * 关键点：
 * 1. 使用int16_t存储数据（有符号16位）
 * 2. 正确合并高8位和低8位
 * 3. 注意符号扩展
 */

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t temp;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} MPU6050_RawData_t;

/**
 * @brief  Read all sensor data from MPU6050
 * @param  raw_data: Pointer to store raw data
 * @retval 0=Success, 1=Failed
 */
uint8_t MPU6050_Read_All_Correct(MPU6050_RawData_t *raw_data)
{
    uint8_t buf[14];
    uint8_t i;
    
    /* Read 14 bytes starting from ACCEL_XOUT_H (0x3B) */
    if(MPU6050_Read_Bytes(MPU6050_ACCEL_XOUT_H, buf, 14))
    {
        printf("IIC read failed!\r\n");
        return 1;
    }
    
    /* Debug: Print raw bytes */
    printf("Raw bytes: ");
    for(i = 0; i < 14; i++)
    {
        printf("%02X ", buf[i]);
    }
    printf("\r\n");
    
    /* Merge high and low bytes (Big Endian) */
    /* 
     * Correct way:
     * value = ((int16_t)high_byte << 8) | low_byte
     * 
     * Cast to int16_t first to ensure sign extension
     */
    
    raw_data->accel_x = ((int16_t)buf[0] << 8) | buf[1];   /* 0x3B, 0x3C */
    raw_data->accel_y = ((int16_t)buf[2] << 8) | buf[3];   /* 0x3D, 0x3E */
    raw_data->accel_z = ((int16_t)buf[4] << 8) | buf[5];   /* 0x3F, 0x40 */
    raw_data->temp    = ((int16_t)buf[6] << 8) | buf[7];   /* 0x41, 0x42 */
    raw_data->gyro_x  = ((int16_t)buf[8] << 8) | buf[9];   /* 0x43, 0x44 */
    raw_data->gyro_y  = ((int16_t)buf[10] << 8) | buf[11]; /* 0x45, 0x46 */
    raw_data->gyro_z  = ((int16_t)buf[12] << 8) | buf[13]; /* 0x47, 0x48 */
    
    return 0;
}

/**
 * 4.2 常见错误的数据合并方式
 * ============================
 * 
 * 错误1：使用uint8_t
 * ------------------
 * uint8_t high = buf[0];  // 例如 0xFF
 * uint8_t low = buf[1];   // 例如 0x80
 * uint16_t value = (high << 8) | low;  // 结果: 0xFF80 (正数)
 * 
 * 正确应该是 -128，但得到的是 65408！
 * 
 * 错误2：先移位再转换
 * --------------------
 * int16_t value = (buf[0] << 8) | buf[1];
 * 
 * 如果buf[0]是uint8_t，移位后还是uint8_t（溢出）
 * 
 * 错误3：忘记类型转换
 * --------------------
 * int16_t value = buf[0] << 8 | buf[1];
 * 
 * buf[0]是uint8_t，移位后可能溢出
 */

/* =============================================================================
 * 第五部分：调试和诊断
 * ============================================================================= */

/**
 * @brief  Comprehensive MPU6050 diagnostic
 * @note   Run this to identify the problem
 */
void MPU6050_Diagnose_Zero_Data(void)
{
    uint8_t reg_val;
    MPU6050_RawData_t raw_data;
    
    printf("\r\n========== MPU6050 Diagnostic ==========\r\n");
    
    /* Test 1: Check WHO_AM_I */
    MPU6050_Read_Reg(MPU6050_WHO_AM_I, &reg_val);
    printf("1. WHO_AM_I (0x75): 0x%02X ", reg_val);
    if(reg_val == 0x68 || reg_val == 0x70)
        printf("(OK)\r\n");
    else
        printf("(ERROR)\r\n");
    
    /* Test 2: Check PWR_MGMT_1 */
    MPU6050_Read_Reg(MPU6050_PWR_MGMT_1, &reg_val);
    printf("2. PWR_MGMT_1 (0x6B): 0x%02X\r\n", reg_val);
    printf("   - SLEEP bit (b6): %d (%s)\r\n", 
           (reg_val >> 6) & 0x01,
           (reg_val >> 6) & 0x01 ? "SLEEPING!" : "AWAKE");
    printf("   - CLKSEL (b2:0): %d\r\n", reg_val & 0x07);
    
    /* Test 3: Check PWR_MGMT_2 */
    MPU6050_Read_Reg(MPU6050_PWR_MGMT_2, &reg_val);
    printf("3. PWR_MGMT_2 (0x6C): 0x%02X\r\n", reg_val);
    if(reg_val != 0x00)
        printf("   WARNING: Some axes may be disabled!\r\n");
    
    /* Test 4: Check CONFIG */
    MPU6050_Read_Reg(MPU6050_CONFIG, &reg_val);
    printf("4. CONFIG (0x1A): 0x%02X (DLPF_CFG=%d)\r\n", 
           reg_val, reg_val & 0x07);
    
    /* Test 5: Check sample rate */
    MPU6050_Read_Reg(MPU6050_SMPLRT_DIV, &reg_val);
    printf("5. SMPLRT_DIV (0x19): 0x%02X\r\n", reg_val);
    
    /* Test 6: Read raw sensor data */
    printf("\r\n6. Reading sensor data...\r\n");
    if(MPU6050_Read_All_Correct(&raw_data) == 0)
    {
        printf("   Accel X: %d (0x%04X)\r\n", raw_data.accel_x, raw_data.accel_x);
        printf("   Accel Y: %d (0x%04X)\r\n", raw_data.accel_y, raw_data.accel_y);
        printf("   Accel Z: %d (0x%04X)\r\n", raw_data.accel_z, raw_data.accel_z);
        printf("   Temp:    %d (0x%04X)\r\n", raw_data.temp, raw_data.temp);
        printf("   Gyro X:  %d (0x%04X)\r\n", raw_data.gyro_x, raw_data.gyro_x);
        printf("   Gyro Y:  %d (0x%04X)\r\n", raw_data.gyro_y, raw_data.gyro_y);
        printf("   Gyro Z:  %d (0x%04X)\r\n", raw_data.gyro_z, raw_data.gyro_z);
        
        /* Check if all zeros */
        if(raw_data.accel_x == 0 && raw_data.accel_y == 0 && raw_data.accel_z == 0 &&
           raw_data.gyro_x == 0 && raw_data.gyro_y == 0 && raw_data.gyro_z == 0)
        {
            printf("\r\n   ERROR: All data is ZERO!\r\n");
            printf("   -> Check if MPU6050 is in sleep mode\r\n");
            printf("   -> Check PWR_MGMT_1 register\r\n");
        }
        else
        {
            printf("\r\n   Data looks good!\r\n");
        }
    }
    else
    {
        printf("   ERROR: Failed to read data!\r\n");
    }
    
    printf("\r\n========== End of Diagnostic ==========\r\n");
}

/* =============================================================================
 * 第六部分：快速修复
 * ============================================================================= */

/**
 * @brief  Quick fix for zero data problem
 * @note   Most likely solution: Wake up from sleep mode
 */
void MPU6050_Quick_Fix(void)
{
    printf("Applying quick fix...\r\n");
    
    /* Step 1: Wake up */
    printf("1. Waking up MPU6050...\r\n");
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_1, 0x01);  /* Wake up + PLL */
    delay_ms(10);
    
    /* Step 2: Enable all axes */
    printf("2. Enabling all sensors...\r\n");
    MPU6050_Write_Reg(MPU6050_PWR_MGMT_2, 0x00);
    
    /* Step 3: Verify */
    uint8_t pwr_mgmt_1;
    MPU6050_Read_Reg(MPU6050_PWR_MGMT_1, &pwr_mgmt_1);
    
    if((pwr_mgmt_1 & 0x40) == 0)
    {
        printf("Success! MPU6050 is now awake.\r\n");
    }
    else
    {
        printf("Failed! Still in sleep mode.\r\n");
    }
}

/* =============================================================================
 * 第七部分：总结
 * ============================================================================= */

/**
 * 问题诊断流程图
 * ==============
 * 
 * 开始
 *  |
 *  ├─ 读取WHO_AM_I正常？
 *  │   ├─ 否 → 检查IIC通信
 *  │   └─ 是 → 继续
 *  |
 *  ├─ 读取PWR_MGMT_1
 *  │   ├─ bit6 = 1？
 *  │   │   ├─ 是 → 睡眠模式！写入0x01唤醒 ✓
 *  │   │   └─ 否 → 继续
 *  │   └─ 其他位异常？
 *  │       ├─ 是 → 重新初始化
 *  │       └─ 否 → 继续
 *  |
 *  ├─ 读取原始字节
 *  │   ├─ 全为0x00？
 *  │   │   ├─ 是 → 检查PWR_MGMT_2，检查硬件
 *  │   │   └─ 否 → 继续
 *  │   └─ 有非零值？
 *  │       ├─ 是 → 数据合并代码错误 ✓
 *  │       └─ 否 → 继续
 *  |
 *  └─ 检查IIC波形
 *      └─ 时序错误？
 *          ├─ 是 → 修复IIC驱动 ✓
 *          └─ 否 → 芯片损坏？
 */

/**
 * 最可能的解决方案（按概率排序）
 * =================================
 * 
 * 1. 唤醒MPU6050（90%概率）
 *    MPU6050_Write_Reg(0x6B, 0x01);
 * 
 * 2. 修复数据合并代码（5%概率）
 *    使用int16_t和正确的移位操作
 * 
 * 3. 修复IIC时序（4%概率）
 *    检查Repeated Start条件
 * 
 * 4. 硬件问题（1%概率）
 *    检查电源和上拉电阻
 */

/* End of file */

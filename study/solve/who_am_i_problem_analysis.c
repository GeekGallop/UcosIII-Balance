/**
 ******************************************************************************
 * @file    who_am_i_problem_analysis.c
 * @brief   MPU6050 WHO_AM_I读取问题分析与解决方案
 * @note    问题：读取WHO_AM_I始终返回0x70，但其他寄存器读写正常
 * @author  分析者
 * @date    2026-02-14
 ******************************************************************************
 * 
 * 问题描述：
 * =========
 * 用户在MPU6050初始化时遇到以下问题：
 * 
 * 1. 写入其他寄存器（如CONFIG 0x1A）后读取，能正确返回设置的值 ✓
 * 2. 读取WHO_AM_I只读寄存器（0x75），始终返回0x70 ✗
 * 
 * 预期值：WHO_AM_I应该返回 0x68（MPU6050的固定ID）
 * 实际值：返回 0x70
 * 
 * 关键线索：
 * ----------
 * - 写寄存器后读正常 → IIC通信基本正常
 * - 只有WHO_AM_I异常 → 可能是地址、时序或芯片型号问题
 * 
 ******************************************************************************/

/* =============================================================================
 * 第一部分：问题分析
 * ============================================================================= */

/**
 * 1.1 可能原因列表（按概率排序）
 * =================================
 * 
 * 原因1：芯片型号不是MPU6050，而是MPU6500/MPU9250（概率最高⭐⭐⭐⭐⭐）
 * ------------------------------------------------------------------------
 * 不同芯片的WHO_AM_I值不同：
 * - MPU6050: 0x68
 * - MPU6500: 0x70  ← 你的返回值！
 * - MPU9250: 0x71 (实际上是MPU9250内部的AK8963磁力计地址)
 * - MPU9255: 0x73
 * 
 * 验证方法：
 * 查看芯片丝印，确认具体型号
 * 
 * 
 * 原因2：IIC设备地址错误（概率⭐⭐⭐⭐）
 * -------------------------------------
 * MPU6050有两个可能的地址：
 * - AD0引脚接地：地址 = 0x68
 * - AD0引脚接VCC：地址 = 0x69
 * 
 * 如果地址错误，可能读到其他设备的寄存器
 * 
 * 验证方法：
 * 检查硬件连接，确认AD0引脚状态
 * 
 * 
 * 原因3：IIC通信时序问题（概率⭐⭐⭐）
 * -----------------------------------
 * 虽然其他寄存器读写正常，但WHO_AM_I是只读寄存器，
 * 可能在读取时有特殊的时序要求
 * 
 * 可能的问题：
 * - 重复启动条件（Repeated Start）处理不当
 * - 寄存器地址传输后没有正确切换读写方向
 * - ACK/NACK时序错误
 * 
 * 
 * 原因4：寄存器地址定义错误（概率⭐⭐）
 * ------------------------------------
 * 检查代码中WHO_AM_I的地址定义：
 * #define MPU6050_WHO_AM_I  0x75  // 正确
 * 
 * 如果定义错误，可能读到其他寄存器的值
 * 
 * 
 * 原因5：芯片损坏或假冒（概率⭐）
 * ------------------------------
 * 如果芯片是假冒产品，WHO_AM_I可能返回异常值
 */

/**
 * 1.2 最可能的原因分析
 * =====================
 * 
 * 根据你的描述：
 * - 其他寄存器读写正常 → IIC通信没问题
 * - 只有WHO_AM_I返回0x70 → 芯片型号很可能是MPU6500
 * 
 * 为什么MPU6500会返回0x70？
 * ------------------------
 * MPU6500是MPU6050的升级版，主要改进：
 * 1. 更低的功耗
 * 2. 更高的精度
 * 3. 更小的封装
 * 4. 不同的WHO_AM_I值（0x70 vs 0x68）
 * 
 * 寄存器兼容性：
 * --------------
 * MPU6500与MPU6050的寄存器基本兼容，
 * 所以其他寄存器读写正常，
 * 只有WHO_AM_I不同
 * 
 * 这是最常见的情况！
 */

/* =============================================================================
 * 第二部分：验证方法
 * ============================================================================= */

/**
 * 2.1 检查芯片丝印
 * =================
 * 
 * 查看芯片表面的文字：
 * 
 * MPU6050丝印示例：
 * ┌─────────┐
 * │ MP92    │  ← 批次代码
 * │ 6050A   │  ← 型号：MPU6050
 * │ L8P143  │  ← 日期/批次
 * └─────────┘
 * 
 * MPU6500丝印示例：
 * ┌─────────┐
 * │ MP92    │
 * │ 6500    │  ← 型号：MPU6500
 * │ L8P143  │
 * └─────────┘
 * 
 * 如果丝印是"6500"而不是"6050"，
 * 那么WHO_AM_I=0x70是正常的！
 */

/**
 * 2.2 代码验证测试
 * =================
 * 
 * 运行以下测试代码，帮助定位问题：
 */

#include <stdio.h>

/* 测试1：读取多个寄存器，观察返回值 */
void Test_Read_Multiple_Registers(void)
{
    uint8_t value;
    
    printf("=== Register Read Test ===\r\n");
    
    /* 读取WHO_AM_I (0x75) */
    MPU6050_Read_Reg(0x75, &value);
    printf("WHO_AM_I (0x75): 0x%02X\r\n", value);
    
    /* 读取CONFIG (0x1A) */
    MPU6050_Read_Reg(0x1A, &value);
    printf("CONFIG (0x1A): 0x%02X\r\n", value);
    
    /* 读取PWR_MGMT_1 (0x6B) */
    MPU6050_Read_Reg(0x6B, &value);
    printf("PWR_MGMT_1 (0x6B): 0x%02X\r\n", value);
    
    /* 读取GYRO_CONFIG (0x1B) */
    MPU6050_Read_Reg(0x1B, &value);
    printf("GYRO_CONFIG (0x1B): 0x%02X\r\n", value);
    
    /* 读取ACCEL_CONFIG (0x1C) */
    MPU6050_Read_Reg(0x1C, &value);
    printf("ACCEL_CONFIG (0x1C): 0x%02X\r\n", value);
}

/* 预期输出（MPU6050）：
 * WHO_AM_I (0x75): 0x68
 * CONFIG (0x1A): 0x00 (或上次写入的值)
 * PWR_MGMT_1 (0x6B): 0x40 (默认睡眠模式)
 * GYRO_CONFIG (0x1B): 0x00
 * ACCEL_CONFIG (0x1C): 0x00
 */

/* 预期输出（MPU6500）：
 * WHO_AM_I (0x75): 0x70  ← 这就是你的情况！
 * CONFIG (0x1A): 0x00
 * PWR_MGMT_1 (0x6B): 0x40
 * GYRO_CONFIG (0x1B): 0x00
 * ACCEL_CONFIG (0x1C): 0x00
 */

/**
 * 2.3 IIC通信波形检查
 * ===================
 * 
 * 使用逻辑分析仪或示波器抓取IIC波形：
 * 
 * 正确的WHO_AM_I读取时序：
 * 
 * S:  START
 * P:  STOP
 * A:  ACK
 * N:  NACK
 * 
 * Master:  S  0xD0  A  0x75  A  Sr  0xD1  A  [Data]  N  P
 * Slave:            A       A       A        A  0x68        
 * 
 * 波形解释：
 * 1. S:    起始条件
 * 2. 0xD0: 发送设备地址(0x68) + 写标志(0) = 0xD0
 * 3. A:    从机应答
 * 4. 0x75: 发送寄存器地址(WHO_AM_I)
 * 5. A:    从机应答
 * 6. Sr:   重复起始条件（关键！）
 * 7. 0xD1: 发送设备地址(0x68) + 读标志(1) = 0xD1
 * 8. A:    从机应答
 * 9. [Data]: 读取数据（应该是0x68或0x70）
 * 10. N:   主机发送NACK（表示不再读取）
 * 11. P:   停止条件
 * 
 * 检查要点：
 * - 设备地址是否正确（0xD0/0xD1）
 * - 寄存器地址是否正确（0x75）
 * - 是否有重复起始条件（Sr）
 * - 数据位是否正确
 */

/* =============================================================================
 * 第三部分：解决方案
 * ============================================================================= */

/**
 * 3.1 方案A：如果是MPU6500（推荐）
 * =================================
 * 
 * 修改代码，支持MPU6500：
 */

/* 修改头文件中的定义 */
#ifndef __MPU6050_H
#define __MPU6050_H

/* 设备ID定义 */
#define MPU6050_WHO_AM_I_VAL    0x68    /* MPU6050 */
#define MPU6500_WHO_AM_I_VAL    0x70    /* MPU6500 */
#define MPU9250_WHO_AM_I_VAL    0x71    /* MPU9250 */
#define MPU9255_WHO_AM_I_VAL    0x73    /* MPU9255 */

/* 修改初始化函数 */
uint8_t MPU6050_Init(void)
{
    uint8_t who_am_i;
    
    /* 读取WHO_AM_I */
    MPU6050_Read_Reg(MPU6050_WHO_AM_I, &who_am_i);
    
    /* 检查是否是支持的芯片 */
    if (who_am_i != MPU6050_WHO_AM_I_VAL && 
        who_am_i != MPU6500_WHO_AM_I_VAL)
    {
        printf("Error: Unknown device ID 0x%02X\r\n", who_am_i);
        return 1;  /* 不支持的芯片 */
    }
    
    printf("Detected: %s (ID=0x%02X)\r\n", 
           who_am_i == MPU6050_WHO_AM_I_VAL ? "MPU6050" : "MPU6500",
           who_am_i);
    
    /* 继续初始化... */
    /* MPU6500与MPU6050寄存器基本兼容，初始化流程相同 */
    
    return 0;
}

/**
 * 3.2 方案B：检查IIC地址
 * =======================
 * 
 * 如果AD0引脚接VCC，地址是0x69
 */

/* 修改地址定义 */
#define MPU6050_ADDR_AD0_LOW    0x68    /* AD0接地 */
#define MPU6050_ADDR_AD0_HIGH   0x69    /* AD0接VCC */

/* 根据硬件选择地址 */
#define MPU6050_ADDR            MPU6050_ADDR_AD0_LOW  /* 或 AD0_HIGH */

/**
 * 3.3 方案C：修复IIC读取时序
 * ===========================
 * 
 * 如果问题是时序导致的，修复读取函数：
 */

uint8_t MPU6050_Read_Reg_Fixed(uint8_t reg, uint8_t *data)
{
    OS_ERR err;
    uint8_t result = 0;
    
    OSMutexPend(&IIC_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    
    /* 步骤1：发送设备地址（写模式） */
    IIC_Start();
    IIC_Send_Byte((MPU6050_ADDR << 1) | 0);  /* 写模式 */
    if (IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    /* 步骤2：发送寄存器地址 */
    IIC_Send_Byte(reg);
    if (IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    /* 步骤3：重复起始条件（关键！） */
    IIC_Start();  /* Sr: Repeated Start */
    
    /* 步骤4：发送设备地址（读模式） */
    IIC_Send_Byte((MPU6050_ADDR << 1) | 1);  /* 读模式 */
    if (IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    /* 步骤5：读取数据 */
    *data = IIC_Read_Byte(0);  /* 读一个字节，发送NACK */
    
exit:
    IIC_Stop();
    OSMutexPost(&IIC_Mutex, OS_OPT_POST_NONE, &err);
    
    return result;
}

/* =============================================================================
 * 第四部分：调试技巧
 * ============================================================================= */

/**
 * 4.1 添加详细调试信息
 * =====================
 */

void MPU6050_Debug_Read(uint8_t reg)
{
    uint8_t value;
    uint8_t status;
    
    printf("\r\n=== Debug Read Register 0x%02X ===\r\n", reg);
    
    /* 打印IIC通信步骤 */
    printf("1. Sending START...\r\n");
    printf("2. Sending device address 0x%02X (WRITE)...\r\n", (MPU6050_ADDR << 1));
    printf("3. Sending register address 0x%02X...\r\n", reg);
    printf("4. Sending REPEATED START...\r\n");
    printf("5. Sending device address 0x%02X (READ)...\r\n", (MPU6050_ADDR << 1) | 1);
    printf("6. Reading data...\r\n");
    printf("7. Sending STOP...\r\n");
    
    status = MPU6050_Read_Reg(reg, &value);
    
    printf("Result: status=%d, value=0x%02X\r\n", status, value);
}

/**
 * 4.2 检查IIC总线状态
 * ===================
 */

void IIC_Bus_Check(void)
{
    /* 检查SDA和SCL电平 */
    uint8_t scl = GPIO_ReadInputDataBit(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN);
    uint8_t sda = GPIO_ReadInputDataBit(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN);
    
    printf("IIC Bus Status:\r\n");
    printf("  SCL: %d (%s)\r\n", scl, scl ? "HIGH" : "LOW");
    printf("  SDA: %d (%s)\r\n", sda, sda ? "HIGH" : "LOW");
    
    if (scl && sda)
    {
        printf("  Status: IDLE (OK)\r\n");
    }
    else
    {
        printf("  Status: BUSY or ERROR!\r\n");
    }
}

/**
 * 4.3 扫描IIC总线设备
 * ===================
 */

void IIC_Scan_Bus(void)
{
    uint8_t i;
    uint8_t ack;
    
    printf("\r\n=== IIC Bus Scan ===\r\n");
    printf("Scanning for devices...\r\n");
    
    for (i = 0x03; i < 0x78; i++)
    {
        IIC_Start();
        IIC_Send_Byte(i << 1);
        ack = IIC_Wait_Ack();
        IIC_Stop();
        
        if (ack == 0)
        {
            printf("  Found device at address 0x%02X (0x%02X)\r\n", i, i << 1);
        }
        
        delay_ms(1);
    }
    
    printf("Scan complete.\r\n");
}

/* =============================================================================
 * 第五部分：总结和建议
 * ============================================================================= */

/**
 * 5.1 问题诊断流程图
 * ==================
 * 
 * 开始
 *  |
 *  ├─ 读取WHO_AM_I返回0x70？
 *  │   ├─ 是 → 检查芯片丝印
 * * │   │       ├─ 丝印是6500 → 这是MPU6500，正常现象！✓
 *  │   │       ├─ 丝印是6050 → 可能是假冒芯片或损坏
 *  │   │       └─ 看不清丝印 → 继续其他测试
 *  │   │
 *  │   └─ 否 → 检查返回值
 *  │           ├─ 返回0x68 → MPU6050正常 ✓
 *  │           ├─ 返回0x00或0xFF → 通信失败，检查硬件
 *  │           └─ 其他值 → 可能是其他型号
 *  │
 *  ├─ 其他寄存器读写正常？
 *  │   ├─ 是 → IIC通信正常，问题在芯片型号
 *  │   └─ 否 → 检查IIC通信代码和硬件连接
 *  │
 *  └─ 使用逻辑分析仪抓取波形
 *      ├─ 波形正常 → 芯片问题
 *      └─ 波形异常 → 修复IIC时序
 */

/**
 * 5.2 最终建议
 * ============
 * 
 * 最可能的情况：你的芯片是MPU6500而不是MPU6050
 * 
 * 建议操作：
 * 1. 查看芯片丝印，确认型号
 * 2. 如果是MPU6500，修改代码支持0x70 ID
 * 3. MPU6500与MPU6050寄存器兼容，可以正常使用
 * 
 * 验证代码：
 */

void Check_MPU_Type(void)
{
    uint8_t id;
    MPU6050_Read_Reg(0x75, &id);
    
    switch (id)
    {
        case 0x68:
            printf("MPU6050 detected\r\n");
            break;
        case 0x70:
            printf("MPU6500 detected (this is your case!)\r\n");
            break;
        case 0x71:
            printf("MPU9250 detected\r\n");
            break;
        case 0x73:
            printf("MPU9255 detected\r\n");
            break;
        default:
            printf("Unknown device: 0x%02X\r\n", id);
            break;
    }
}

/**
 * 5.3 快速修复代码
 * =================
 * 
 * 如果你确认是MPU6500，只需要修改这一行：
 */

/* 原代码（MPU6050） */
#define MPU6050_ID      0x68

/* 修改后（支持MPU6500） */
#define MPU6050_ID      0x70  /* 或同时支持多个ID */

/* 或者更通用的做法 */
#define IS_VALID_MPU_ID(id) ((id) == 0x68 || (id) == 0x70 || (id) == 0x71)

/* End of file */

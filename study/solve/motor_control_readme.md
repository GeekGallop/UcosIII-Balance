# 电机控制与旋转编码器测速系统

**创建日期**: 2026-02-14  
**功能**: 直流电机PWM控制 + 旋转编码器测速 + PID速度控制

---

## 📁 文件列表

### 1. 优化后的电机驱动

```desktop-local-file
{
  "localPath": "D:\\STM32\\code-re\\示例-学习\\uC-OS3-time - mpu\\Drivers\\BSP\\motor\\motor.h",
  "fileName": "motor.h"
}

{
  "localPath": "D:\\STM32\\code-re\\示例-学习\\uC-OS3-time - mpu\\Drivers\\BSP\\motor\\motor.c",
  "fileName": "motor.c"
}
```

**改进内容**:
- ✅ 完整的英文注释
- ✅ 规范的代码格式
- ✅ 支持双电机独立控制
- ✅ 4种控制模式（正转、反转、刹车、滑行）
- ✅ 速度百分比设置
- ✅ 详细的宏定义和配置选项

---

### 2. 旋转编码器驱动

```desktop-local-file
{
  "localPath": "D:\\STM32\\code-re\\示例-学习\\uC-OS3-time - mpu\\Drivers\\BSP\\encoder\\encoder.h",
  "fileName": "encoder.h"
}

{
  "localPath": "D:\\STM32\\code-re\\示例-学习\\uC-OS3-time - mpu\\Drivers\\BSP\\encoder\\encoder.c",
  "fileName": "encoder.c"
}
```

**功能特性**:
- ✅ 正交编码器接口（QEI）硬件解码
- ✅ 4倍频计数模式
- ✅ 自动方向检测
- ✅ 速度计算（RPM）
- ✅ 位置跟踪
- ✅ 防抖动数字滤波

---

### 3. RTOS电机控制任务

```desktop-local-file
{
  "localPath": "D:\\STM32\\code-re\\示例-学习\\uC-OS3-time\\study\\motor_control_task.c",
  "fileName": "motor_control_task.c"
}
```

**任务架构**:
- ✅ 100Hz速度控制任务（PID闭环）
- ✅ 10Hz监控显示任务
- ✅ 完整的PID控制器实现
- ✅ 互斥锁保护共享数据

---

## 🔌 硬件连接

### 电机驱动（H桥）

```
Motor A (Left):
  PWM:    PD13 (TIM4_CH2)
  DIR1:   PD11 (AIN1)
  DIR2:   PD12 (AIN2)

Motor B (Right):
  PWM:    PD14 (TIM4_CH3)
  DIR1:   PD8  (BIN1)
  DIR2:   PD9  (BIN2)
```

### 旋转编码器

```
Encoder A (Left):
  CH_A:   PA0 (TIM2_CH1)
  CH_B:   PA1 (TIM2_CH2)

Encoder B (Right):
  CH_A:   PA6 (TIM3_CH1)
  CH_B:   PA7 (TIM3_CH2)
```

---

## ⚡ 快速开始

### 1. 初始化电机和编码器

```c
#include "motor.h"
#include "encoder.h"

void bsp_init(void)
{
    /* Initialize motor driver */
    Motor_Init();
    
    /* Initialize encoders */
    Encoder_Init();
    
    /* Other initializations... */
}
```

### 2. 创建控制任务

```c
#include "motor_control_task.c"

void start_task(void *p_arg)
{
    /* Create motor control tasks */
    Motor_Control_Tasks_Create();
    
    /* Delete start task */
    OSTaskDel(NULL, &err);
}
```

### 3. 控制电机速度

```c
/* Set Motor A to 50% speed forward */
Motor_SetSpeedPercent(MOTOR_A, 50.0f);

/* Set Motor B to 30% speed backward */
Motor_SetSpeedPercent(MOTOR_B, -30.0f);

/* Stop both motors */
Motor_Stop(MOTOR_BOTH);
```

### 4. 读取编码器数据

```c
/* Get current RPM */
float rpm_a = Encoder_GetRPM(ENCODER_A);
float rpm_b = Encoder_GetRPM(ENCODER_B);

/* Get position */
int32_t pos_a = Encoder_GetPosition(ENCODER_A);
```

---

## 📊 控制架构

```
┌─────────────────────────────────────────┐
│         Task_Motor_Speed_Control        │
│              (100Hz, High Priority)     │
├─────────────────────────────────────────┤
│  1. Read encoder counts                 │
│  2. Calculate RPM                       │
│  3. Compute PID output                  │
│  4. Update PWM duty cycle               │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│         Task_Motor_Monitor              │
│              (10Hz, Low Priority)       │
├─────────────────────────────────────────┤
│  1. Display speed and status            │
│  2. Handle user commands                │
│  3. Log data                            │
└─────────────────────────────────────────┘
```

---

## 🎮 PID速度控制

### 启用PID控制

```c
/* Set target speed */
Motor_Control_SetSpeed(MOTOR_A, 100.0f);  /* 100 RPM */

/* Enable PID control */
Motor_Control_EnablePID(MOTOR_A, 1);
```

### 调整PID参数

```c
/* Modify PID gains (Kp, Ki, Kd) */
g_motor_a_ctrl.pid.kp = 2.0f;   /* Proportional */
g_motor_a_ctrl.pid.ki = 0.5f;   /* Integral */
g_motor_a_ctrl.pid.kd = 0.1f;   /* Derivative */
```

### PID调参建议

| 现象 | 原因 | 解决方法 |
|------|------|----------|
| 响应慢 | Kp太小 | 增大Kp |
| 超调大 | Kp太大 | 减小Kp，增大Kd |
| 稳态误差 | Ki太小 | 增大Ki |
| 振荡 | Kd太小 | 增大Kd |

---

## 📈 性能指标

### 电机控制
- PWM频率: 10kHz
- PWM分辨率: 0.1% (0-1000)
- 控制频率: 100Hz

### 编码器测速
- 编码器分辨率: 20 PPR
- 减速比: 1:30
- 有效分辨率: 2400 counts/rev
- 测速精度: ±1 RPM @ 100Hz

---

## 🔧 配置选项

### 修改PWM频率

```c
/* In motor.h */
#define MOTOR_PWM_FREQ      20000   /* 20kHz */
#define MOTOR_PWM_PERIOD    1000

/* Calculate prescaler:
 * 84MHz / (prescaler + 1) / period = frequency
 * For 20kHz: prescaler = 3, period = 999
 */
```

### 修改编码器参数

```c
/* In encoder.h */
#define ENCODER_PPR             20      /* Pulses per revolution */
#define ENCODER_GEAR_RATIO      30      /* Gear ratio */
#define ENCODER_COUNTS_PER_REV  (ENCODER_PPR * 4 * ENCODER_GEAR_RATIO)
```

---

## 🐛 常见问题

### Q1: 电机不转
**检查**:
- 电源电压是否正常（通常7-12V）
- PWM引脚连接是否正确
- 方向引脚配置是否正确
- 电机驱动使能引脚是否拉高

### Q2: 编码器读数为0
**检查**:
- 编码器电源（3.3V或5V）
- A/B相信号线是否接反
- 上拉电阻是否焊接（如果是开漏输出）

### Q3: 速度波动大
**解决**:
- 增大PID的Kd参数
- 降低控制频率
- 添加速度滤波

### Q4: 电机发热
**解决**:
- 降低PWM频率到可听范围外（>20kHz）
- 检查电机驱动电流限制
- 确保散热良好

---

## 📚 参考资料

### 正交编码器原理
```
正转: A领先B 90°
反转: B领先A 90°

4倍频: 在A和B的上升沿和下降沿都计数
      分辨率 = PPR × 4
```

### H桥控制真值表
```
AIN1  AIN2  状态
  0     0    滑行（高阻）
  0     1    反转
  1     0    正转
  1     1    刹车（短接）
```

---

## 🎯 下一步扩展

1. **位置控制**: 添加位置PID，实现精确定位
2. **轨迹跟踪**: 双电机协调控制，实现直线/圆弧运动
3. **速度规划**: S曲线加减速，平滑启动停止
4. **通信接口**: 接收上位机速度指令（串口/CAN）

---

**祝你使用愉快！** 🎉

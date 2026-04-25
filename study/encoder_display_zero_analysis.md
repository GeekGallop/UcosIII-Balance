# 编码器LCD显示为零问题分析报告

## 问题描述
MPU6050数据在LCD上正常显示，但编码器（Encoder）数据显示始终为零。

---

## 根本原因分析

### 🔴 问题1：TIM8定时器被注释掉了（最严重）

**文件位置**：`Drivers\BSP\bsp.c`

**问题代码**：
```c
void bsp_init(void)
{
    // ... 其他初始化 ...
    
    Motor_Init();
    
    //TIM8_Int_Init();  // <-- 被注释掉了！！！
}
```

**影响**：
- TIM8是编码器数据更新的定时器，每10ms触发一次中断
- `TIM8_Int_Init()`被注释导致：
  1. TIM8定时器未初始化
  2. NVIC中断未配置
  3. `Encoder_UpdateAll()` 永远不会被调用
  4. 编码器位置始终为0

**证据**：
```c
// tim.c 中的中断服务函数
void TIM8_UP_TIM13_IRQHandler(void)
{
    OSIntEnter();  
    if(TIM_GetITStatus(TIM8, TIM_IT_Update) == SET)
    {
        Encoder_UpdateAll();  // <-- 每10ms更新一次编码器数据
    }
    TIM_ClearITPendingBit(TIM8, TIM_IT_Update);
    OSIntExit();
}
```

---

### 🟡 问题2：编码器任务重复调用 `Encoder_UpdateAll()`

**文件位置**：`Drivers\BSP\encoder\task_encoder.c`

**问题代码**：
```c
void Task_Encoder_Speed(void *p_arg)
{
    while (1) {
        OSMutexPend(&g_encoder_mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        Encoder_UpdateAll();  // <-- 重复调用！
        
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        // ...
    }
}
```

**问题分析**：
- `Encoder_UpdateAll()` 已经在 TIM8 中断中被调用（10ms周期）
- 任务中再次调用会导致：
  1. 重复计算delta，数据错误
  2. 与中断竞争，可能产生不一致数据
  3. 如果TIM8未启动，这里的调用也无法获取有效数据

**正确做法**：
编码器任务应该只读取已经由中断更新的数据，而不是再次调用Update：
```c
void Task_Encoder_Speed(void *p_arg)
{
    while (1) {
        // 直接读取数据，不调用 Encoder_UpdateAll()
        OSMutexPend(&g_encoder_mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        // 计算RPM等（基于中断已更新的数据）
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        
        // 获取数据...
        pos_left = g_encoder_left_data.position;
        // ...
        
        OSMutexPost(&g_encoder_mutex, OS_OPT_POST_NONE, &err);
    }
}
```

---

### 🟡 问题3：消息队列数据类型大小问题（潜在风险）

**文件位置**：`Drivers\BSP\lcd\task_lcd.c`

**问题代码**：
```c
typedef struct {
    Display_Data_Type_t type;           // 枚举类型
    union {
        MPU6050_Data_t mpu_data;        // 可能很大
        MPU6050_Attitude_t attitude;    // 较小
        Encoder_Display_Data_t encoder; // 中等
    } data;
} Display_Data_Packet_t;
```

**潜在问题**：
- 联合体大小由最大成员决定
- 如果 `MPU6050_Data_t` 很大，每次发送都会传输大量数据
- 使用静态变量发送可能导致数据覆盖

**当前代码**：
```c
int LCD_Send_Encoder_Data(Encoder_Display_Data_t *encoder_data)
{
    static Display_Data_Packet_t enc_packet;  // <-- 静态变量！
    packet = &enc_packet;
    // ...
    OSQPost(&g_display_queue, (void *)packet, ...);
}
```

**风险**：
- 静态变量在多次调用时会被覆盖
- 如果队列满，数据可能在被处理前被下一次调用覆盖

---

## 解决方案

### 方案1：启用TIM8定时器（必须）

**修改文件**：`Drivers\BSP\bsp.c`

```c
void bsp_init(void)
{
    /* Configure NVIC priority group */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    
    /* Initialize delay function (168MHz) */
    delay_init(168);
    
    /* Initialize LCD with mutex */
    lcd_config();
    
    /* Configure external interrupt */
    exti_config();
    
    /* Initialize LED */
    led_init();
    
    /* Configure USART with mutex and queue */
    usart_config();
    
    /* Initialize KEY */
    KEY_Init();
    
    /* Initialize motor */
    Motor_Init();
    
    /* Initialize TIM8 for encoder update - 10ms interrupt */
    TIM8_Int_Init();  // <-- 取消注释！
}
```

---

### 方案2：修复编码器任务

**修改文件**：`Drivers\BSP\encoder\task_encoder.c`

```c
void Task_Encoder_Speed(void *p_arg)
{
    OS_ERR err;
    int16_t speed_left, speed_right;
    float rpm_left, rpm_right;
    int32_t pos_left, pos_right;
    int8_t dir_left, dir_right;
    Encoder_Display_Data_t display_data;
    
    (void)p_arg;
    
    /* Initialize encoder hardware */
    Encoder_Init();
    
    printf("Encoder Task Started - 100Hz sampling\r\n");
    
    while (1) {
        /* 注意：Encoder_UpdateAll() 在TIM8中断中调用，这里不需要再调用！
         * 这里只读取已由中断更新的数据
         */
        
        /* Read all encoder data with mutex protection */
        OSMutexPend(&g_encoder_mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        /* Calculate speed and RPM (based on data updated by interrupt) */
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        Encoder_Calculate_Speed(&g_encoder_right_data, 10.0f);
        
        /* Get all data for local copy */
        speed_left = g_encoder_left_data.speed;
        speed_right = g_encoder_right_data.speed;
        rpm_left = g_encoder_left_data.rpm;
        rpm_right = g_encoder_right_data.rpm;
        pos_left = g_encoder_left_data.position;
        pos_right = g_encoder_right_data.position;
        dir_left = g_encoder_left_data.direction;
        dir_right = g_encoder_right_data.direction;
        
        OSMutexPost(&g_encoder_mutex, OS_OPT_POST_NONE, &err);
        
        /* Prepare display data */
        display_data.position_left = pos_left;
        display_data.position_right = pos_right;
        display_data.speed_left = speed_left;
        display_data.speed_right = speed_right;
        display_data.rpm_left = rpm_left;
        display_data.rpm_right = rpm_right;
        display_data.direction_left = dir_left;
        display_data.direction_right = dir_right;
        
        /* Send to LCD display task (non-blocking) */
        LCD_Send_Encoder_Data(&display_data);
        
        /* 10ms period = 100Hz sampling rate */
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}
```

---

### 方案3：修复消息队列发送（可选但推荐）

**修改文件**：`Drivers\BSP\lcd\task_lcd.c`

使用动态内存或确保数据被复制：

```c
int LCD_Send_Encoder_Data(Encoder_Display_Data_t *encoder_data)
{
    OS_ERR err;
    Display_Data_Packet_t *packet;
    
    /* 从uC/OS-III内存分区分配，或使用局部变量 */
    /* 方案A：使用局部变量（推荐，简单场景） */
    Display_Data_Packet_t local_packet;
    packet = &local_packet;
    
    packet->type = DISPLAY_DATA_ENCODER;
    packet->data.encoder = *encoder_data;
    
    /* Post to queue - 注意：uC/OS-III会复制数据 */
    OSQPost(&g_display_queue, 
            (void *)packet, 
            sizeof(Display_Data_Packet_t), 
            OS_OPT_POST_FIFO | OS_OPT_POST_NO_SCHED, 
            &err);
    
    return (err == OS_ERR_NONE) ? 0 : -1;
}
```

**注意**：实际上 uC/OS-III 的 `OSQPost` 只传递指针，不复制数据。所以更好的方案是使用内存分区或确保发送方等待。

---

## 数据流验证

修复后的正确数据流：

```
┌─────────────────────────────────────────────────────────────┐
│  TIM8 中断 (10ms)                                           │
│  ├─ TIM8_UP_TIM13_IRQHandler()                              │
│  │   └─ Encoder_UpdateAll()  ← 读取硬件计数器，更新位置     │
│  │       ├─ Encoder_Update(LEFT)                            │
│  │       └─ Encoder_Update(RIGHT)                           │
│  └─ 每10ms执行一次                                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  Encoder Task (10ms周期)                                    │
│  ├─ Task_Encoder_Speed()                                    │
│  │   ├─ 获取互斥量                                          │
│  │   ├─ Encoder_Calculate_Speed() ← 基于中断更新的数据计算  │
│  │   ├─ 读取 position, speed, rpm, direction               │
│  │   ├─ 释放互斥量                                          │
│  │   └─ LCD_Send_Encoder_Data() ← 发送到LCD任务             │
│  └─ OSTimeDly(10)                                           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  LCD Task (50ms周期)                                        │
│  ├─ Task_LCD_Display()                                      │
│  │   ├─ OSQPend() 接收数据                                  │
│  │   ├─ LCD_Process_Data_Packet() 缓存数据                  │
│  │   └─ LCD_Update_Encoder_XXX_Display() 显示              │
│  └─ OSTimeDly(50)                                           │
└─────────────────────────────────────────────────────────────┘
```

---

## 调试建议

### 1. 验证TIM8是否工作
在 `TIM8_UP_TIM13_IRQHandler` 中添加LED闪烁或串口输出：
```c
void TIM8_UP_TIM13_IRQHandler(void)
{
    OSIntEnter();  
    if(TIM_GetITStatus(TIM8, TIM_IT_Update) == SET)
    {
        static uint32_t cnt = 0;
        if (++cnt >= 100) {  // 每1秒输出一次
            cnt = 0;
            printf("TIM8 IRQ working!\r\n");
        }
        
        Encoder_UpdateAll();
    }
    TIM_ClearITPendingBit(TIM8, TIM_IT_Update);
    OSIntExit();
}
```

### 2. 验证编码器硬件计数器
在 `Task_Encoder_Speed` 中添加调试输出：
```c
void Task_Encoder_Speed(void *p_arg)
{
    // ...
    while (1) {
        // 直接读取硬件计数器（不经过Update）
        uint16_t hw_cnt_left = TIM_GetCounter(TIM2);
        uint16_t hw_cnt_right = TIM_GetCounter(TIM3);
        printf("HW Counters: L=%u, R=%u\r\n", hw_cnt_left, hw_cnt_right);
        
        // ... 正常代码
    }
}
```

### 3. 验证互斥量工作
检查 `Encoder_Update` 是否成功获取互斥量：
```c
void Encoder_Update(uint8_t encoder)
{
    // ...
    OSMutexPend(&g_encoder_mutex, 0, OS_OPT_PEND_NON_BLOCKING, NULL, &err);
    
    if (err == OS_ERR_NONE) {
        // 成功获取
        data->position += delta;
        // ...
        OSMutexPost(&g_encoder_mutex, OS_OPT_POST_NONE, &err);
    } else {
        // 获取失败（在ISR中不应该发生）
        printf("Encoder_Update: Mutex failed!\r\n");
    }
}
```

---

## 总结

| 问题 | 严重程度 | 修复方法 |
|------|----------|----------|
| TIM8被注释 | 🔴 严重 | 取消 `bsp.c` 中的注释 |
| 重复调用Update | 🟡 中等 | 从 `task_encoder.c` 中删除 `Encoder_UpdateAll()` |
| 消息队列数据 | 🟢 轻微 | 使用局部变量或内存分区 |

**最关键的修复**：在 `bsp.c` 中启用 `TIM8_Int_Init()`！

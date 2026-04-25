# 编码器LCD显示为零问题分析 - 版本2

## 问题描述
TIM8已启用，但编码器在LCD上显示仍始终为零。

---

## 深入分析发现的问题

### 🔴 问题1：uC/OS-III 中断优先级配置冲突（最可能原因）

**文件位置**：`Drivers\BSP\tim\tim.c`

**当前配置**：
```c
NVIC_InitStructure.NVIC_IRQChannel = TIM8_UP_TIM13_IRQn;
NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;  // 抢占优先级1
NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x03;         // 子优先级3
```

**问题分析**：
1. 在 uC/OS-III 中，`NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4)` 配置下：
   - 抢占优先级范围：0-15（4位）
   - 子优先级：0（无子优先级）

2. **关键问题**：uC/OS-III 的 PendSV 和 SysTick 中断优先级设置：
   - uC/OS-III 会将 PendSV 和 SysTick 设置为最低优先级（通常是15）
   - 但 TIM8 优先级为1，这在 NVIC_PriorityGroup_4 下是**高于**PendSV的
   - 这本身不是问题，但如果 uC/OS-III 的临界区处理不当，可能导致中断被错误地屏蔽

3. **更严重的问题**：`OSMutexPend` 在中断中的使用
   ```c
   void Encoder_Update(uint8_t encoder)
   {
       // ...
       OSMutexPend(&g_encoder_mutex, 0, OS_OPT_PEND_NON_BLOCKING, NULL, &err);
       // ...
   }
   ```
   在 uC/OS-III 中，**在中断中调用 OSMutexPend 是非常危险的**，即使使用 NON_BLOCKING 模式！

**解决方案**：
在中断中**不应该使用互斥量**。应该使用以下替代方案之一：
- 方案A：使用临界区（关中断）保护共享数据
- 方案B：使用双缓冲机制
- 方案C：使用原子操作（如果数据类型允许）

---

### 🔴 问题2：数据更新和读取的时序问题

**文件位置**：`Drivers\BSP\encoder\task_encoder.c`

**当前代码**：
```c
void Task_Encoder_Speed(void *p_arg)
{
    while (1) {
        OSMutexPend(&g_encoder_mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        /* Calculate speed and RPM */
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        Encoder_Calculate_Speed(&g_encoder_right_data, 10.0f);
        
        /* Get all data for local copy */
        pos_left = g_encoder_left_data.position;
        // ...
        
        OSMutexPost(&g_encoder_mutex, OS_OPT_POST_NONE, &err);
        
        /* Send to LCD */
        LCD_Send_Encoder_Data(&display_data);
        
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);  // 10ms
    }
}
```

**问题分析**：
1. 任务和中断都在竞争互斥量
2. 如果中断中的 `Encoder_Update` 无法获取互斥量（虽然是非阻塞的），数据就不会更新
3. 任务每10ms运行一次，与TIM8中断频率相同，可能导致频繁竞争

**验证方法**：
在 `Encoder_Update` 中添加调试输出：
```c
void Encoder_Update(uint8_t encoder)
{
    // ...
    OSMutexPend(&g_encoder_mutex, 0, OS_OPT_PEND_NON_BLOCKING, NULL, &err);
    
    if (err == OS_ERR_NONE) {
        data->position += delta;
        // ...
        OSMutexPost(&g_encoder_mutex, OS_OPT_POST_NONE, &err);
    } else {
        // 添加调试输出
        static uint32_t fail_cnt = 0;
        if (++fail_cnt % 100 == 0) {
            printf("Encoder_Update: Mutex failed %lu times\r\n", fail_cnt);
        }
    }
    
    data->last_counter = current_counter;  // 这行在mutex失败时也会执行
}
```

---

### 🟡 问题3：消息队列数据被覆盖

**文件位置**：`Drivers\BSP\lcd\task_lcd.c`

**当前代码**：
```c
int LCD_Send_Encoder_Data(Encoder_Display_Data_t *encoder_data)
{
    static Display_Data_Packet_t enc_packet;  // 静态变量！
    packet = &enc_packet;
    
    packet->type = DISPLAY_DATA_ENCODER;
    packet->data.encoder = *encoder_data;
    
    OSQPost(&g_display_queue, (void *)packet, ...);
}
```

**问题分析**：
1. 使用静态变量发送数据到队列
2. uC/OS-III 的 `OSQPost` 只传递指针，不复制数据
3. 如果队列满，数据可能在被处理前被下一次调用覆盖
4. 这可能导致LCD显示旧数据或零

**解决方案**：
使用消息池或确保数据复制：
```c
// 方案：使用消息池
#define MSG_POOL_SIZE 10
static Display_Data_Packet_t msg_pool[MSG_POOL_SIZE];
static uint8_t msg_pool_idx = 0;

int LCD_Send_Encoder_Data(Encoder_Display_Data_t *encoder_data)
{
    OS_ERR err;
    Display_Data_Packet_t *packet;
    
    // 从池中获取一个消息
    packet = &msg_pool[msg_pool_idx];
    msg_pool_idx = (msg_pool_idx + 1) % MSG_POOL_SIZE;
    
    packet->type = DISPLAY_DATA_ENCODER;
    packet->data.encoder = *encoder_data;
    
    OSQPost(&g_display_queue, (void *)packet, sizeof(Display_Data_Packet_t), 
            OS_OPT_POST_FIFO | OS_OPT_POST_NO_SCHED, &err);
    
    return (err == OS_ERR_NONE) ? 0 : -1;
}
```

---

### 🟡 问题4：LCD任务队列处理可能有问题

**文件位置**：`Drivers\BSP\lcd\task_lcd.c`

**当前代码**：
```c
void Task_LCD_Display(void *p_arg)
{
    while (1) {
        // 处理所有消息
        do {
            packet = (Display_Data_Packet_t *)OSQPend(
                &g_display_queue,
                0,  // No timeout
                OS_OPT_PEND_NON_BLOCKING,
                &msg_size,
                &ts,
                &err
            );
            
            if (err == OS_ERR_NONE && packet != NULL) {
                LCD_Process_Data_Packet(packet);
            }
        } while (err == OS_ERR_NONE);
        
        // 更新显示
        LCD_Update_MPU_Display();
        LCD_Update_Encoder_Left_Display();
        LCD_Update_Encoder_Right_Display();
        
        OSTimeDly(50, OS_OPT_TIME_DLY, &err);  // 50ms
    }
}
```

**问题分析**：
1. 每次循环处理所有消息，但只保留最后一条消息的数据（因为会覆盖缓存）
2. 如果编码器数据在MPU6050数据之后到达，可能会被覆盖处理逻辑影响
3. `LCD_Process_Data_Packet` 中设置 `enc_left_valid = 1`，但如果数据包类型判断错误，可能不执行

---

### 🟢 问题5：硬件计数器可能真的为零

**验证方法**：
在 `Task_Encoder_Speed` 中直接读取硬件计数器：
```c
void Task_Encoder_Speed(void *p_arg)
{
    while (1) {
        // 直接读取硬件计数器（不经过Update）
        uint16_t hw_left = TIM_GetCounter(TIM2);
        uint16_t hw_right = TIM_GetCounter(TIM3);
        printf("HW Counters: L=%u, R=%u\r\n", hw_left, hw_right);
        
        // ... 正常代码
        OSTimeDly(100, OS_OPT_TIME_DLY, &err);  // 100ms for debug
    }
}
```

如果硬件计数器也为零，说明：
1. 编码器硬件未连接
2. GPIO配置错误
3. TIM2/TIM3未正确配置

---

## 推荐的修复方案

### 方案1：修复中断中的互斥量问题（最重要）

**修改文件**：`Drivers\BSP\encoder\encoder.c`

将 `Encoder_Update` 中的互斥量改为临界区：
```c
void Encoder_Update(uint8_t encoder)
{
    TIM_TypeDef *tim;
    Encoder_Data_t *data;
    uint16_t current_counter;
    int16_t delta;
    CPU_SR_ALLOC();  // uC/OS-III 临界区变量
    
    /* Select timer and data structure */
    if (encoder == ENCODER_LEFT) {
        tim = ENC_L_TIM;
        data = &g_encoder_left_data;
    } else if (encoder == ENCODER_RIGHT) {
        tim = ENC_R_TIM;
        data = &g_encoder_right_data;
    } else {
        return;
    }
    
    /* Read hardware counter */
    current_counter = TIM_GetCounter(tim);
    delta = (int16_t)(current_counter - data->last_counter);
    
    /* 使用临界区代替互斥量（ISR安全） */
    CPU_CRITICAL_ENTER();
    
    data->position += delta;
    data->delta_position = delta;
    data->speed = delta;
    
    if (delta > 0) {
        data->direction = 1;
    } else if (delta < 0) {
        data->direction = -1;
    } else {
        data->direction = 0;
    }
    
    data->last_counter = current_counter;
    
    CPU_CRITICAL_EXIT();
}
```

同时修改所有读取函数使用临界区：
```c
int32_t Encoder_GetPosition(uint8_t encoder)
{
    CPU_SR_ALLOC();
    int32_t position = 0;
    
    CPU_CRITICAL_ENTER();
    
    if (encoder == ENCODER_LEFT) {
        position = g_encoder_left_data.position;
    } else if (encoder == ENCODER_RIGHT) {
        position = g_encoder_right_data.position;
    }
    
    CPU_CRITICAL_EXIT();
    
    return position;
}
```

### 方案2：修复消息队列数据覆盖问题

**修改文件**：`Drivers\BSP\lcd\task_lcd.c`

使用消息池：
```c
#define MSG_POOL_SIZE 16
static Display_Data_Packet_t g_msg_pool[MSG_POOL_SIZE];
static volatile uint8_t g_msg_pool_wr_idx = 0;
static volatile uint8_t g_msg_pool_rd_idx = 0;

int LCD_Send_Encoder_Data(Encoder_Display_Data_t *encoder_data)
{
    OS_ERR err;
    Display_Data_Packet_t *packet;
    
    // 检查队列是否已满
    uint8_t next_wr = (g_msg_pool_wr_idx + 1) % MSG_POOL_SIZE;
    if (next_wr == g_msg_pool_rd_idx) {
        return -1;  // 队列满
    }
    
    packet = &g_msg_pool[g_msg_pool_wr_idx];
    g_msg_pool_wr_idx = next_wr;
    
    packet->type = DISPLAY_DATA_ENCODER;
    packet->data.encoder = *encoder_data;
    
    OSQPost(&g_display_queue, (void *)packet, sizeof(Display_Data_Packet_t), 
            OS_OPT_POST_FIFO, &err);
    
    return (err == OS_ERR_NONE) ? 0 : -1;
}
```

### 方案3：添加调试输出定位问题

在关键位置添加调试信息：
```c
// 1. 在 TIM8 中断中
void TIM8_UP_TIM13_IRQHandler(void)
{
    OSIntEnter();
    if(TIM_GetITStatus(TIM8, TIM_IT_Update) == SET) {
        static uint32_t irq_cnt = 0;
        if (++irq_cnt % 100 == 0) {
            printf("TIM8 IRQ: %lu\r\n", irq_cnt);
        }
        Encoder_UpdateAll();
    }
    TIM_ClearITPendingBit(TIM8, TIM_IT_Update);
    OSIntExit();
}

// 2. 在 Encoder_Update 中
void Encoder_Update(uint8_t encoder)
{
    // ... 计算 delta
    if (delta != 0) {
        static uint32_t update_cnt = 0;
        if (++update_cnt % 100 == 0) {
            printf("Encoder_Update: encoder=%d, delta=%d, pos=%ld\r\n", 
                   encoder, delta, data->position);
        }
    }
    // ...
}

// 3. 在 Task_Encoder_Speed 中
void Task_Encoder_Speed(void *p_arg)
{
    while (1) {
        printf("Task: pos_l=%ld, pos_r=%ld\r\n", 
               g_encoder_left_data.position, 
               g_encoder_right_data.position);
        // ...
    }
}
```

---

## 调试步骤建议

1. **第一步**：验证TIM8中断是否执行
   - 在中断中添加LED翻转或串口输出
   - 确认每10ms触发一次

2. **第二步**：验证硬件计数器
   - 在任务中直接读取 TIM2/TIM3 计数器
   - 手动转动电机，看计数器是否变化

3. **第三步**：验证 Encoder_Update 是否被调用
   - 添加调试输出
   - 检查 delta 是否非零

4. **第四步**：验证互斥量/临界区
   - 检查是否成功获取锁
   - 考虑改用临界区

5. **第五步**：验证消息队列
   - 检查数据是否正确发送到队列
   - 检查LCD任务是否正确接收

---

## 最可能的根本原因

根据分析，最可能的原因是：

**在 uC/OS-III 中断中使用了互斥量（`OSMutexPend`），导致中断处理异常或数据未更新。**

uC/OS-III 的文档明确建议：**在中断服务程序中不应该使用互斥量**，应该使用：
- 关中断（临界区）
- 信号量（Semaphore）
- 消息队列（ISR安全版本）

请尝试将 `Encoder_Update` 中的互斥量改为临界区，这很可能解决问题！

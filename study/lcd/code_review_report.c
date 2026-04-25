/**
 ******************************************************************************
 * @file    code_review_report.c
 * @brief   uC-OS3-time 项目代码审查报告
 * @note    本文件包含对 STM32 uC/OS-III 项目发现的所有问题及改进建议
 * @date    2026-02-15
 ******************************************************************************
 */

/*
 ******************************************************************************
 *                              审查摘要
 ******************************************************************************
 * 
 * 项目名称: uC-OS3-time (STM32F4 + uC/OS-III RTOS)
 * 审查日期: 2026-02-15
 * 发现问题总数: 15个
 *   - 严重问题: 3个
 *   - 高优先级: 4个
 *   - 中优先级: 5个
 *   - 低优先级: 3个
 * 
 ******************************************************************************
 */

/*
 ******************************************************************************
 *                          第一部分: 严重问题
 ******************************************************************************
 */

/*
 * 问题 #1 [严重]: LCD_Task 创建后立即被删除
 * 位置: task.c, start_task() 函数
 * 
 * 问题描述:
 *   以下代码在创建 LCD_Task 后立即将其删除:
 *   
 *   OSTaskCreate(&LCD_Task_TCB, "LCD_Task", LCD_Sensor_Task, NULL, 8, ...);
 *   ...
 *   OSTaskDel(&LCD_Task_TCB, &err);  // <-- 这一行删除了任务!
 *   OSTaskDel(NULL, &err);           // 删除启动任务
 * 
 * 影响:
 *   LCD_Sensor_Task 永远不会运行。任务被创建后立即删除，导致 LCD 显示功能完全失效。
 * 
 * 修复建议:
 *   删除这行代码: OSTaskDel(&LCD_Task_TCB, &err);
 *   只有启动任务应该使用 OSTaskDel(NULL, &err) 删除自身。
 */

/*
 * 问题 #2 [严重]: 多个 MPU6050 任务共享同一个信号量
 * 位置: task.c, Task_MPU6050_Display() 和 Task_MPU6050_LCD_Display()
 * 
 * 问题描述:
 *   Task_MPU6050_Display 和 Task_MPU6050_LCD_Display 两个任务都在等待同一个
 *   信号量 MPU6050_Data_Ready_Sem。当 Task_MPU6050_Read 中调用 OSSemPost 时，
 *   只有一个任务能收到信号（uC/OS-III 中二值信号量的特性）。
 * 
 * 影响:
 *   其中一个显示任务会错过数据更新，导致串口输出和 LCD 显示不一致。
 * 
 * 修复建议:
 *   方案 A: 使用计数信号量（增加初始计数值）
 *   方案 B: 发送信号时使用 OS_OPT_POST_ALL 选项
 *   方案 C: 为每个消费任务创建独立的信号量
 *   方案 D: 使用消息队列替代信号量+互斥锁的组合
 */

/*
 * 问题 #3 [严重]: 任务优先级设置不一致
 * 位置: task.c, 多个任务定义处
 * 
 * 问题描述:
 *   - PROTOCOL_PRIO 定义为 15，但 LCD/MPU 任务使用优先级 8
 *   - 启动任务优先级为 2（最高），这是正确的
 *   - 但 LCD_TASK_PRIO 定义为 10，实际创建时却使用优先级 8
 *   
 *   #define PROTOCOL_PRIO       15  // 串口任务高优先级
 *   #define LCD_TASK_PRIO       10  // 定义为 10
 *   ...
 *   OSTaskCreate(&LCD_Task_TCB, ..., 8, ...);  // 但实际创建用 8!
 * 
 * 影响:
 *   优先级混乱导致系统行为不可预测，难以维护。串口协议任务优先级低于显示任务，
 *   可能丢失数据。
 * 
 * 修复建议:
 *   统一使用定义的常量:
 *   - PROTOCOL_PRIO: 4 (高于显示任务)
 *   - LCD_TASK_PRIO: 8
 *   - MPU6050_TASK_READ_PRIO: 6 (较高，用于传感器读取)
 *   - MPU6050_TASK_DISPLAY_PRIO: 9 (较低，用于显示)
 */

/*
 ******************************************************************************
 *                          第二部分: 高优先级问题
 ******************************************************************************
 */

/*
 * 问题 #4 [高]: 协议任务使用了非线程安全的 strtok
 * 位置: task.c, Protocol_Task() 函数
 * 
 * 问题描述:
 *   代码使用 strtok() 解析接收到的数据包:
 *   
 *   char *str1 = strtok(rx_buffer, ",");
 *   char *str2 = strtok(NULL, ",");
 *   ...
 * 
 *   strtok() 使用内部静态状态，不是线程安全的。如果其他任务或中断也使用 strtok，
 *   会导致解析出错。
 * 
 * 影响:
 *   随机解析错误、数据损坏、系统不稳定。
 * 
 * 修复建议:
 *   改用 strtok_r()（可重入版本）:
 *   
 *   char *saveptr;
 *   char *str1 = strtok_r(rx_buffer, ",", &saveptr);
 *   char *str2 = strtok_r(NULL, ",", &saveptr);
 *   ...
 */

/*
 * 问题 #5 [高]: OSTaskCreate 缺少错误处理
 * 位置: task.c, start_task() 函数
 * 
 * 问题描述:
 *   所有 OSTaskCreate 调用都没有检查错误码:
 *   
 *   OSTaskCreate(&Protocol_Task_TCB, ..., &err);
 *   // 没有检查 err == OS_ERR_NONE
 * 
 * 影响:
 *   如果任务创建失败（如内存不足），系统会继续运行但缺少该任务，
 *   导致难以排查的运行时故障。
 * 
 * 修复建议:
 *   在每个 OSTaskCreate 后添加错误检查:
 *   
 *   OSTaskCreate(&Protocol_Task_TCB, ..., &err);
 *   if(err != OS_ERR_NONE) {
 *       printf("创建 Protocol_Task 失败, 错误码=%d\r\n", err);
 *       // 适当处理错误
 *   }
 */

/*
 * 问题 #6 [高]: 潜在的栈溢出风险
 * 位置: task.c, Protocol_Task()
 * 
 * 问题描述:
 *   Protocol_Task 栈大小为 2048 字节，但使用了:
 *   - 局部缓冲区: char rx_buffer[50]
 *   - printf 调用（占用大量栈空间）
 *   - strtok 和其他字符串操作
 *   
 *   栈检查限制设为 10% (PROTOCOL_STK_SIZE / 10 = 204)，
 *   只剩下 204 字节作为水位标记。
 * 
 * 影响:
 *   串口数据量大时存在栈溢出风险。
 * 
 * 修复建议:
 *   - 将 PROTOCOL_STK_SIZE 增加到 2560 或 3072
 *   - 或减少任务中的 printf 使用
 *   - 使用 OSStatTask 监控实际栈使用情况
 */

/*
 * 问题 #7 [高]: USART 队列消息大小设置不正确
 * 位置: usart.c, USART1_IRQHandler()
 * 
 * 问题描述:
 *   代码将字节发送到队列:
 *   
 *   void *p_msg = (void *)(uintptr_t)data_rx;
 *   OSQPost(&USART_Rx_Queue, p_msg, 0, OS_OPT_POST_FIFO, &err);
 *   
 *   msg_size 参数为 0，但接收任务使用:
 *   p_msg = OSQPend(&USART_Rx_Queue, ..., &msg_size, &ts, &err);
 *   
 *   技术上指针传递是正确的，但容易引起混淆。
 * 
 * 影响:
 *   代码可读性问题。msg_size 始终为 0 或 sizeof(void*)，不是实际的字节值。
 * 
 * 修复建议:
 *   考虑使用实际大小的 OSQPost() 或清晰记录设计意图。
 *   更好的方法: 使用合适的消息结构体:
 *   
 *   typedef struct {
 *       uint8_t data;
 *   } UsartMsg_t;
 */

/*
 ******************************************************************************
 *                          第三部分: 中优先级问题
 ******************************************************************************
 */

/*
 * 问题 #8 [中]: 全局变量缺少 volatile 修饰
 * 位置: task.c
 * 
 * 问题描述:
 *   全局 PID 参数没有声明为 volatile:
 *   
 *   float p=0.0;
 *   float i=0.0;
 *   float d=0.0;
 *   
 *   这些变量在 Protocol_Task 中修改，在 LCD_Sensor_Task 中读取。
 *   没有 volatile 修饰，编译器优化可能导致问题。
 * 
 * 影响:
 *   潜在的优化问题，寄存器中可能存在旧值。
 * 
 * 修复建议:
 *   声明为 volatile:
 *   
 *   volatile float p = 0.0f;
 *   volatile float i = 0.0f;
 *   volatile float d = 0.0f;
 */

/*
 * 问题 #9 [中]: bsp.h 头文件保护不完整
 * 位置: bsp.h
 * 
 * 问题描述:
 *   头文件使用了:
 *   
 *   #ifndef __BSP_H
 *   
 *   但缺少:
 *   
 *   #define __BSP_H
 * 
 * 影响:
 *   头文件保护不能正常工作，可能导致重复包含问题。
 * 
 * 修复建议:
 *   修复头文件保护:
 *   
 *   #ifndef __BSP_H
 *   #define __BSP_H
 *   ...
 *   #endif /* __BSP_H */
 */

/*
 * 问题 #10 [中]: MPU6050 头文件存在重复定义
 * 位置: mpu6050.h
 * 
 * 问题描述:
 *   多个寄存器定义重复:
 *   
 *   #define MPU_SELF_TESTZ_REG      0X0F
 *   #define MPU_SELF_TESTA_REG      0X10
 *   #define MPU_SAMPLE_RATE_REG     0X19
 *   ...
 *   #define MPU_SELF_TESTZ_REG      0X0F  // 重复!
 *   #define MPU_SELF_TESTA_REG      0X10  // 重复!
 *   #define MPU_SAMPLE_RATE_REG     0X19  // 重复!
 * 
 * 影响:
 *   编译器警告，潜在的定义冲突。
 * 
 * 修复建议:
 *   删除重复的定义。
 */

/*
 * 问题 #11 [中]: LCD_Fill 调用时宽高参数颠倒
 * 位置: task.c, Task_MPU6050_LCD_Display()
 * 
 * 问题描述:
 *   LCD_Fill(0, 0, LCD_HEIGHT, LCD_WIDTH, WHITE);
 *   
 *   根据 lcd.h:
 *   - LCD_WIDTH = 160
 *   - LCD_HEIGHT = 128
 *   
 *   正确的调用应该是 LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, WHITE)
 *   或 LCD_Fill(0, 0, LCD_HEIGHT-1, LCD_WIDTH-1, WHITE) 作为坐标值。
 * 
 * 影响:
 *   清屏不正确，可能导致越界访问。
 * 
 * 修复建议:
 *   修复参数顺序:
 *   LCD_Fill(0, 0, LCD_WIDTH-1, LCD_HEIGHT-1, WHITE);
 *   或检查 LCD_Fill 函数签名确保正确使用。
 */

/*
 * 问题 #12 [中]: 硬编码的延时值
 * 位置: task.c, 多处
 * 
 * 问题描述:
 *   多处使用硬编码的延时值:
 *   - OSTimeDly(200, ...)  // 200ms
 *   - OSTimeDly(10, ...)   // 10ms
 *   - OSTimeDly(100, ...)  // 100ms
 *   - OSTimeDly(300, ...)  // 300ms
 *   
 *   这些出现在多个任务中且没有文档说明。
 * 
 * 影响:
 *   代码难以维护和理解。改变时钟节拍率会破坏时序。
 * 
 * 修复建议:
 *   定义常量:
 *   
 *   #define MPU6050_INIT_DELAY_MS    200
 *   #define MPU6050_READ_PERIOD_MS   10
 *   #define DISPLAY_UPDATE_PERIOD_MS 100
 *   
 *   然后使用: OSTimeDlyHMSM(0, 0, 0, MPU6050_READ_PERIOD_MS, ...)
 */

/*
 ******************************************************************************
 *                          第四部分: 低优先级问题
 ******************************************************************************
 */

/*
 * 问题 #13 [低]: 注释风格不一致
 * 位置: 多个文件
 * 
 * 问题描述:
 *   中英文注释混用，格式不统一。
 *   有些文件使用英文注释，有些是乱码的中文。
 * 
 * 影响:
 *   代码可读性，维护困难。
 * 
 * 修复建议:
 *   生产代码统一使用英文注释。
 */

/*
 * 问题 #14 [低]: 创建了未使用的队列 MPU6050_Data_Queue
 * 位置: mpu6050.c
 * 
 * 问题描述:
 *   创建了 MPU6050_Data_Queue 但从未使用。代码实际使用互斥锁+信号量模式。
 * 
 * 影响:
 *   浪费内存（分配了 10 个消息槽）。
 * 
 * 修复建议:
 *   选择:
 *   A) 删除未使用的队列创建
 *   B) 用队列模式替代互斥锁+信号量（推荐，设计更简洁）
 */

/*
 * 问题 #15 [低]: printf 在中断上下文中的风险
 * 位置: usart.c, fputc()
 * 
 * 问题描述:
 *   fputc 使用互斥锁可能阻塞。如果在中断中调用 printf（不应该这样做），
 *   可能导致死锁。
 * 
 * 影响:
 *   误用时可能导致死锁。
 * 
 * 修复建议:
 *   添加警告注释:
 *   
 *   /* 警告: 不要在中断服务程序中调用 printf! */
 *   
 *   或实现中断安全的 printf 版本。
 */

/*
 ******************************************************************************
 *                          第五部分: 改进建议
 ******************************************************************************
 */

/*
 * 建议 #1: 实现合适的任务同步机制
 * 
 * 当前模式（互斥锁 + 信号量）:
 *   - Task_MPU6050_Read: 获取互斥锁 -> 更新数据 -> 释放互斥锁 -> 发送信号量
 *   - Task_MPU6050_Display: 等待信号量 -> 获取互斥锁 -> 读取数据 -> 释放互斥锁
 * 
 * 更好的模式（消息队列）:
 *   - Task_MPU6050_Read: OSQPost(&queue, &data, sizeof(data), ...)
 *   - Task_MPU6050_Display: OSQPend(&queue, ..., &msg_size, ...)
 *   
 * 优点:
 *   - 不需要互斥锁
 *   - 天然支持多个消费者
 *   - 数据被复制，没有共享状态问题
 */

/*
 * 建议 #2: 添加任务栈监控
 * 
 * 在 os_cfg.h 中启用栈检查:
 *   #define OS_CFG_STAT_TASK_STK_CHK_EN    1u
 *   #define OS_CFG_TASK_STK_REDZONE_EN     1u
 * 
 * 然后在监控任务中定期检查:
 *   OS_ERR err;
 *   CPU_STK_SIZE free;
 *   CPU_STK_SIZE used;
 *   OSTaskStkChk(&TaskTCB, &free, &used, &err);
 */

/*
 * 建议 #3: 使用统一的命名规范
 * 
 * 当前不一致的命名:
 *   - start_task vs Task_MPU6050_Read
 *   - LCD_Sensor_Task vs Task_MPU6050_Display
 *   
 * 建议的规范:
 *   - 任务函数: Task_<名称>()
 *   - 任务控制块: <名称>_TCB
 *   - 任务栈: <名称>_Stk[]
 *   - 优先级: <名称>_PRIO
 */

/*
 * 建议 #4: 添加系统初始化验证
 * 
 * 添加函数验证所有 OS 对象是否创建成功:
 * 
 *   static void VerifyOSObjects(void)
 *   {
 *       OS_ERR err;
 *       
 *       // 检查互斥锁
 *       OSMutexPend(&USART_Mutex, 0, OS_OPT_PEND_NON_BLOCKING, NULL, &err);
 *       if(err == OS_ERR_OBJ_TYPE || err == OS_ERR_PEND_ISR) {
 *           printf("USART_Mutex 未初始化!\r\n");
 *       }
 *       
 *       // 其他对象的类似检查...
 *   }
 */

/*
 ******************************************************************************
 *                          第六部分: 修正代码示例
 ******************************************************************************
 */

/*
 * 示例 1: 修复后的任务创建（带错误检查）
 */
#if 0  /* 示例代码 - 不要编译 */

void start_task(void *p_arg)
{
    OS_ERR err;
    
    /* 初始化 CPU 库 */
    CPU_Init();
    
    /* 创建协议任务 */
    OSTaskCreate(&Protocol_Task_TCB,
                 "Protocol_Task",
                 Protocol_Task,
                 NULL,
                 PROTOCOL_PRIO,  /* 使用定义的常量 */
                 &Protocol_Task_STK[0],
                 PROTOCOL_STK_SIZE / 10,
                 PROTOCOL_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err);
    
    if(err != OS_ERR_NONE) {
        printf("错误: 创建 Protocol_Task 失败, 错误码=%d\r\n", err);
        /* 处理错误 - 可能停止或重试 */
    }
    
    /* 创建其他任务... */
    
    /* 删除启动任务（只删除自己） */
    OSTaskDel(NULL, &err);
}

#endif

/*
 * 示例 2: 修复后的协议任务（使用 strtok_r）
 */
#if 0  /* 示例代码 - 不要编译 */

void Protocol_Task(void *p_arg)
{
    OS_ERR err;
    void *p_msg;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    
    uint8_t rx_state = 0;
    char rx_buffer[50];
    uint8_t rx_index = 0;
    
    memset(rx_buffer, 0, sizeof(rx_buffer));
    
    while(1)
    {
        p_msg = OSQPend(&USART_Rx_Queue, 0, OS_OPT_PEND_BLOCKING, 
                        &msg_size, &ts, &err);
        
        if(err == OS_ERR_NONE)
        {
            uint8_t rx_data = (uint8_t)(uintptr_t)p_msg;
            
            switch(rx_state)
            {
                case 0:
                    if(rx_data == '[')
                    {
                        rx_state = 1;
                        rx_index = 0;
                        memset(rx_buffer, 0, sizeof(rx_buffer));
                    }
                    break;
                    
                case 1:
                    if(rx_data == ']')
                    {
                        rx_state = 0;
                        rx_buffer[rx_index] = '\0';
                        
                        /* 使用 strtok_r 替代 strtok */
                        char *saveptr;
                        char *cmd = strtok_r(rx_buffer, ",", &saveptr);
                        char *p_str = strtok_r(NULL, ",", &saveptr);
                        char *i_str = strtok_r(NULL, ",", &saveptr);
                        char *d_str = strtok_r(NULL, ",", &saveptr);
                        
                        if(cmd && strcmp(cmd, "PID") == 0)
                        {
                            if(p_str && i_str && d_str)
                            {
                                p = atof(p_str);
                                i = atof(i_str);
                                d = atof(d_str);
                            }
                        }
                        /* ... */
                    }
                    else if(rx_index < sizeof(rx_buffer) - 1)
                    {
                        rx_buffer[rx_index++] = rx_data;
                    }
                    break;
            }
        }
    }
}

#endif

/*
 * 示例 3: 修复后的 MPU6050 数据共享（使用队列）
 */
#if 0  /* 示例代码 - 不要编译 */

/* 在 mpu6050.c 中 - 移除互斥锁和信号量，仅使用队列 */
OS_Q MPU6050_Data_Queue;

void Task_MPU6050_Read(void *p_arg)
{
    OS_ERR err;
    MPU6050_RawData_t raw;
    MPU6050_Data_t data;
    
    (void)p_arg;
    
    OSTimeDly(200, OS_OPT_TIME_DLY, &err);
    
    if(MPU6050_Init() != 0)
    {
        OSTaskDel(NULL, &err);
        return;
    }
    
    while(1)
    {
        if(MPU6050_Read_Raw_Data(&raw) == 0)
        {
            MPU6050_Process_Data(&raw, &data);
            
            /* 发送到队列 - 数据被复制 */
            OSQPost(&MPU6050_Data_Queue, &data, sizeof(data),
                    OS_OPT_POST_FIFO | OS_OPT_POST_ALL, &err);
        }
        
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

void Task_MPU6050_Display(void *p_arg)
{
    OS_ERR err;
    MPU6050_Data_t data;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    
    (void)p_arg;
    
    while(1)
    {
        /* 直接接收数据 - 不需要互斥锁 */
        void *p_msg = OSQPend(&MPU6050_Data_Queue, 1000, 
                              OS_OPT_PEND_BLOCKING, &msg_size, &ts, &err);
        
        if(err == OS_ERR_NONE && msg_size == sizeof(data))
        {
            memcpy(&data, p_msg, sizeof(data));
            /* 处理并显示数据... */
        }
    }
}

#endif

/*
 ******************************************************************************
 *                               报告结束
 ******************************************************************************
 * 
 * 审查完成于 2026-02-15
 * 建议修复严重和高优先级问题后进行下一次审查。
 * 
 ******************************************************************************
 */

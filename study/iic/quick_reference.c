/**
 ******************************************************************************
 * @file    quick_reference.c
 * @brief   Quick reference for bare-metal to RTOS porting
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 */

/* ==================== 快速参考卡片 ==================== */

/**
 * 1. 延时函数替换对照表
 * =====================
 * 
 * 裸机                          RTOS
 * -------------------------    ----------------------------------
 * delay_ms(10);                OSTimeDly(10, OS_OPT_TIME_DLY, &err);
 * delay_us(100);               volatile uint16_t i=168; while(i--);
 * while(i--);                  volatile uint16_t i=xx; while(i--);
 * for(i=0;i<1000;i++);         OSTimeDly(1, OS_OPT_TIME_DLY, &err);
 * 
 * 注意：
 * - 短延时（<1ms）：使用 volatile 空循环
 * - 长延时（≥1ms）：使用 OSTimeDly()
 * - 必须加 volatile 防止编译器优化
 */

/**
 * 2. 互斥锁使用模板
 * =================
 * 
 * // 声明互斥锁（全局）
 * OS_MUTEX XXX_Mutex;
 * 
 * // 创建互斥锁（初始化时）
 * void XXX_Init(void)
 * {
 *     OS_ERR err;
 *     OSMutexCreate(&XXX_Mutex, "XXX Mutex", &err);
 * }
 * 
 * // 使用互斥锁（操作共享资源时）
 * uint8_t XXX_Operation(void)
 * {
 *     OS_ERR err;
 *     uint8_t result;
 *     
 *     // 获取互斥锁
 *     OSMutexPend(&XXX_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
 *     if(err != OS_ERR_NONE)
 *         return 1;
 *     
 *     // 执行操作
 *     // ...
 *     result = 0;
 *     
 *     // 释放互斥锁
 *     OSMutexPost(&XXX_Mutex, OS_OPT_POST_NONE, &err);
 *     
 *     return result;
 * }
 */

/**
 * 3. 信号量使用模板
 * =================
 * 
 * // 声明信号量（全局）
 * OS_SEM Data_Ready_Sem;
 * 
 * // 创建信号量（初始化时）
 * void Init(void)
 * {
 *     OS_ERR err;
 *     OSSemCreate(&Data_Ready_Sem, "Data Ready", 0, &err);
 * }
 * 
 * // 发送信号（数据生产者）
 * void Task_Producer(void *p_arg)
 * {
 *     OS_ERR err;
 *     while(1)
 *     {
 *         // 生成数据
 *         // ...
 *         
 *         // 通知数据就绪
 *         OSSemPost(&Data_Ready_Sem, OS_OPT_POST_1, &err);
 *         
 *         OSTimeDly(10, OS_OPT_TIME_DLY, &err);
 *     }
 * }
 * 
 * // 等待信号（数据消费者）
 * void Task_Consumer(void *p_arg)
 * {
 *     OS_ERR err;
 *     while(1)
 *     {
 *         // 等待数据就绪（超时1秒）
 *         OSSemPend(&Data_Ready_Sem, 1000, OS_OPT_PEND_BLOCKING, NULL, &err);
 *         
 *         if(err == OS_ERR_NONE)
 *         {
 *             // 处理数据
 *             // ...
 *         }
 *         else
 *         {
 *             // 超时处理
 *             printf("Timeout!\r\n");
 *         }
 *     }
 * }
 */

/**
 * 4. 消息队列使用模板
 * ===================
 * 
 * // 声明队列（全局）
 * OS_Q Data_Queue;
 * 
 * // 创建队列（初始化时）
 * void Init(void)
 * {
 *     OS_ERR err;
 *     OSQCreate(&Data_Queue, "Data Queue", 10, &err);  // 10个消息
 * }
 * 
 * // 发送消息
 * void Task_Sender(void *p_arg)
 * {
 *     OS_ERR err;
 *     uint32_t data = 123;
 *     
 *     OSQPost(&Data_Queue, (void*)data, sizeof(data), 
 *             OS_OPT_POST_FIFO, &err);
 * }
 * 
 * // 接收消息
 * void Task_Receiver(void *p_arg)
 * {
 *     OS_ERR err;
 *     void *p_msg;
 *     OS_MSG_SIZE msg_size;
 *     CPU_TS ts;
 *     
 *     p_msg = OSQPend(&Data_Queue, 1000, OS_OPT_PEND_BLOCKING, 
 *                     &msg_size, &ts, &err);
 *     
 *     if(err == OS_ERR_NONE)
 *     {
 *         uint32_t data = (uint32_t)p_msg;
 *         printf("Received: %lu\r\n", data);
 *     }
 * }
 */

/**
 * 5. 任务创建模板
 * ===============
 * 
 * // 定义任务（在task.c中）
 * #define TASK_XXX_PRIO       10
 * #define TASK_XXX_STK_SIZE   512
 * OS_TCB      Task_XXX_TCB;
 * CPU_STK     Task_XXX_STK[TASK_XXX_STK_SIZE];
 * void Task_XXX(void *p_arg);
 * 
 * // 创建任务（在start_task中）
 * OSTaskCreate(&Task_XXX_TCB, 
 *              "Task_XXX", 
 *              Task_XXX, 
 *              NULL,
 *              TASK_XXX_PRIO,
 *              &Task_XXX_STK[0],
 *              TASK_XXX_STK_SIZE / 10,
 *              TASK_XXX_STK_SIZE,
 *              0, 0, NULL,
 *              OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
 *              &err);
 * 
 * // 任务函数
 * void Task_XXX(void *p_arg)
 * {
 *     OS_ERR err;
 *     (void)p_arg;
 *     
 *     // 初始化（可选）
 *     OSTimeDly(100, OS_OPT_TIME_DLY, &err);
 *     
 *     while(1)
 *     {
 *         // 任务主体
 *         // ...
 *         
 *         // 延时
 *         OSTimeDly(10, OS_OPT_TIME_DLY, &err);
 *     }
 * }
 */

/**
 * 6. 中断处理模板
 * ===============
 * 
 * void XXX_IRQHandler(void)
 * {
 *     OSIntEnter();  // 进入中断
 *     
 *     if(中断标志位)
 *     {
 *         // 读取数据
 *         uint8_t data = XXX_ReceiveData();
 *         
 *         // 发送到队列/信号量
 *         OS_ERR err;
 *         OSQPost(&Queue, (void*)data, 0, OS_OPT_POST_FIFO, &err);
 *         
 *         // 清除中断标志
 *         XXX_ClearFlag();
 *     }
 *     
 *     OSIntExit();  // 退出中断
 * }
 * 
 * 注意：
 * - 中断中不能调用阻塞函数（如OSMutexPend）
 * - 中断中只能调用Post类函数（OSQPost, OSSemPost等）
 * - 中断优先级必须 > OS_CPU_CFG_INT_PRIO_MIN
 */

/**
 * 7. 常见错误对照表
 * =================
 * 
 * 错误代码                     原因                          解决方法
 * -------------------------   ---------------------------   --------------------------
 * OS_ERR_PEND_ISR             在中断中调用Pend函数          改用Post或在任务中处理
 * OS_ERR_MUTEX_OWNER          互斥锁未正确释放              检查Pend/Post配对
 * OS_ERR_Q_FULL               队列已满                      增加队列大小或加快消费
 * OS_ERR_TIMEOUT              等待超时                      检查信号源或增加超时时间
 * OS_ERR_TASK_STK_CHK_ERR     栈溢出                        增加任务栈大小
 */

/**
 * 8. 优先级分配建议
 * =================
 * 
 * 优先级  任务类型              示例
 * ------  -------------------  ---------------------------
 * 0-5     系统关键任务          中断后处理、实时控制
 * 6-10    高优先级任务          传感器读取、通信接收
 * 11-15   中优先级任务          数据处理、算法计算
 * 16-20   低优先级任务          显示、日志、统计
 * 21-30   后台任务              空闲时处理、低频任务
 * 
 * 注意：
 * - 数字越小优先级越高
 * - 相同优先级使用时间片轮转
 * - 避免优先级反转问题
 */

/**
 * 9. 任务栈大小建议
 * =================
 * 
 * 任务类型              栈大小      说明
 * -------------------  ----------  ---------------------------
 * 简单任务（无printf）  256-512    只做简单操作
 * 一般任务              512-1024   有局部变量、函数调用
 * 复杂任务              1024-2048  有浮点运算、大数组
 * 使用printf的任务      ≥1024      printf栈消耗大
 * 
 * 注意：
 * - 栈溢出会导致系统崩溃
 * - 使用OS_OPT_TASK_STK_CHK检查栈使用情况
 * - 宁可大一点，不要刚好够用
 */

/**
 * 10. 调试技巧
 * ============
 * 
 * 1. 打印任务栈使用情况
 * ---------------------
 * void Task_Monitor(void *p_arg)
 * {
 *     OS_ERR err;
 *     CPU_STK_SIZE free, used;
 *     
 *     while(1)
 *     {
 *         OSTaskStkChk(&Task_XXX_TCB, &free, &used, &err);
 *         printf("Task_XXX: Free=%lu, Used=%lu\r\n", free, used);
 *         
 *         OSTimeDly(1000, OS_OPT_TIME_DLY, &err);
 *     }
 * }
 * 
 * 2. 打印任务运行状态
 * -------------------
 * OS_TCB *p_tcb;
 * p_tcb = OSTaskGetCurrent(&err);
 * printf("Current Task: %s, Prio=%d\r\n", p_tcb->NamePtr, p_tcb->Prio);
 * 
 * 3. 检查互斥锁状态
 * -----------------
 * if(XXX_Mutex.OwnerTCBPtr != NULL)
 * {
 *     printf("Mutex owned by: %s\r\n", XXX_Mutex.OwnerTCBPtr->NamePtr);
 * }
 * 
 * 4. 检查队列状态
 * ---------------
 * printf("Queue: %d/%d\r\n", XXX_Queue.NbrEntries, XXX_Queue.NbrEntriesSize);
 */

/**
 * 11. 性能优化建议
 * ================
 * 
 * 1. 减少任务切换
 *    - 合理设置任务周期
 *    - 避免频繁的Pend/Post
 * 
 * 2. 使用定点运算
 *    - 避免浮点除法
 *    - 使用查表法
 * 
 * 3. 减少printf
 *    - printf很慢，避免在高频任务中使用
 *    - 使用条件打印
 * 
 * 4. 优化中断处理
 *    - 中断中只做必要操作
 *    - 复杂处理放到任务中
 * 
 * 5. 合理使用DMA
 *    - IIC/SPI使用DMA传输
 *    - 减少CPU占用
 */

/**
 * 12. 移植检查清单
 * ================
 * 
 * □ 1. 所有delay_ms()改为OSTimeDly()
 * □ 2. 所有共享资源添加互斥锁保护
 * □ 3. 所有初始化在任务中完成
 * □ 4. 中断优先级正确配置
 * □ 5. 任务栈大小足够
 * □ 6. 任务优先级合理分配
 * □ 7. 错误处理完善
 * □ 8. 测试所有功能
 * □ 9. 长时间稳定性测试
 * □ 10. 性能优化
 */

/* ==================== 结束 ==================== */

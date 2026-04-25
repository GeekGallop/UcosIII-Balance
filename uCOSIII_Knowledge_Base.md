# uC/OS-III 知识库 - 平衡车项目

> 本文档基于 STM32F4 + uC/OS-III 平衡车项目代码分析整理
> 涵盖操作系统基础概念、任务管理、同步机制、通信机制、配置参数等核心知识点

---

## 目录

1. [操作系统基础概念](#1-操作系统基础概念)
2. [uC/OS-III 概述](#2-ucos-iii-概述)
3. [任务管理](#3-任务管理)
4. [任务间同步与通信](#4-任务间同步与通信)
5. [中断管理](#5-中断管理)
6. [系统配置](#6-系统配置)
7. [项目任务架构分析](#7-项目任务架构分析)
8. [关键API速查](#8-关键api速查)

---

## 1. 操作系统基础概念

### 1.1 什么是操作系统 (Operating System)

操作系统是管理计算机硬件与软件资源的系统软件，为应用程序提供统一的服务接口。

```
┌─────────────────────────────────────────┐
│              应用程序层                   │
│    App1    App2    App3    App4         │
├─────────────────────────────────────────┤
│              操作系统层                   │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐   │
│  │ 进程管理 │ │ 内存管理 │ │ 文件系统 │   │
│  └─────────┘ └─────────┘ └─────────┘   │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐   │
│  │ 设备驱动 │ │ 网络协议 │ │ 安全机制 │   │
│  └─────────┘ └─────────┘ └─────────┘   │
├─────────────────────────────────────────┤
│              硬件抽象层                   │
│         HAL / BSP / Board Support       │
├─────────────────────────────────────────┤
│              硬件层                       │
│    CPU    Memory    I/O    Storage      │
└─────────────────────────────────────────┘
```

### 1.2 进程 (Process) 是什么

**进程是操作系统资源分配的基本单位**，是程序的一次执行实例。

#### 进程的特征

| 特征 | 说明 |
|------|------|
| **独立性** | 每个进程拥有独立的地址空间、内存、文件句柄等资源 |
| **动态性** | 进程是程序的执行过程，有生命周期（创建→执行→终止） |
| **并发性** | 多个进程可以在宏观上同时执行 |
| **制约性** | 进程间可能需要同步或互斥访问共享资源 |

#### 进程的组成

```
┌─────────────────────────────────────┐
│           进程控制块 (PCB)           │
│  - 进程ID (PID)                     │
│  - 进程状态                         │
│  - 程序计数器 (PC)                  │
│  - 寄存器状态                       │
│  - 内存地址信息                     │
│  - 打开文件列表                     │
├─────────────────────────────────────┤
│           程序段 (Code)              │
│  - 可执行代码                       │
├─────────────────────────────────────┤
│           数据段 (Data)              │
│  - 全局变量、静态变量                │
├─────────────────────────────────────┤
│           堆栈段 (Stack/Heap)        │
│  - 局部变量、函数调用信息            │
│  - 动态分配的内存                    │
└─────────────────────────────────────┘
```

#### 进程的状态转换

```
                    ┌─────────────┐
         调度运行    │   运行态     │   时间片用完/被抢占
        ┌───────────►│  (Running)  │◄────────────────┐
        │            └──────┬──────┘                 │
        │                   │ I/O请求/等待事件        │
   ┌────┴────┐              ▼                        │
   │  就绪态  │◄──────┌─────────────┐                │
   │ (Ready) │       │   阻塞态     │                │
   └────┬────┘       │  (Blocked)  │────────────────┘
        │ 事件发生    └─────────────┘   I/O完成/事件到达
        └───────────────────┘
```

### 1.3 线程 (Thread) 是什么

**线程是CPU调度的基本单位**，是进程内的执行单元。一个进程可以包含多个线程。

#### 线程 vs 进程

| 对比项 | 进程 (Process) | 线程 (Thread) |
|--------|---------------|---------------|
| **资源占用** | 独立地址空间，资源开销大 | 共享进程资源，开销小 |
| **通信方式** | 需要IPC（管道、共享内存等） | 直接访问共享内存 |
| **切换开销** | 大（需要切换页表） | 小（只需保存寄存器） |
| **安全性** | 进程间隔离，一个崩溃不影响其他 | 线程共享内存，一个崩溃可能影响整个进程 |
| **创建速度** | 慢 | 快 |
| **适用场景** | 需要隔离的应用 | 需要频繁通信的并发任务 |

#### 线程的组成

```
┌─────────────────────────────────────┐
│           线程控制块 (TCB)           │
│  - 线程ID (TID)                     │
│  - 线程状态                         │
│  - 程序计数器 (PC)                  │
│  - 寄存器集合                       │
│  - 栈指针 (SP)                      │
│  - 优先级                           │
└─────────────────────────────────────┘

同一进程内的多个线程共享：
┌─────────────────────────────────────┐
│  - 代码段                           │
│  - 数据段（全局变量）                │
│  - 堆（动态内存）                    │
│  - 打开的文件描述符                  │
└─────────────────────────────────────┘

每个线程私有：
┌─────────────────────────────────────┐
│  - 栈（局部变量、函数调用链）        │
│  - 寄存器状态                       │
│  - 程序计数器                       │
└─────────────────────────────────────┘
```

#### 多线程的优势

1. **响应性**：一个线程阻塞时，其他线程可以继续执行
2. **资源共享**：线程间自然共享进程资源
3. **经济性**：创建和切换开销小于进程
4. **可扩展性**：多核CPU上真正并行执行

### 1.4 嵌入式系统中的任务 (Task)

在嵌入式实时操作系统（如 uC/OS-III）中，**任务 (Task)** 类似于线程的概念：

```
┌─────────────────────────────────────────┐
│              嵌入式系统                  │
│                                         │
│  ┌─────────────────────────────────┐   │
│  │         单个程序 (固件)           │   │
│  │                                 │   │
│  │  ┌─────┐ ┌─────┐ ┌─────┐       │   │
│  │  │任务1│ │任务2│ │任务3│  ...  │   │
│  │  │(TCB)│ │(TCB)│ │(TCB)│       │   │
│  │  └──┬──┘ └──┬──┘ └──┬──┘       │   │
│  │     │       │       │           │   │
│  │  ┌──┴──┐ ┌──┴──┐ ┌──┴──┐       │   │
│  │  │ 栈1 │ │ 栈2 │ │ 栈3 │       │   │
│  │  └─────┘ └─────┘ └─────┘       │   │
│  │                                 │   │
│  │  共享：全局变量、外设、中断      │   │
│  └─────────────────────────────────┘   │
│                                         │
│  注意：嵌入式系统通常只有一个"进程"      │
│       即整个固件程序                    │
└─────────────────────────────────────────┘
```

### 1.5 线程/任务间通信 (Inter-Thread Communication)

#### 为什么需要通信

```
任务A (生产者)              任务B (消费者)
    │                          │
    │ 生成数据                  │ 需要数据
    │                          │
    └───────────┬──────────────┘
                │
         ┌──────┴──────┐
         │  需要通信机制  │
         │  传递数据/同步 │
         └─────────────┘
```

#### 通信方式分类

| 通信方式 | 用途 | 典型场景 |
|----------|------|----------|
| **共享内存** | 直接数据交换 | 大数据量共享 |
| **消息传递** | 数据+通知 | 任务间解耦 |
| **信号/事件** | 同步通知 | 事件触发 |
| **管道/队列** | 数据流 | 生产者-消费者 |

#### 1.5.1 共享内存 + 同步机制

```c
// ========== 共享内存示例 ==========
// 共享数据（全局变量）
int shared_counter = 0;

// 任务A：增加计数器
void Task_A(void) {
    while (1) {
        // 访问共享数据
        shared_counter++;
        
        delay(100);
    }
}

// 任务B：读取计数器
void Task_B(void) {
    while (1) {
        // 访问共享数据
        printf("Counter: %d\n", shared_counter);
        
        delay(200);
    }
}
```

**问题**：没有同步机制，可能导致数据竞争 (Race Condition)！

#### 1.5.2 互斥访问 (Mutual Exclusion)

```c
// ========== 使用互斥锁保护共享数据 ==========
OS_MUTEX counter_mutex;  // 互斥锁
int shared_counter = 0;  // 共享数据

void Task_A(void) {
    OS_ERR err;
    while (1) {
        // 请求锁
        OSMutexPend(&counter_mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        // ========== 临界区开始 ==========
        shared_counter++;
        // ========== 临界区结束 ==========
        
        // 释放锁
        OSMutexPost(&counter_mutex, OS_OPT_POST_NONE, &err);
        
        OSTimeDly(100, OS_OPT_TIME_DLY, &err);
    }
}

void Task_B(void) {
    OS_ERR err;
    while (1) {
        // 请求锁
        OSMutexPend(&counter_mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        // ========== 临界区开始 ==========
        printf("Counter: %d\n", shared_counter);
        // ========== 临界区结束 ==========
        
        // 释放锁
        OSMutexPost(&counter_mutex, OS_OPT_POST_NONE, &err);
        
        OSTimeDly(200, OS_OPT_TIME_DLY, &err);
    }
}
```

#### 1.5.3 消息队列 (Message Queue)

```c
// ========== 使用消息队列传递数据 ==========
OS_Q data_queue;  // 消息队列

typedef struct {
    int sensor_id;
    float value;
    uint32_t timestamp;
} Sensor_Data_t;

// 生产者任务
void Sensor_Task(void) {
    OS_ERR err;
    Sensor_Data_t data;
    
    while (1) {
        // 读取传感器
        data.sensor_id = 1;
        data.value = read_sensor();
        data.timestamp = get_time();
        
        // 发送数据到队列
        OSQPost(&data_queue, &data, sizeof(data), 
                OS_OPT_POST_FIFO, &err);
        
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

// 消费者任务
void Process_Task(void) {
    OS_ERR err;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    Sensor_Data_t *p_data;
    
    while (1) {
        // 等待接收数据（阻塞）
        p_data = OSQPend(&data_queue, 0, OS_OPT_PEND_BLOCKING,
                        &msg_size, &ts, &err);
        
        if (err == OS_ERR_NONE) {
            // 处理数据
            printf("Sensor %d: %.2f\n", 
                   p_data->sensor_id, p_data->value);
        }
    }
}
```

#### 1.5.4 信号量 (Semaphore)

```c
// ========== 使用信号量进行同步 ==========
OS_SEM data_ready_sem;  // 信号量
int shared_data = 0;    // 共享数据

// 生产者任务
void Producer_Task(void) {
    OS_ERR err;
    
    while (1) {
        // 生产数据
        shared_data = produce_data();
        
        // 发送信号：数据已准备好
        OSSemPost(&data_ready_sem, OS_OPT_POST_1, &err);
        
        OSTimeDly(100, OS_OPT_TIME_DLY, &err);
    }
}

// 消费者任务
void Consumer_Task(void) {
    OS_ERR err;
    CPU_TS ts;
    
    while (1) {
        // 等待信号
        OSSemPend(&data_ready_sem, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
        
        // 消费数据
        consume_data(shared_data);
    }
}
```

### 1.6 同步 vs 互斥

| 概念 | 目的 | 类比 |
|------|------|------|
| **互斥 (Mutual Exclusion)** | 保护共享资源，防止同时访问 | 厕所门锁（一次只能一个人） |
| **同步 (Synchronization)** | 协调执行顺序，等待特定条件 | 交通信号灯（按顺序通行） |

```
互斥示例（保护共享资源）：
┌─────────┐              ┌─────────┐
│  任务A   │              │  任务B   │
│  写数据  │◄────互斥────►│  读数据  │
└────┬────┘              └────┬────┘
     │                        │
     └────────┬───────────────┘
              ▼
        ┌───────────┐
        │  共享数据  │
        └───────────┘

同步示例（协调执行顺序）：
┌─────────┐              ┌─────────┐
│  任务A   │              │  任务B   │
│ 生产者  │───信号量────►│ 消费者  │
│ 生产数据 │              │ 等待数据 │
└─────────┘              └─────────┘
```

### 1.7 实时操作系统 (RTOS) 特点

#### 实时性的含义

| 类型 | 定义 | 示例 |
|------|------|------|
| **硬实时 (Hard Real-Time)** | 必须在截止时间内完成，否则系统失效 | 汽车安全气囊、飞机控制 |
| **软实时 (Soft Real-Time)** | 偶尔超时可以接受，性能下降 | 视频播放、网络通信 |

#### RTOS vs 通用OS

| 特性 | RTOS (如 uC/OS-III) | 通用OS (如 Linux/Windows) |
|------|---------------------|---------------------------|
| **调度策略** | 优先级抢占调度 | 时间片轮转 + 优先级 |
| **确定性** | 响应时间确定 | 响应时间不确定 |
| **内核大小** | 小巧（几KB到几十KB） | 庞大（MB级） |
| **中断延迟** | 极短（微秒级） | 较长（毫秒级） |
| **内存占用** | 极小 | 较大 |
| **适用场景** | 嵌入式控制 | 通用计算 |

---

## 2. uC/OS-III 概述

### 2.1 什么是 uC/OS-III

uC/OS-III (Micro-Controller Operating Systems Version 3) 是 Micrium 公司开发的抢占式实时操作系统内核，特点：

- **抢占式多任务调度**：支持优先级抢占，最高优先级就绪任务立即执行
- **时间片轮转调度**：同优先级任务可按时间片轮转执行
- **可裁剪配置**：通过 `os_cfg.h` 配置所需功能，减小代码体积
- **确定性**：所有系统调用执行时间可预测
- **丰富的同步机制**：信号量、互斥锁、消息队列、事件标志组等

### 2.2 核心组件

```
┌─────────────────────────────────────────┐
│           应用程序 (Application)          │
├─────────────────────────────────────────┤
│  任务管理  │  时间管理  │  内存管理       │
├─────────────────────────────────────────┤
│  信号量   │  互斥锁   │  消息队列        │
├─────────────────────────────────────────┤
│  事件标志  │  定时器   │  任务信号量/队列  │
├─────────────────────────────────────────┤
│           内核层 (Kernel)               │
│  调度器 │ 就绪列表 │ 等待列表 │ 时钟节拍  │
├─────────────────────────────────────────┤
│        移植层 (Port) - 硬件相关          │
│     上下文切换 │ 中断管理 │ 时钟节拍      │
├─────────────────────────────────────────┤
│           硬件层 (Hardware)              │
└─────────────────────────────────────────┘
```

### 2.3 启动流程

```c
// 标准启动流程
int main(void)
{
    OS_ERR err;
    
    // 1. 初始化 uC/OS-III
    OSInit(&err);
    
    // 2. 板级初始化（外设、中断等）
    bsp_init();
    
    // 3. 创建初始任务（通常创建一个启动任务，再创建其他任务）
    app_start();  // 内部调用 OSTaskCreate() 创建启动任务
    
    // 4. 启动多任务调度（永不返回）
    OSStart(&err);
    
    // 理论上不会执行到这里
    return 0;
}
```

---

## 3. 任务管理

### 3.1 任务基础概念

#### 任务状态机

```
                    ┌─────────────┐
                    │   就绪态     │◄────────────────────────┐
                    │  (Ready)    │                         │
                    └──────┬──────┘                         │
                           │ 被调度器选中                      │
                           ▼                                │
┌─────────┐         ┌─────────────┐         ┌─────────┐     │
│  休眠态  │◄───────│   运行态     │────────►│  等待态  │─────┘
│(Dormant)│ 删除    │  (Running)   │  等待   │(Pending)│ 事件发生
└─────────┘         └─────────────┘         └─────────┘
                           │
                           │ 延时
                           ▼
                    ┌─────────────┐
                    │   延时态     │
                    │  (Delayed)   │
                    └─────────────┘
```

#### 任务控制块 (OS_TCB)

```c
// 任务控制块 - 存储任务的所有状态信息
OS_TCB  Task_TCB;           // 任务控制块
CPU_STK Task_STK[512];      // 任务栈空间

// 任务创建参数
#define TASK_PRIO       5       // 任务优先级（数值越小优先级越高）
#define TASK_STK_SIZE   512     // 任务栈大小（字为单位）
```

### 3.2 任务创建

#### 完整创建示例

```c
#include "os.h"

/* ========== 任务定义 ========== */
#define TASK_PRIO       5
#define TASK_STK_SIZE   512

OS_TCB      Task_TCB;                   // 任务控制块
CPU_STK     Task_STK[TASK_STK_SIZE];    // 任务栈

void Task_Function(void *p_arg);        // 任务函数声明

/* ========== 任务创建 ========== */
void create_task(void)
{
    OS_ERR err;
    
    OSTaskCreate(
        &Task_TCB,                          // 任务控制块指针
        "Task Name",                        // 任务名称（调试用）
        Task_Function,                      // 任务函数指针
        NULL,                               // 传递给任务的参数
        TASK_PRIO,                          // 任务优先级
        &Task_STK[0],                       // 栈底指针
        TASK_STK_SIZE / 10,                 // 栈限制水位（10%）
        TASK_STK_SIZE,                      // 栈总大小
        0,                                  // 消息队列大小（0=不使用）
        0,                                  // 时间片（0=默认）
        NULL,                               // 扩展指针
        OS_OPT_TASK_STK_CHK |               // 选项：启用栈检查
        OS_OPT_TASK_STK_CLR |               // 选项：清零栈
        OS_OPT_TASK_SAVE_FP,                // 选项：保存浮点寄存器
        &err                                // 错误码返回
    );
    
    if (err != OS_ERR_NONE) {
        // 错误处理
    }
}

/* ========== 任务函数 ========== */
void Task_Function(void *p_arg)
{
    (void)p_arg;    // 避免未使用参数警告
    
    // 任务初始化代码
    
    while (1) {
        // 任务主体代码
        
        // 必须包含让出CPU的操作，如延时、等待信号量等
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);  // 延时10个时钟节拍
    }
}
```

#### 创建选项详解

| 选项宏 | 值 | 说明 |
|--------|-----|------|
| `OS_OPT_TASK_NONE` | 0x0000 | 无特殊选项 |
| `OS_OPT_TASK_STK_CHK` | 0x0001 | 启用栈检查 |
| `OS_OPT_TASK_STK_CLR` | 0x0002 | 创建时清零栈 |
| `OS_OPT_TASK_SAVE_FP` | 0x0004 | 保存浮点寄存器（FPU任务必需） |
| `OS_OPT_TASK_NO_TLS` | 0x0008 | 不使用线程本地存储 |

### 3.3 任务优先级

#### 优先级规则

```c
// uC/OS-III 优先级规则
#define OS_CFG_PRIO_MAX     32      // 最大优先级数（配置决定）

// 优先级数值越小，优先级越高
#define START_TASK_PRIO     2       // 启动任务 - 高优先级
#define ENCODER_TASK_PRIO   4       // 编码器任务 - 次高优先级
#define PID_TASK_PRIO       5       // PID控制任务
#define PROTOCOL_PRIO       7       // 协议解析任务
#define MPU6050_TASK_PRIO   8       // MPU6050任务
#define LCD_TASK_PRIO       10      // LCD显示任务 - 低优先级
#define MONITOR_TASK_PRIO   20      // 监控任务 - 最低优先级
```

#### 本项目任务优先级分配

| 任务 | 优先级 | 周期/触发方式 | 说明 |
|------|--------|--------------|------|
| Start Task | 2 | 一次性 | 系统启动任务，创建其他任务后删除 |
| Encoder Task | 4 | 10ms | 编码器速度测量，高实时性 |
| PID Task | 5 | 10ms | PID控制算法，需要及时响应 |
| Protocol Task | 7 | 消息触发 | UART协议解析，处理上位机命令 |
| MPU6050 Task | 8 | 10ms | 姿态传感器读取 |
| LCD Task | 10 | 50ms | LCD显示更新，低实时性要求 |
| Monitor Task | 20 | 1000ms | 系统监控，最低优先级 |

### 3.4 任务延时

```c
// 相对延时 - 从当前时间开始延时
OSTimeDly(ticks, OS_OPT_TIME_DLY, &err);

// 绝对延时 - 配合循环使用，保持固定周期
OSTimeDlyHMSM(0, 0, 0, 10,           // 时、分、秒、毫秒
              OS_OPT_TIME_HMSM_STRICT,  // 严格模式
              &err);

// 延时示例：100Hz采样周期
void Task_Sample(void *p_arg)
{
    OS_ERR err;
    
    while (1) {
        // 执行采样
        Sample_Data();
        
        // 延时10ms = 100Hz
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}
```

### 3.5 任务删除

```c
// 删除自身任务
OSTaskDel(NULL, &err);

// 删除指定任务
OSTaskDel(&Task_TCB, &err);
```

### 3.6 时间片轮转调度

```c
// 启用时间片轮转调度
void start_task(void *p_arg)
{
    OS_ERR err;
    
    // ... 初始化代码 ...
    
    // 启用时间片轮转，时间片为10个时钟节拍
    OSSchedRoundRobinCfg(OS_TRUE, 10, &err);
    
    // 创建同优先级任务时，它们将按时间片轮转执行
    
    // 删除启动任务
    OSTaskDel(NULL, &err);
}
```

---

## 4. 任务间同步与通信

### 4.1 消息队列 (Message Queue)

#### 消息队列原理

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  发送任务    │────►│   消息队列   │────►│  接收任务    │
│  (Post)     │     │   (OS_Q)    │     │  (Pend)     │
└─────────────┘     └─────────────┘     └─────────────┘
                           │
                    ┌──────┴──────┐
                    ▼             ▼
              ┌─────────┐   ┌─────────┐
              │ 消息1   │   │ 消息2   │
              └─────────┘   └─────────┘
```

#### 消息队列创建与使用

```c
#include "os.h"

// 1. 定义消息队列
OS_Q     My_Queue;          // 消息队列对象

// 2. 创建消息队列
void Queue_Init(void)
{
    OS_ERR err;
    
    OSQCreate(&My_Queue,            // 队列指针
              "Queue Name",         // 队列名称
              10,                   // 队列深度（最大消息数）
              &err);                // 错误码
}

// 3. 发送消息（非阻塞）
void Send_Message(void *p_msg, OS_MSG_SIZE msg_size)
{
    OS_ERR err;
    
    OSQPost(&My_Queue,              // 队列指针
            p_msg,                  // 消息指针
            msg_size,               // 消息大小
            OS_OPT_POST_FIFO |      // FIFO模式（先入先出）
            OS_OPT_POST_NO_SCHED,   // 不立即调度
            &err);
}

// 4. 接收消息（阻塞）
void Receive_Message(void)
{
    OS_ERR err;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    void *p_msg;
    
    // 无限等待消息
    p_msg = OSQPend(&My_Queue,              // 队列指针
                    0,                      // 超时时间（0=无限等待）
                    OS_OPT_PEND_BLOCKING,   // 阻塞模式
                    &msg_size,              // 返回消息大小
                    &ts,                    // 时间戳
                    &err);                  // 错误码
    
    if (err == OS_ERR_NONE) {
        // 处理接收到的消息
        Process_Message(p_msg, msg_size);
    }
}
```

#### 本项目消息队列应用

```c
// ========== USART 接收队列 ==========
OS_Q  USART_Rx_Queue;       // UART接收队列

// 中断中发送字节到队列
void USART1_IRQHandler(void)
{
    OSIntEnter();
    OS_ERR err;
    
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
        uint8_t data_rx = USART_ReceiveData(USART1);
        void *p_msg = (void *)(uintptr_t)data_rx;
        
        // 中断中发送消息到队列
        OSQPost(&USART_Rx_Queue, p_msg, 0, OS_OPT_POST_FIFO, &err);
        
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
    OSIntExit();
}

// 任务中接收处理
void Protocol_Task(void *p_arg)
{
    OS_ERR err;
    void *p_msg;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    
    while (1) {
        // 阻塞等待消息
        p_msg = OSQPend(&USART_Rx_Queue, 0, OS_OPT_PEND_BLOCKING, 
                        &msg_size, &ts, &err);
        
        if (err == OS_ERR_NONE) {
            uint8_t rx_data = (uint8_t)(uintptr_t)p_msg;
            // 处理接收到的字节...
        }
    }
}
```

#### 多队列设计模式

```c
// ========== LCD显示任务使用双队列接收不同数据源 ==========
OS_Q g_mpu_display_queue;       // MPU6050数据队列
OS_Q g_encoder_display_queue;   // 编码器数据队列

// LCD任务轮询两个队列
void Task_LCD_Display(void *p_arg)
{
    OS_ERR err;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    void *p_msg;
    
    while (1) {
        // 非阻塞检查MPU队列
        do {
            p_msg = OSQPend(&g_mpu_display_queue, 0, OS_OPT_PEND_NON_BLOCKING,
                           &msg_size, &ts, &err);
            if (err == OS_ERR_NONE) {
                Process_MPU_Data(p_msg);
            }
        } while (err == OS_ERR_NONE);
        
        // 非阻塞检查Encoder队列
        do {
            p_msg = OSQPend(&g_encoder_display_queue, 0, OS_OPT_PEND_NON_BLOCKING,
                           &msg_size, &ts, &err);
            if (err == OS_ERR_NONE) {
                Process_Encoder_Data(p_msg);
            }
        } while (err == OS_ERR_NONE);
        
        // 更新显示
        LCD_Update();
        
        OSTimeDly(50, OS_OPT_TIME_DLY, &err);  // 20Hz刷新
    }
}
```

### 4.2 互斥锁 (Mutex)

#### 互斥锁原理

```
┌─────────┐                    ┌─────────┐
│  任务A   │◄───── 锁定 ──────►│  互斥锁  │
│ (访问中) │                    │ (Mutex) │
└─────────┘                    └────┬────┘
                                    │
                              ┌─────┴─────┐
                              │  共享资源  │
                              │ (临界区)  │
                              └───────────┘
```

#### 互斥锁使用

```c
#include "os.h"

OS_MUTEX  My_Mutex;         // 互斥锁对象

// 1. 创建互斥锁
void Mutex_Init(void)
{
    OS_ERR err;
    OSMutexCreate(&My_Mutex, "Mutex Name", &err);
}

// 2. 使用互斥锁保护临界区
void Access_Shared_Resource(void)
{
    OS_ERR err;
    
    // 请求互斥锁（阻塞等待）
    OSMutexPend(&My_Mutex,          // 互斥锁指针
                0,                  // 超时时间（0=无限等待）
                OS_OPT_PEND_BLOCKING, // 阻塞模式
                NULL,               // 时间戳（可选）
                &err);              // 错误码
    
    // ========== 临界区开始 ==========
    // 访问共享资源
    g_shared_data.value = new_value;
    // ========== 临界区结束 ==========
    
    // 释放互斥锁
    OSMutexPost(&My_Mutex, OS_OPT_POST_NONE, &err);
}
```

#### 本项目互斥锁应用

```c
// ========== MPU6050数据保护 ==========
OS_MUTEX MPU6050_Mutex;
MPU6050_Data_t g_mpu6050_data = {0};  // 全局共享数据

// 写入任务
void Task_MPU6050_Read(void *p_arg)
{
    OS_ERR err;
    
    while (1) {
        // 读取传感器数据
        MPU6050_Read_Data(&raw_data);
        
        // 用互斥锁保护全局数据更新
        OSMutexPend(&MPU6050_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        g_mpu6050_data = processed_data;  // 更新共享数据
        OSMutexPost(&MPU6050_Mutex, OS_OPT_POST_NONE, &err);
        
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

// 读取任务
void Task_Read_MPU_Data(void)
{
    OS_ERR err;
    MPU6050_Data_t local_copy;
    
    // 用互斥锁保护读取
    OSMutexPend(&MPU6050_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    local_copy = g_mpu6050_data;  // 复制数据到本地
    OSMutexPost(&MPU6050_Mutex, OS_OPT_POST_NONE, &err);
    
    // 使用本地副本处理，不占用锁
    Process_Data(&local_copy);
}
```

### 4.3 信号量 (Semaphore)

#### 信号量 vs 互斥锁

| 特性 | 信号量 (Semaphore) | 互斥锁 (Mutex) |
|------|-------------------|----------------|
| 用途 | 资源计数、同步 | 互斥访问 |
| 初始值 | 任意非负整数 | 1（解锁状态） |
| 所有权 | 无 | 有（只有持有者能释放） |
| 优先级继承 | 不支持 | 支持 |
| 递归锁定 | 不支持 | 支持（配置后） |

#### 信号量使用

```c
#include "os.h"

OS_SEM  My_Semaphore;

// 1. 创建信号量
void Semaphore_Init(void)
{
    OS_ERR err;
    
    // 初始值为0，表示资源不可用
    OSSemCreate(&My_Semaphore, "Sem Name", 0, &err);
}

// 2. 等待信号量（阻塞）
void Wait_For_Signal(void)
{
    OS_ERR err;
    CPU_TS ts;
    
    OSSemPend(&My_Semaphore,      // 信号量指针
              0,                  // 超时时间
              OS_OPT_PEND_BLOCKING,
              &ts,
              &err);
    
    if (err == OS_ERR_NONE) {
        // 收到信号，继续执行
    }
}

// 3. 发送信号量
void Send_Signal(void)
{
    OS_ERR err;
    
    OSSemPost(&My_Semaphore,      // 信号量指针
              OS_OPT_POST_1 |     // 只唤醒一个等待任务
              OS_OPT_POST_NO_SCHED,
              &err);
}
```

### 4.4 任务信号量 (Task Semaphore)

```c
// 直接向特定任务发送信号
OS_ERR err;

// 发送信号到指定任务
OSTaskSemPost(&Task_TCB, OS_OPT_POST_NONE, &err);

// 在任务中等待信号
OSTaskSemPend(0, OS_OPT_PEND_BLOCKING, NULL, &err);
```

### 4.5 事件标志组 (Event Flags)

```c
#include "os.h"

OS_FLAG_GRP  Event_Flags;

// 定义事件位
#define EVENT_BUTTON_PRESSED    0x01
#define EVENT_DATA_READY        0x02
#define EVENT_TIMEOUT           0x04

// 1. 创建事件标志组
void EventFlags_Init(void)
{
    OS_ERR err;
    OSFlagCreate(&Event_Flags, "Event Flags", 0, &err);
}

// 2. 设置事件标志
void Set_Event(void)
{
    OS_ERR err;
    OSFlagPost(&Event_Flags,            // 事件标志组
               EVENT_DATA_READY,        // 要设置的事件位
               OS_OPT_POST_FLAG_SET,    // 设置标志
               &err);
}

// 3. 等待事件标志
void Wait_For_Events(void)
{
    OS_ERR err;
    CPU_TS ts;
    OS_FLAGS flags;
    
    // 等待多个事件（OR关系）
    flags = OSFlagPend(&Event_Flags,
                       EVENT_BUTTON_PRESSED | EVENT_DATA_READY,
                       0,                               // 超时时间
                       OS_OPT_PEND_FLAG_SET_ANY |       // 任意一个事件触发
                       OS_OPT_PEND_BLOCKING,
                       &ts,
                       &err);
    
    if (err == OS_ERR_NONE) {
        if (flags & EVENT_BUTTON_PRESSED) {
            // 处理按钮事件
        }
        if (flags & EVENT_DATA_READY) {
            // 处理数据就绪事件
        }
    }
}
```

---

## 5. 中断管理

### 5.1 中断与uC/OS-III集成

```c
// ========== 标准中断处理框架 ==========
void USART1_IRQHandler(void)
{
    OS_ERR err;
    
    // 1. 进入中断（通知内核）
    OSIntEnter();
    
    // 2. 中断处理代码
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
        uint8_t data = USART_ReceiveData(USART1);
        
        // 发送消息到队列（唤醒任务处理）
        OSQPost(&USART_Rx_Queue, (void *)(uintptr_t)data, 0, 
                OS_OPT_POST_FIFO, &err);
    }
    
    // 3. 退出中断（可能触发任务切换）
    OSIntExit();
}
```

### 5.2 中断安全API

| 功能 | 任务级API | 中断级API |
|------|----------|----------|
| 发送消息队列 | `OSQPost()` | `OSQPost()` (相同) |
| 发送信号量 | `OSSemPost()` | `OSSemPost()` (相同) |
| 发送任务信号量 | `OSTaskSemPost()` | `OSTaskSemPost()` (相同) |
| 设置事件标志 | `OSFlagPost()` | `OSFlagPost()` (相同) |
| 释放互斥锁 | `OSMutexPost()` | 中断中不可用 |

### 5.3 中断优先级配置

```c
// NVIC配置 - 中断优先级必须高于OS_CPU_CFG_INT_PRIO_MIN
NVIC_InitTypeDef NVIC_InitStruct;

NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;
NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 5;  // 抢占优先级
NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;         // 子优先级
NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
NVIC_Init(&NVIC_InitStruct);
```

---

## 6. 系统配置

### 6.1 os_cfg.h 核心配置

```c
/* ==================== 系统核心配置 ==================== */

// 最大优先级数（决定任务优先级范围：0 ~ OS_CFG_PRIO_MAX-1）
#define OS_CFG_PRIO_MAX                 32u

// 最小任务栈大小
#define OS_CFG_STK_SIZE_MIN             64u

// 启用时间片轮转调度
#define OS_CFG_SCHED_ROUND_ROBIN_EN     1u

/* ==================== 功能模块使能 ==================== */

// 事件标志
#define OS_CFG_FLAG_EN                  1u
#define OS_CFG_FLAG_DEL_EN              1u
#define OS_CFG_FLAG_PEND_ABORT_EN       1u

// 内存管理
#define OS_CFG_MEM_EN                   1u

// 互斥锁
#define OS_CFG_MUTEX_EN                 1u
#define OS_CFG_MUTEX_DEL_EN             1u
#define OS_CFG_MUTEX_PEND_ABORT_EN      1u

// 消息队列
#define OS_CFG_Q_EN                     1u
#define OS_CFG_Q_DEL_EN                 1u
#define OS_CFG_Q_FLUSH_EN               1u
#define OS_CFG_Q_PEND_ABORT_EN          1u

// 信号量
#define OS_CFG_SEM_EN                   1u
#define OS_CFG_SEM_DEL_EN               1u
#define OS_CFG_SEM_PEND_ABORT_EN        1u
#define OS_CFG_SEM_SET_EN               1u

// 统计任务
#define OS_CFG_STAT_TASK_EN             1u
#define OS_CFG_STAT_TASK_STK_CHK_EN     1u

// 任务管理功能
#define OS_CFG_TASK_DEL_EN              1u
#define OS_CFG_TASK_SUSPEND_EN          1u
#define OS_CFG_TASK_Q_EN                1u

// 定时器
#define OS_CFG_TMR_EN                   1u

// 调试功能
#define OS_CFG_DBG_EN                   1u
```

### 6.2 时钟节拍配置

```c
// os_cfg_app.h 中配置
#define OS_CFG_TICK_RATE_HZ             1000u   // 时钟节拍频率 1kHz

// 启动任务中配置SysTick
void start_task(void *p_arg)
{
    OS_ERR err;
    CPU_INT32U cnts;
    RCC_ClocksTypeDef rcc_clocks;
    
    // 获取系统时钟
    RCC_GetClocksFreq(&rcc_clocks);
    
    // 计算SysTick计数值
    cnts = ((CPU_INT32U)rcc_clocks.HCLK_Frequency) / OSCfg_TickRate_Hz;
    
    // 初始化SysTick
    OS_CPU_SysTickInit(cnts);
    
    // ...
}
```

### 6.3 统计任务

```c
// 启用统计任务后，可以获取CPU使用率
void CPU_Usage_Monitor(void)
{
    OS_ERR err;
    
    // 初始化统计任务（在创建其他任务之前调用）
    OSStatTaskCPUUsageInit(&err);
    
    // 获取当前CPU使用率（百分比，放大100倍）
    CPU_INT16U cpu_usage = OSStatTaskCPUUsage;      // 例如：1500 表示 15.00%
    CPU_INT16U cpu_usage_max = OSStatTaskCPUUsageMax;
    
    printf("CPU Usage: %d.%02d%%\n", 
           cpu_usage / 100, cpu_usage % 100);
}
```

### 6.4 栈检查

```c
// 检查任务栈使用情况
void Check_Task_Stack(OS_TCB *p_tcb)
{
    OS_ERR err;
    CPU_STK_SIZE free_stk;
    CPU_STK_SIZE used_stk;
    
    OSTaskStkChk(p_tcb, &free_stk, &used_stk, &err);
    
    if (err == OS_ERR_NONE) {
        CPU_STK_SIZE total_stk = free_stk + used_stk;
        float usage_pct = (float)used_stk / total_stk * 100.0f;
        
        printf("Task: %s\n", p_tcb->NamePtr);
        printf("  Stack: %d/%d (%.1f%%)\n", 
               used_stk, total_stk, usage_pct);
        
        if (usage_pct > 80.0f) {
            printf("WARNING: Stack usage high!\n");
        }
    }
}
```

---

## 7. 项目任务架构分析

### 7.1 系统架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层 (Application)                      │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────┐ │
│  │  Start Task │  │  Protocol   │  │   Encoder   │  │   PID   │ │
│  │   (Prio 2)  │  │   (Prio 7)  │  │   (Prio 4)  │  │(Prio 5) │ │
│  │   一次性    │  │  消息触发   │  │   10ms周期  │  │ 10ms周期│ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────┬────┘ │
│         │                │                │              │      │
│  ┌──────▼──────┐  ┌──────▼──────┐  ┌──────▼──────┐       │      │
│  │   MPU6050   │  │    LCD      │  │   Monitor   │       │      │
│  │   (Prio 8)  │  │   (Prio 10) │  │   (Prio 20) │       │      │
│  │   10ms周期  │  │   50ms周期  │  │  1000ms周期 │       │      │
│  └─────────────┘  └─────────────┘  └─────────────┘       │      │
│                                                          │      │
├──────────────────────────────────────────────────────────┼──────┤
│                      同步/通信层                          │      │
│  ┌─────────────────┐  ┌─────────────────┐               │      │
│  │  USART_Rx_Queue │  │ g_mpu_display_q │               │      │
│  │  (串口接收队列)  │  │ (MPU显示队列)   │               │      │
│  └─────────────────┘  └─────────────────┘               │      │
│  ┌─────────────────┐  ┌─────────────────┐               │      │
│  │g_encoder_disp_q │  │ g_pid_encoder_q │               │      │
│  │(编码器显示队列)  │  │ (PID编码器队列) │               │      │
│  └─────────────────┘  └─────────────────┘               │      │
│  ┌─────────────────┐  ┌─────────────────┐               │      │
│  │  g_pid_mpu_q    │  │  MPU6050_Mutex  │               │      │
│  │  (PID MPU队列)  │  │  (MPU数据互斥锁) │               │      │
│  └─────────────────┘  └─────────────────┘               │      │
│  ┌─────────────────┐                                    │      │
│  │   USART_Mutex   │                                    │      │
│  │  (串口发送互斥锁)│                                    │      │
│  └─────────────────┘                                    │      │
├─────────────────────────────────────────────────────────┼──────┤
│                      驱动层 (Drivers)                    │      │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │      │
│  │  USART  │ │  MPU6050│ │ Encoder │ │  Motor  │       │      │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘       │      │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐                    │      │
│  │   LCD   │ │   I2C   │ │   TIM   │                    │      │
│  └─────────┘ └─────────┘ └─────────┘                    │      │
├─────────────────────────────────────────────────────────┴──────┤
│              uC/OS-III 内核 (Kernel)                           │
│     调度器 │ 就绪列表 │ 等待列表 │ 时钟节拍 │ 中断管理         │
├────────────────────────────────────────────────────────────────┤
│              硬件层 (STM32F4)                                   │
│     Cortex-M4 │ SysTick │ NVIC │ GPIO │ TIM │ USART │ I2C      │
└────────────────────────────────────────────────────────────────┘
```

### 7.2 数据流图

```
┌─────────────────────────────────────────────────────────────────────┐
│                           数据流向                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────┐    中断     ┌──────────────┐                         │
│  │ USART RX │────────────►│ USART_Rx_Queue│────────────────┐       │
│  └──────────┘             └──────────────┘                │       │
│                                                          ▼       │
│  ┌──────────┐    I2C      ┌──────────┐   消息队列    ┌──────────┐ │
│  │ MPU6050  │────────────►│ MPU Task │──────────────►│ LCD Task │ │
│  └──────────┘             └──────────┘               └──────────┘ │
│       │                        │                           ▲      │
│       │                        │ 消息队列                   │      │
│       │                        └───────────────────────────┤      │
│       │                                                    │      │
│       │ 互斥锁                                             │      │
│       ▼                                                    │      │
│  ┌──────────┐   消息队列    ┌──────────┐   消息队列    ┌────┘      │
│  │g_mpu_data│◄─────────────│ PID Task │──────────────►│           │
│  └──────────┘              └──────────┘               │           │
│       ▲                        ▲                      │           │
│       │                        │ 消息队列              │           │
│  ┌────┴────┐              ┌────┴────┐                 │           │
│  │ Encoder │──────────────►│ Encoder │─────────────────┘           │
│  │  TIM    │   消息队列    │  Task   │                             │
│  └─────────┘              └─────────┘                             │
│                                                                     │
│  ┌──────────┐              ┌──────────┐                            │
│  │ Protocol │──────────────►│  Motor   │                            │
│  │  Task    │   设置速度    │  Driver  │                            │
│  └──────────┘              └──────────┘                            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.3 任务详细说明

#### 7.3.1 Start Task (启动任务)

```c
#define START_TASK_PRIO     2
#define START_STK_SIZE      512

void start_task(void *p_arg)
{
    OS_ERR err;
    CPU_INT32U cnts;
    RCC_ClocksTypeDef rcc_clocks;
    
    // 初始化CPU库
    CPU_Init();
    
    // 配置SysTick时钟
    RCC_GetClocksFreq(&rcc_clocks);
    cnts = ((CPU_INT32U)rcc_clocks.HCLK_Frequency) / OSCfg_TickRate_Hz;
    OS_CPU_SysTickInit(cnts);
    
    // 启用时间片轮转调度
    OSSchedRoundRobinCfg(OS_TRUE, 10, &err);
    
    // 创建各个应用任务...
    OSTaskCreate(&Protocol_Task_TCB, ...);
    OSTaskCreate(&LCD_Task_TCB, ...);
    OSTaskCreate(&MPU6050_Task_TCB, ...);
    OSTaskCreate(&Encoder_Task_TCB, ...);
    OSTaskCreate(&PID_Task_TCB, ...);
    
    // 初始化统计任务
    OSStatTaskCPUUsageInit(&err);
    
    // 删除启动任务自身
    OSTaskDel(NULL, &err);
}
```

**知识点**：
- 启动任务负责系统初始化和创建其他任务
- 使用 `OSSchedRoundRobinCfg()` 启用时间片轮转
- 任务完成后应删除自身，释放资源

#### 7.3.2 Protocol Task (协议解析任务)

```c
#define PROTOCOL_PRIO       7
#define PROTOCOL_STK_SIZE   2048

void Protocol_Task(void *p_arg)
{
    OS_ERR err;
    void *p_msg;
    OS_MSG_SIZE msg_size;
    CPU_TS ts;
    
    uint8_t rx_state = 0;
    char rx_buffer[50];
    uint8_t rx_index = 0;
    
    while(1) {
        // 阻塞等待串口数据（无限等待）
        p_msg = OSQPend(&USART_Rx_Queue, 0, OS_OPT_PEND_BLOCKING, 
                        &msg_size, &ts, &err);
        
        if(err == OS_ERR_NONE) {
            uint8_t rx_data = (uint8_t)(uintptr_t)p_msg;
            
            // 状态机解析协议帧 [CMD,PARAM1,PARAM2,...]
            switch(rx_state) {
                case 0: // 等待帧头 '['
                    if(rx_data == '[') {
                        rx_state = 1;
                        rx_index = 0;
                    }
                    break;
                    
                case 1: // 接收数据
                    if(rx_data == ']') {
                        // 帧结束，解析命令
                        rx_buffer[rx_index] = '\0';
                        Parse_Command(rx_buffer);
                        rx_state = 0;
                    } else {
                        rx_buffer[rx_index++] = rx_data;
                    }
                    break;
            }
        }
    }
}
```

**知识点**：
- 使用消息队列实现中断到任务的通信
- 状态机解析串口协议帧
- 支持命令：PID参数设置、LED控制、电机速度设置等

#### 7.3.3 Encoder Task (编码器任务)

```c
#define ENCODER_TASK_PRIO       4
#define ENCODER_TASK_STK_SIZE   2048

void Task_Encoder_Speed(void *p_arg)
{
    OS_ERR err;
    int16_t speed_left, speed_right;
    float rpm_left, rpm_right;
    
    while (1) {
        // 更新编码器计数
        Encoder_UpdateAll();
        
        // 计算速度
        Encoder_Calculate_Speed(&g_encoder_left_data, 10.0f);
        Encoder_Calculate_Speed(&g_encoder_right_data, 10.0f);
        
        // 准备显示数据
        display_data.speed_left = g_encoder_left_data.speed;
        display_data.speed_right = g_encoder_right_data.speed;
        
        // 发送到LCD显示队列（非阻塞）
        LCD_Send_Encoder_Data(&display_data);
        
        // 发送到PID控制队列
        PID_Send_Encoder_Data(&pid_encoder_data);
        
        // 10ms周期 = 100Hz采样
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}
```

**知识点**：
- 周期性任务使用 `OSTimeDly()` 实现固定采样率
- 使用多个消息队列向不同消费者发送数据
- 栈使用监控：定期调用 `OSTaskStkChk()` 检查栈使用率

#### 7.3.4 PID Task (PID控制任务)

```c
#define PID_TASK_PRIO       5
#define PID_TASK_STK_SIZE   2048

void Task_PID_Control(void *p_arg)
{
    OS_ERR err;
    
    // 初始化PID系统（创建消息队列）
    PID_System_Init();
    
    while (1) {
        // 非阻塞轮询编码器数据队列
        PID_Process_Encoder_Queue();
        
        // 非阻塞轮询MPU数据队列
        PID_Process_MPU_Queue();
        
        // 执行PID控制
        PID_Execute_Control();
        
        // 10ms控制周期
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}
```

**知识点**：
- 使用双队列接收不同数据源的数据
- 非阻塞消息接收：`OS_OPT_PEND_NON_BLOCKING`
- PID控制周期与传感器采样周期同步

#### 7.3.5 LCD Task (显示任务)

```c
#define LCD_TASK_PRIO       10
#define LCD_TASK_STK_SIZE   2048

void Task_LCD_Display(void *p_arg)
{
    OS_ERR err;
    
    // 初始化LCD显示系统（创建两个消息队列）
    LCD_Display_Init();
    
    while (1) {
        // 处理MPU队列消息（非阻塞轮询）
        LCD_Process_MPU_Queue();
        
        // 处理Encoder队列消息（非阻塞轮询）
        LCD_Process_Encoder_Queue();
        
        // 更新LCD显示
        LCD_Update_MPU_Display();
        LCD_Update_Encoder_Left_Display();
        
        // 50ms刷新周期 = 20Hz
        OSTimeDly(50, OS_OPT_TIME_DLY, &err);
    }
}
```

**知识点**：
- 使用双队列接收不同数据源的数据
- 显示任务优先级较低，不影响实时控制
- 数据缓存机制：队列消息处理后缓存，定时刷新显示

#### 7.3.6 MPU6050 Task (姿态传感器任务)

```c
#define MPU6050_TASK_PRIO       8
#define MPU6050_TASK_STK_SIZE   1024

void Task_MPU6050_Read(void *p_arg)
{
    OS_ERR err;
    MPU6050_RawData_t raw_data;
    MPU6050_Data_t processed_data;
    MPU6050_Attitude_t attitude_data;
    
    // 延时等待系统初始化
    OSTimeDly(200, OS_OPT_TIME_DLY, &err);
    
    // 初始化MPU6050
    MPU6050_Init();
    
    while (1) {
        // 读取原始数据
        MPU6050_Read_Raw_Data(&raw_data);
        
        // 处理数据（转换为物理量）
        MPU6050_Process_Data(&raw_data, &processed_data);
        
        // 计算姿态角
        MPU6050_Calculate_Attitude(&processed_data, &attitude_data);
        
        // 互斥锁保护全局数据更新
        OSMutexPend(&MPU6050_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        g_mpu6050_data = processed_data;
        OSMutexPost(&MPU6050_Mutex, OS_OPT_POST_NONE, &err);
        
        // 发送到显示队列
        LCD_Send_MPU_Data(&attitude_data, processed_data.temp);
        
        // 发送到PID队列
        PID_Send_MPU_Data(&pid_mpu_data);
        
        // 10ms采样周期
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}
```

**知识点**：
- 使用互斥锁保护全局共享数据
- 延时启动：等待其他系统组件就绪
- 数据多路分发：同时发送到显示和PID控制任务

### 7.4 关键设计模式

#### 7.4.1 中断-任务通信模式

```
中断服务程序                    任务
    │                            │
    │  1. 接收硬件数据            │
    ▼                            │
┌─────────┐                     │
│ 读取寄存器 │                     │
└────┬────┘                     │
     │                           │
     │  2. OSQPost()             │
     ▼                           ▼
┌─────────┐                 ┌─────────┐
│ 消息队列 │◄────────────────│ OSQPend()│
└─────────┘                 └────┬────┘
                                 │
                                 │ 3. 处理数据
                                 ▼
                            ┌─────────┐
                            │ 业务逻辑 │
                            └─────────┘
```

#### 7.4.2 生产者-消费者模式

```
生产者任务(Encoder)            消费者任务(LCD/PID)
        │                            │
        │  OSQPost()                 │ OSQPend()
        ▼                            ▼
   ┌─────────┐                  ┌─────────┐
   │ 队列    │─────────────────►│ 队列    │
   └─────────┘                  └─────────┘
        │                            │
        │  OSQPost()                 │ OSQPend()
        ▼                            ▼
   ┌─────────┐                  ┌─────────┐
   │ 队列    │─────────────────►│ 队列    │
   └─────────┘                  └─────────┘
```

#### 7.4.3 互斥访问模式

```
任务A (写入)                    任务B (读取)
    │                            │
    │ OSMutexPend()              │ OSMutexPend()
    ▼                            ▼
┌─────────┐                  ┌─────────┐
│ 互斥锁  │◄────────────────►│ 互斥锁  │
└────┬────┘                  └────┬────┘
     │                            │
     ▼                            ▼
┌─────────┐                  ┌─────────┐
│ 写共享  │                  │ 读共享  │
│ 数据    │                  │ 数据    │
└────┬────┘                  └────┬────┘
     │                            │
     │ OSMutexPost()              │ OSMutexPost()
     ▼                            ▼
```

---

## 8. 关键API速查

### 8.1 任务管理API

| API | 功能 | 示例 |
|-----|------|------|
| `OSInit()` | 初始化uC/OS-III | `OSInit(&err);` |
| `OSStart()` | 启动调度器 | `OSStart(&err);` |
| `OSTaskCreate()` | 创建任务 | 见3.2节 |
| `OSTaskDel()` | 删除任务 | `OSTaskDel(NULL, &err);` |
| `OSTimeDly()` | 任务延时 | `OSTimeDly(10, OS_OPT_TIME_DLY, &err);` |
| `OSTimeDlyHMSM()` | 时分秒毫秒延时 | `OSTimeDlyHMSM(0,0,0,10,...)` |
| `OSTaskStkChk()` | 检查栈使用 | `OSTaskStkChk(&tcb, &free, &used, &err);` |

### 8.2 消息队列API

| API | 功能 | 示例 |
|-----|------|------|
| `OSQCreate()` | 创建队列 | `OSQCreate(&q, "name", 10, &err);` |
| `OSQPost()` | 发送消息 | `OSQPost(&q, p_msg, size, opt, &err);` |
| `OSQPend()` | 接收消息 | `p_msg = OSQPend(&q, 0, opt, &size, &ts, &err);` |
| `OSQDel()` | 删除队列 | `OSQDel(&q, opt, &err);` |
| `OSQFlush()` | 清空队列 | `OSQFlush(&q, &err);` |

### 8.3 互斥锁API

| API | 功能 | 示例 |
|-----|------|------|
| `OSMutexCreate()` | 创建互斥锁 | `OSMutexCreate(&mutex, "name", &err);` |
| `OSMutexPend()` | 请求互斥锁 | `OSMutexPend(&mutex, 0, opt, NULL, &err);` |
| `OSMutexPost()` | 释放互斥锁 | `OSMutexPost(&mutex, opt, &err);` |
| `OSMutexDel()` | 删除互斥锁 | `OSMutexDel(&mutex, opt, &err);` |

### 8.4 信号量API

| API | 功能 | 示例 |
|-----|------|------|
| `OSSemCreate()` | 创建信号量 | `OSSemCreate(&sem, "name", 0, &err);` |
| `OSSemPend()` | 等待信号量 | `OSSemPend(&sem, 0, opt, &ts, &err);` |
| `OSSemPost()` | 发送信号量 | `OSSemPost(&sem, opt, &err);` |
| `OSSemSet()` | 设置信号量值 | `OSSemSet(&sem, cnt, &err);` |

### 8.5 事件标志API

| API | 功能 | 示例 |
|-----|------|------|
| `OSFlagCreate()` | 创建事件标志组 | `OSFlagCreate(&grp, "name", 0, &err);` |
| `OSFlagPend()` | 等待事件标志 | `flags = OSFlagPend(&grp, mask, to, opt, &ts, &err);` |
| `OSFlagPost()` | 设置/清除事件标志 | `OSFlagPost(&grp, mask, opt, &err);` |

### 8.6 中断管理API

| API | 功能 | 示例 |
|-----|------|------|
| `OSIntEnter()` | 进入中断 | `OSIntEnter();` |
| `OSIntExit()` | 退出中断 | `OSIntExit();` |

### 8.7 统计功能API

| API | 功能 | 示例 |
|-----|------|------|
| `OSStatTaskCPUUsageInit()` | 初始化CPU统计 | `OSStatTaskCPUUsageInit(&err);` |
| `OSStatReset()` | 重置统计 | `OSStatReset(&err);` |

---

## 附录

### A. 错误码参考

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `OS_ERR_NONE` | 0 | 无错误 |
| `OS_ERR_TIMEOUT` | 1 | 超时 |
| `OS_ERR_Q_FULL` | 40 | 队列已满 |
| `OS_ERR_Q_EMPTY` | 41 | 队列为空 |
| `OS_ERR_MUTEX_NOT_OWNER` | 50 | 非互斥锁所有者 |
| `OS_ERR_SEM_OVF` | 60 | 信号量溢出 |
| `OS_ERR_PRIO_EXIST` | 70 | 优先级已存在 |
| `OS_ERR_TASK_CREATE_ISR` | 80 | 在中断中创建任务 |

### B. 推荐任务优先级分配

```
优先级 0-1:    保留（最高优先级，通常不使用）
优先级 2-5:    关键实时任务（控制算法、紧急处理）
优先级 6-10:   重要任务（数据采集、通信）
优先级 11-20:  一般任务（显示、日志）
优先级 21-30:  后台任务（监控、维护）
优先级 31:     空闲任务（Idle Task，内核自动创建）
```

### C. 栈大小推荐

| 任务类型 | 推荐栈大小 | 说明 |
|----------|-----------|------|
| 简单任务 | 256-512 | 无复杂函数调用 |
| 标准任务 | 512-1024 | 一般应用任务 |
| 复杂任务 | 1024-2048 | 使用printf、浮点运算 |
| 中断栈 | 512-1024 | 单独配置 |

---

## D. 面试高频考点

### D.1 任务调度相关问题

#### Q1: uC/OS-III 是什么类型的调度算法？
**A:** uC/OS-III 采用**抢占式优先级调度算法**（Preemptive Priority-Based Scheduling）。

- **抢占式**：高优先级任务可以立即打断低优先级任务的执行
- **优先级调度**：总是执行就绪队列中优先级最高的任务
- **同优先级支持**：支持时间片轮转（Round-Robin）调度同优先级任务

```
调度时机：
1. 任务创建时
2. 任务删除时
3. 任务挂起时
4. 任务延时结束时
5. 中断退出时（OSIntExit）
6. 任务主动放弃CPU（OSTimeDly等）
```

#### Q2: 任务切换的开销有哪些？
**A:** 任务切换（Context Switch）的开销包括：

| 开销项 | 说明 | 典型值 |
|--------|------|--------|
| 保存现场 | 保存当前任务的寄存器、程序计数器、状态字 | 12-20个寄存器 |
| 恢复现场 | 恢复新任务的寄存器、程序计数器、状态字 | 12-20个寄存器 |
| 调度算法 | 查找最高优先级就绪任务 | O(1) - 使用位图 |
| 缓存失效 | 新任务的代码/数据可能不在缓存中 | 视情况而定 |

**优化建议**：
- 减少不必要的任务切换
- 合理设置任务优先级
- 避免频繁创建/删除任务

#### Q3: 什么是优先级反转？如何解决？
**A:** 

**优先级反转（Priority Inversion）**：低优先级任务持有资源，高优先级任务等待，中优先级任务抢占CPU，导致高优先级任务被延迟。

```
场景示例：
时间线 ─────────────────────────────────────────►

高优先级任务H:    │等待MUTEX│───────────────►
                  ↑       ↑
中优先级任务M: ───────────│运行│─────────────►
                          ↑
低优先级任务L: ───│运行│──│持有MUTEX│───────►
```

**解决方案**：

1. **优先级继承（Priority Inheritance）**
   - 当高优先级任务等待低优先级任务持有的互斥锁时，临时提升低优先级任务的优先级
   - uC/OS-III 的互斥锁（Mutex）支持优先级继承

```c
// 使用互斥锁自动启用优先级继承
OSMutexCreate(&MyMutex, "MyMutex", &err);
OSMutexPend(&MyMutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
// 低优先级任务临时提升到高优先级
// ... 临界区代码 ...
OSMutexPost(&MyMutex, OS_OPT_POST_NONE, &err);
// 恢复原始优先级
```

2. **优先级天花板（Priority Ceiling）**
   - 预先设定资源的优先级上限
   - 获取资源时，任务优先级提升到天花板

#### Q4: 时间片轮转是什么？如何配置？
**A:** 

**时间片轮转（Round-Robin Scheduling）**：同优先级任务按时间片轮流执行。

```c
// 启用时间片轮转
void start_task(void *p_arg)
{
    OS_ERR err;
    
    // 参数1: OS_TRUE 启用, OS_FALSE 禁用
    // 参数2: 默认时间片大小（时钟节拍数）
    OSSchedRoundRobinCfg(OS_TRUE, 10, &err);
    
    // 创建同优先级任务
    OSTaskCreate(&Task1_TCB, "Task1", Task1_Func, NULL, 5, ...);
    OSTaskCreate(&Task2_TCB, "Task2", Task2_Func, NULL, 5, ...);
    // Task1 和 Task2 将按10个tick的时间片轮转执行
}
```

### D.2 同步机制相关问题

#### Q5: 互斥锁（Mutex）和信号量（Semaphore）的区别？
**A:**

| 特性 | 互斥锁（Mutex） | 信号量（Semaphore） |
|------|----------------|---------------------|
| **用途** | 互斥访问共享资源 | 资源计数、任务同步 |
| **初始值** | 1（解锁） | 任意非负整数 |
| **所有权** | 有（只有持有者能释放） | 无 |
| **优先级继承** | 支持 | 不支持 |
| **递归锁定** | 支持（配置后） | 不支持 |
| **使用场景** | 保护临界区 | 生产者-消费者、资源池 |

**代码对比**：

```c
// ========== 互斥锁 - 保护临界区 ==========
OS_MUTEX g_mutex;
int g_counter = 0;

void Task_A(void)
{
    OS_ERR err;
    OSMutexPend(&g_mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    g_counter++;  // 临界区
    OSMutexPost(&g_mutex, OS_OPT_POST_NONE, &err);
}

// ========== 信号量 - 资源计数 ==========
OS_SEM g_sem;
#define BUFFER_SIZE 10

void Producer(void)
{
    OS_ERR err;
    // 生产数据
    OSSemPost(&g_sem, OS_OPT_POST_1, &err);  // 信号量+1
}

void Consumer(void)
{
    OS_ERR err;
    OSSemPend(&g_sem, 0, OS_OPT_PEND_BLOCKING, NULL, &err);  // 信号量-1
    // 消费数据
}
```

#### Q6: 什么是死锁？如何避免？
**A:**

**死锁（Deadlock）**：两个或多个任务互相等待对方持有的资源，导致所有任务都无法继续执行。

```
死锁示例：
任务A持有资源1，请求资源2
任务B持有资源2，请求资源1

任务A: ───│持有R1│───│等待R2│───► (阻塞)
任务B: ───│持有R2│───│等待R1│───► (阻塞)
```

**避免策略**：

1. **资源有序分配**
   - 给所有资源编号
   - 任务必须按编号顺序申请资源

```c
// 资源编号：R1=1, R2=2, R3=3
void Task_Safe(void)
{
    OSMutexPend(&Mutex_R1, ...);  // 先申请编号小的
    OSMutexPend(&Mutex_R2, ...);  // 再申请编号大的
    // 使用资源
    OSMutexPost(&Mutex_R2, ...);
    OSMutexPost(&Mutex_R1, ...);
}
```

2. **超时机制**
   - 申请资源时设置超时时间

```c
OS_ERR err;
OSMutexPend(&Mutex, 100, OS_OPT_PEND_BLOCKING, NULL, &err);
if (err == OS_ERR_TIMEOUT) {
    // 超时处理，避免死锁
}
```

3. **一次性申请所有资源**
   - 要么全部获得，要么一个都不申请

#### Q7: 消息队列和信号量的使用场景？
**A:**

| 场景 | 推荐机制 | 原因 |
|------|----------|------|
| 传递数据 | 消息队列 | 可以携带数据指针 |
| 任务同步 | 信号量 | 轻量级，只传递信号 |
| 资源池管理 | 信号量 | 计数功能 |
| 事件通知 | 信号量/事件标志 | 多对多通知 |
| 数据缓冲 | 消息队列 | FIFO缓冲 |

### D.3 中断相关问题

#### Q8: uC/OS-III 中如何写中断服务程序？
**A:**

```c
void USART1_IRQHandler(void)
{
    OS_ERR err;
    
    // 1. 进入中断
    OSIntEnter();
    
    // 2. 中断处理（尽量简短）
    if (USART_GetITStatus(USART1, USART_IT_RXNE)) {
        uint8_t data = USART_ReceiveData(USART1);
        
        // 发送消息到队列（唤醒任务处理）
        OSQPost(&RxQueue, (void *)(uintptr_t)data, 0, 
                OS_OPT_POST_FIFO, &err);
    }
    
    // 3. 退出中断（可能触发任务切换）
    OSIntExit();
}
```

**中断设计原则**：
1. **简短快速**：中断处理时间尽量短
2. **不阻塞**：中断中不能调用阻塞API
3. **使用OSIntEnter/OSIntExit**：通知内核
4. **延迟处理**：复杂处理放到任务中

#### Q9: 中断延迟时间是多少？
**A:**

| 延迟类型 | 说明 | 典型值 |
|----------|------|--------|
| **硬件延迟** | 中断响应到执行ISR第一条指令 | 12个时钟周期（Cortex-M4） |
| **内核延迟** | OSIntEnter执行时间 | 几个时钟周期 |
| **总延迟** | 硬件+内核 | < 1μs @ 168MHz |

**优化方法**：
- 使用尾链（Tail-chaining）减少切换开销
- 避免在中断中执行复杂计算

### D.4 内存管理相关问题

#### Q10: uC/OS-III 的内存管理机制？
**A:**

uC/OS-III 提供**内存分区（Memory Partition）**管理：

```c
// 1. 定义内存分区
#define MEM_BLK_SIZE    100     // 每块大小
#define MEM_NBLKS       10      // 块数量

OS_MEM  MyMemPart;              // 内存分区控制块
CPU_INT08U  MyMem[MEM_NBLKS][MEM_BLK_SIZE];  // 内存池

// 2. 创建内存分区
void Mem_Init(void)
{
    OS_ERR err;
    OSMemCreate(&MyMemPart,           // 分区控制块
                "MyMemPart",          // 名称
                &MyMem[0][0],         // 内存池首地址
                MEM_BLK_SIZE,         // 块大小
                MEM_NBLKS,            // 块数量
                &err);
}

// 3. 申请内存
void *p_blk = OSMemGet(&MyMemPart, &err);

// 4. 释放内存
OSMemPut(&MyMemPart, p_blk, &err);
```

**特点**：
- 固定大小块分配，无碎片
- O(1)时间复杂度
- 可预测的执行时间

### D.5 实时性问题

#### Q11: 什么是实时操作系统？硬实时和软实时的区别？
**A:**

| 类型 | 定义 | 示例 | 错过截止时间的后果 |
|------|------|------|-------------------|
| **硬实时** | 必须在截止时间内完成 | 汽车安全气囊、飞机控制 | 系统失效、危险 |
| **软实时** | 偶尔超时可以接受 | 视频播放、网络通信 | 性能下降 |
| **准实时** | 尽量满足时限 | 工业控制 | 质量下降 |

**uC/OS-III 特点**：
- 确定性调度（O(1)时间复杂度）
- 可预测的响应时间
- 适合硬实时应用

#### Q12: 如何保证任务的实时性？
**A:**

1. **合理分配优先级**
   - 截止时间越短，优先级越高
   - 关键任务优先级最高

2. **避免优先级反转**
   - 使用互斥锁（支持优先级继承）
   - 避免使用信号量保护临界区

3. **控制中断延迟**
   - 中断处理简短
   - 关中断时间最短化

4. **避免长时间临界区**
   - 临界区代码尽量短
   - 不要在临界区中调用复杂函数

5. **栈溢出保护**
   - 启用栈检查
   - 合理分配栈大小

### D.6 调试技巧

#### Q13: 如何调试uC/OS-III程序？
**A:**

1. **使用统计任务**
```c
// 启用统计任务
#define OS_CFG_STAT_TASK_EN     1u

// 获取CPU使用率
CPU_INT16U usage = OSStatTaskCPUUsage;
```

2. **栈溢出检查**
```c
// 创建任务时启用栈检查
OSTaskCreate(&tcb, "Task", TaskFunc, NULL, prio,
             &stk[0], stk_size/10, stk_size, ...,
             OS_OPT_TASK_STK_CHK, &err);

// 运行时检查
CPU_STK_SIZE free, used;
OSTaskStkChk(&tcb, &free, &used, &err);
```

3. **使用钩子函数**
```c
// 任务切换钩子
void OSTaskSwHook(void)
{
    // 记录任务切换信息
}

// 空闲任务钩子
void OSIdleTaskHook(void)
{
    // 进入低功耗模式
}
```

4. **断言和错误处理**
```c
void App_OS_SetAllHooks(void)
{
    OS_AppTaskCreateHookPtr = MyTaskCreateHook;
    OS_AppTaskDelHookPtr = MyTaskDelHook;
    OS_AppTaskReturnHookPtr = MyTaskReturnHook;
}
```

### D.7 与其他RTOS对比

#### Q14: uC/OS-III vs FreeRTOS vs RT-Thread？
**A:**

| 特性 | uC/OS-III | FreeRTOS | RT-Thread |
|------|-----------|----------|-----------|
| **内核大小** | 6-24KB | 4-9KB | 3-8KB |
| **商业授权** | 商业软件（需授权） | MIT开源 | Apache 2.0 |
| **优先级数** | 可配置（默认32） | 可配置（默认32） | 256 |
| **时间片** | 支持 | 支持 | 支持 |
| **互斥锁优先级继承** | 支持 | 支持 | 支持 |
| **组件丰富度** | 中等 | 需第三方 | 丰富（组件包） |
| **中文支持** | 英文文档 | 英文文档 | 中文社区 |
| **适用场景** | 商业产品 | 通用嵌入式 | IoT/复杂应用 |

---

## E. 常见陷阱与最佳实践

### E.1 常见错误

#### ❌ 错误1：在中断中调用阻塞函数
```c
// 错误！中断中不能调用阻塞函数
void ISR(void)
{
    OS_ERR err;
    OSMutexPend(&mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);  // ❌
    OSTimeDly(100, OS_OPT_TIME_DLY, &err);  // ❌
}
```

#### ❌ 错误2：栈溢出
```c
// 错误！栈大小不足
#define TASK_STK_SIZE   64   // 太小！
CPU_STK TaskStk[64];
// 使用printf或复杂函数会导致栈溢出
```

#### ❌ 错误3：优先级设置不当
```c
// 错误！空闲任务优先级不能用于用户任务
#define MY_TASK_PRIO    OS_CFG_PRIO_MAX - 1  // 31，这是空闲任务优先级！
```

#### ❌ 错误4：忘记检查错误码
```c
// 错误！不检查错误码
OSTaskCreate(&tcb, "Task", func, NULL, prio, stk, limit, size, ...);
// 如果创建失败，程序继续运行可能导致崩溃

// 正确做法
OS_ERR err;
OSTaskCreate(&tcb, "Task", func, NULL, prio, stk, limit, size, ..., &err);
if (err != OS_ERR_NONE) {
    // 错误处理
    Error_Handler();
}
```

---

## F. 面试题大全（30道高频题）

### F.1 uC/OS-II vs uC/OS-III

#### Q1: uC/OS-II 和 uC/OS-III 的主要区别？
**A:**

| 特性 | uC/OS-II | uC/OS-III |
|------|----------|-----------|
| **同优先级任务** | 只能有一个 | 可以有多个（任务组），支持时间片轮转 |
| **可裁剪性** | 一般 | 更强，更多配置选项 |
| **内核对象** | 较少 | 更丰富（任务队列、任务信号量等） |
| **结构** | 较简单 | 更模块化 |
| **优先级数** | 64（固定） | 可配置（默认32） |

### F.2 调度算法

#### Q2: uC/OS-III 使用的调度算法是什么？
**A:** 
- **固定优先级、抢占式调度**
- 总是执行就绪队列中优先级最高的任务
- 同优先级任务之间可启用**时间片轮转**

#### Q3: 任务优先级数值和"高低"的关系？
**A:** 
- **数值越小，优先级越高**
- 0 是最高优先级
- `OS_CFG_PRIO_MAX-1` 为最低优先级（空闲任务）

### F.3 系统启动

#### Q4: OSInit() 和 OSStart() 分别做什么？
**A:**

| 函数 | 作用 | 调用时机 |
|------|------|----------|
| `OSInit()` | 初始化内核数据结构和内核任务控制块 | 必须最先调用，在创建任何任务之前 |
| `OSStart()` | 启动调度器，进行第一次任务切换 | 所有初始化完成后调用，之后不会返回到 main() |

### F.4 空闲任务

#### Q5: Idle Task（空闲任务）有什么用？能删除吗？
**A:**
- **作用**：当无其他就绪任务时运行，保证 CPU 有任务可执行
- **用途**：可在其中做省电处理、系统统计
- **限制**：
  - 由内核自动创建，优先级最低
  - **应用层不能删除、不能修改优先级**

### F.5 统计任务

#### Q6: 为什么需要空闲任务之外的"统计任务"？
**A:**
- **作用**：定期统计 CPU 空闲时间，计算 CPU 使用率
- **启用**：
  - 配置 `OS_CFG_STAT_TASK_EN 1`
  - 调用 `OSStatTaskCPUUsageInit()` 初始化
- **获取**：`OSStatTaskCPUUsage` 变量表示 CPU 使用率（千分之一）

### F.6 O(1)调度

#### Q7: uC/OS-III 如何做到 O(1) 时间找到最高优先级就绪任务？
**A:**
- 使用**位图 + 优先级表**结构
- 每个优先级对应一位或一项
- 通过查表/位操作在 O(1) 时间定位最高优先级就绪任务
- 就绪表使用位图，256个优先级只需查两次表

### F.7 时间片轮转

#### Q8: 什么是时间片轮转？如何开启？
**A:**
- **定义**：同优先级的多个就绪任务按固定 Tick 数轮流执行
- **开启**：
```c
OSSchedRoundRobinCfg(OS_TRUE, time_quanta, &err);
// OS_TRUE: 启用
// time_quanta: 时间片大小（时钟节拍数）
```

#### Q9: 时间片轮转会影响不同优先级任务吗？
**A:**
- **不会**
- 时间片轮转只在**同优先级任务之间**起作用
- 高优先级任务仍然可以随时抢占低优先级任务

### F.8 优先级反转

#### Q10: 什么是优先级反转？uC/OS-III 如何解决？
**A:**

**现象**：低优先级任务持有资源，高优先级任务等待，中间又被中优先级任务抢占，导致高优先级"被反转"。

**解决方案**：
- uC/OS-III 的**互斥锁支持优先级继承**
- 持锁的低优先级任务临时提升到等待者中的最高优先级
- 释放锁后恢复原始优先级

### F.9 同步机制对比

#### Q11: Semaphore 和 Mutex 的核心区别？
**A:**

| 特性 | Semaphore | Mutex |
|------|-----------|-------|
| **类型** | 计数型 | 二元锁（0或1） |
| **用途** | 资源计数、任务同步 | 保护临界区 |
| **所有权** | 无 | 有（只有持有者能释放） |
| **优先级继承** | 不支持 | 支持 |
| **递归锁定** | 不支持 | 支持（配置后） |

#### Q12: 信号量用于互斥有哪些问题？
**A:**
- **不支持优先级继承** → 易导致优先级反转
- **没有"所有者"** → 任何任务都能 Post，易误用
- **结论**：保护临界区推荐使用 **Mutex**，不要用 Semaphore

### F.10 死锁

#### Q13: 死锁产生的典型条件？uC/OS-III 如何避免？
**A:**

**四个必要条件**：
1. **互斥使用**：资源一次只能被一个任务占用
2. **不可剥夺**：已获得的资源不能被强制剥夺
3. **部分分配**：占有且等待其他资源
4. **循环等待**：形成等待环路

**避免策略**：
1. **资源有序分配**：给资源编号，按固定顺序申请
2. **超时机制**：使用超时时间 Pend，避免无限等待
3. **避免嵌套锁 + 交叉锁**的设计

### F.11 中断管理

#### Q14: OSIntEnter() / OSIntExit() 为什么必须成对调用？
**A:**

| 函数 | 作用 |
|------|------|
| `OSIntEnter()` | 增加中断嵌套计数，告诉内核"进入中断" |
| `OSIntExit()` | 减少计数，当回到 0 时检查是否要切换任务 |

**不配对后果**：
- 调度失效
- 嵌套计数错乱
- 可能导致系统崩溃

#### Q15: 中断里可以做哪些 RTOS 操作？有什么限制？
**A:**

**可以调用（非阻塞）**：
- `OSQPost()` - 发送消息队列
- `OSSemPost()` - 发送信号量
- `OSTaskSemPost()` - 发送任务信号量
- `OSFlagPost()` - 设置事件标志

**不能调用（会阻塞）**：
- 任何 `Pend` 函数
- `OSTimeDly()`
- `OSTaskDel()`
- `OSMutexPost()`（可能导致优先级继承问题）

#### Q16: NVIC 优先级与 OS_CPU_CFG_INT_PRIO_MIN 的关系？
**A:**
- Cortex-M：**数值小 = 优先级高**
- 只有优先级数值**不高于（>=）** `OS_CPU_CFG_INT_PRIO_MIN` 的中断才能安全调用 OS API
- 数值更小的"真正高优先级"中断只做裸处理，不调用 OS 函数

### F.12 延时函数

#### Q17: OSTimeDly() 和 OSTimeDlyHMSM() 区别？
**A:**

| 函数 | 参数 | 说明 |
|------|------|------|
| `OSTimeDly()` | Tick 数 | 相对延时 |
| `OSTimeDlyHMSM()` | 时/分/秒/毫秒 | 内部转成 Tick，写法更直观 |

**注意**：两者都是**相对延时**，不是绝对时间

### F.13 软件定时器

#### Q18: 软件定时器 OS_TMR 适合什么场景？和 OSTimeDly 有何不同？
**A:**

| 特性 | OSTimeDly | OS_TMR |
|------|-----------|--------|
| **影响范围** | 只影响当前任务 | 独立定时对象 |
| **到期处理** | 任务继续执行 | 执行回调函数 |
| **用途** | 任务周期性执行 | 心跳、超时检测、定时事件 |
| **灵活性** | 简单 | 可唤醒任务/发消息 |

**适用场景**：
- `OSTimeDly`："周期性任务自己 sleep"
- `OS_TMR`：需要独立定时、到期执行特定操作

### F.14 内存管理

#### Q19: uC/OS-III 的内存管理方式（OS_MEM）特点？
**A:**
- **预先划分**固定大小内存块，形成内存分区
- **分配/释放为 O(1)**，无碎片
- **执行时间可预测**，适合实时系统
- **不建议**大量使用 malloc/free（不可预测、可能碎片）

```c
// 创建内存分区
OSMemCreate(&mem_part, "Mem", mem_buf, blk_size, n_blks, &err);

// 分配/释放
void *p = OSMemGet(&mem_part, &err);
OSMemPut(&mem_part, p, &err);
```

### F.15 栈管理

#### Q20: 栈溢出如何预防和检测？
**A:**

**预防**：
- 设计时预估调用深度、局部变量大小
- 考虑是否使用 printf/浮点运算（占用栈空间大）

**检测**：
1. **创建时启用栈检查**：`OS_OPT_TASK_STK_CHK`
2. **运行时检查**：`OSTaskStkChk()` 获取已用/剩余栈空间
3. **监控**：使用率 > 80% 时应考虑扩栈

### F.16 FPU 支持

#### Q21: 使用 FPU（浮点运算）任务的特别配置是什么？
**A:**
- 创建任务时选项加 `OS_OPT_TASK_SAVE_FP`
- 让内核在任务切换时保存/恢复 FPU 寄存器
- **否则**：多个任务的浮点运算可能互相污染，导致计算错误

```c
OSTaskCreate(&tcb, "Task", func, arg, prio, stk, limit, size, ...,
             OS_OPT_TASK_STK_CHK | OS_OPT_TASK_SAVE_FP,  // 启用FPU保存
             &err);
```

### F.17 调度锁

#### Q22: 调度锁 OSSchedLock/OSSchedUnlock 用来做什么？注意点？
**A:**

**作用**：
- 暂时禁止任务级调度
- 当前任务不会被其他任务抢占
- 中断照常运行

**用途**：
- 保护对"调度原子性"要求高但又不想关中断的代码

**注意**：
- 锁定时间必须**短**
- 否则高优先级任务响应会被拖延
- 影响实时性

### F.18 临界区

#### Q23: 临界区（关中断）和调度锁的区别与使用建议？
**A:**

| 机制 | 范围 | 影响 | 使用建议 |
|------|------|------|----------|
| **关中断（临界区）** | 中断 + 任务切换 | 最大 | 只包非常短的关键操作 |
| **调度锁** | 仅任务切换 | 较小 | 能用调度锁就不用长时间关中断 |

**建议**：
- 能用调度锁就不用长时间关中断
- 关中断只包非常短的关键操作（如操作共享变量）

### F.19 任务删除

#### Q24: 任务删除 OSTaskDel 的正确用法和陷阱？
**A:**

**正确用法**：
- **不能在中断中调用**
- 通常用**"自删"**方式：任务达到结束条件后 `OSTaskDel(NULL, &err)`

**陷阱**：
- 删除他人任务要先清理其使用的资源
- 防止悬挂指针/未释放资源
- 被删除任务占用的资源不会自动释放

### F.20 任务挂起

#### Q25: 任务挂起/恢复（Suspend/Resume）是否推荐大量使用？
**A:**
- **不推荐**当成业务逻辑主机制
- 更多用于**系统控制/调试场景**
- 业务上需要"等待事件/计时"，优先使用：
  - 信号量
  - 事件标志
  - 消息队列
  - 延时函数

### F.21 printf 问题

#### Q26: 为什么 RTOS 中不推荐大量使用 printf？
**A:**
- **耗时长**：格式化输出计算复杂
- **不确定**：执行时间不可预测
- **栈空间大**：内部缓冲区占用大量栈
- **实时性破坏**：尤其在高优先级任务或中断中

**替代方案**：
- "日志任务 + 队列"集中输出
- 正式版本关闭 printf
- 使用轻量级输出（如 ITM/SWO）

### F.22 Tick 频率

#### Q27: Tick 频率（OS_CFG_TICK_RATE_HZ）设置过高或过低的影响？
**A:**

| 设置 | 影响 |
|------|------|
| **过高** | 延时分辨率好，但 Tick 中断频繁，CPU 开销大 |
| **过低** | 省 CPU，但延时粒度粗，难以满足精细实时需求 |

**推荐**：STM32 常用 **1000Hz（1ms）**

### F.23 架构选择

#### Q28: 超级循环结构 vs RTOS 结构，各自适合什么项目？
**A:**

| 架构 | 适用场景 |
|------|----------|
| **超级循环** | 逻辑简单、小项目、对实时性要求不高 |
| **RTOS** | 多功能、复杂控制、对实时性/扩展性有要求的项目（如平衡车、多传感器融合） |

### F.24 调试手段

#### Q29: uC/OS-III 中常见调试手段有哪些？
**A:**

1. **CPU 使用率**：
   - 开启统计任务，读 `OSStatTaskCPUUsage`

2. **栈检查**：
   - `OSTaskStkChk()` 获取栈使用情况

3. **钩子函数**：
   - 任务切换钩子 `OSTaskSwHook()`
   - 空闲任务钩子 `OSIdleTaskHook()`
   - 用于打 log 或测量时间

4. **断言和错误码检查**：
   - 每个 OS 调用后检查 `err`

### F.25 生产者-消费者

#### Q30: 用消息队列 vs 用信号量实现生产者-消费者的区别？
**A:**

| 机制 | 特点 | 适用场景 |
|------|------|----------|
| **消息队列** | 携带数据（指针/结构体），生产者把"数据+通知"一起发 | 小数据、明确的一对一消费 |
| **信号量** | 只表达"有多少个资源/事件"，数据一般放共享区 | 资源池计数、批量数据处理 |

**选择建议**：
- 需要传递具体数据 → **消息队列**
- 只需要通知/计数 → **信号量**
- 大数据量/环形缓冲区 → **信号量 + 共享内存**
```

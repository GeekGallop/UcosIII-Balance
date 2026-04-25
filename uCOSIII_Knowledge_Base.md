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

*文档生成时间：2026-04-25*
*基于 uC/OS-III V3.08.01*

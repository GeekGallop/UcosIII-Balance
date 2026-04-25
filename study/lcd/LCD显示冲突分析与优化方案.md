# LCD 显示冲突分析与优化方案

## 一、当前问题分析

### 1.1 现有LCD任务

当前代码中有**两个LCD显示任务**：

1. **LCD_Sensor_Task** (task.c:323)
   - 显示PID参数
   - 显示位置：Y=60, 80, 100
   - 更新周期：20ms

2. **Task_MPU6050_LCD_Display** (task.c:277)
   - 显示MPU6050数据（Roll, Pitch, Temp）
   - 显示位置：Y=0, 20, 40
   - 更新周期：20ms
   - **问题**：每次更新前调用 `LCD_Fill(0,0,LCD_HEIGHT,LCD_WIDTH,WHITE)` 清屏

### 1.2 冲突点

| 冲突类型 | 说明 | 后果 |
|----------|------|------|
| **清屏冲突** | MPU6050任务每次清屏会擦除PID显示 | PID参数闪烁或消失 |
| **位置重叠风险** | 两个任务都在操作同一屏幕 | 显示内容相互覆盖 |
| **时序竞争** | 两个任务同时调用LCD驱动 | 可能出现花屏或乱码 |
| **无同步机制** | 没有互斥锁保护LCD访问 | 数据竞争 |

### 1.3 具体问题代码

```c
// Task_MPU6050_LCD_Display - 每次更新都清屏！
void Task_MPU6050_LCD_Display(void *p_arg)
{
    ...
    LCD_Fill(0,0,LCD_HEIGHT,LCD_WIDTH,WHITE);  // 清屏会擦除所有内容！
    ...
    while(1)
    {
        ...
        LCD_ShowString(0, 0, (u8*)buf, BLACK, WHITE, 16, 0);   // Y=0
        LCD_ShowString(0, 20, (u8*)buf, BLACK, WHITE, 16, 0);  // Y=20
        LCD_ShowString(0, 40, (u8*)buf, BLACK, WHITE, 16, 0);  // Y=40
        ...
    }
}

// LCD_Sensor_Task - 在Y=60,80,100显示
void LCD_Sensor_Task(void*p_arg)
{
    ...
    LCD_ShowString(0,60,"LCD Sensor Task",BLACK,WHITE,16,0);  // Y=60
    while(1)
    {
        LCD_ShowString(0,80,buff,BLACK,WHITE,16,0);   // Y=80
        LCD_ShowString(0,100,buff,BLACK,WHITE,16,0);  // Y=100
        ...
    }
}
```

## 二、优化方案

### 方案一：统一LCD管理器（推荐）

**核心思想**：
- 使用互斥锁保护LCD访问
- 屏幕分区显示，避免重叠
- 单一任务负责所有LCD更新

**屏幕布局**：
```
┌─────────────────────────────┐
│ Line 0:  MPU6050 Data       │ ← 标题
│ Line 1:  Roll:  xxx.xx      │ ← MPU数据区
│ Line 2:  Pitch: xxx.xx      │
│ Line 3:  Temp:  xxx.xx      │
├─────────────────────────────┤ ← 分隔线 (Y=64)
│ Line 4:  PID Parameters     │ ← 标题
│ Line 5:  P:xx.xx I:xx.xx    │ ← PID数据区
│ Line 6:  D:xx.xx            │
│ Line 7:                     │ ← 保留
└─────────────────────────────┘
```

**实现文件**：
- `lcd_manager.c/h` - LCD管理器（互斥锁保护）
- `lcd_task.c/h` - 统一LCD任务

**优点**：
- ✅ 彻底解决显示冲突
- ✅ 代码结构清晰
- ✅ 易于维护和扩展
- ✅ 支持动态调整更新频率

**缺点**：
- 需要修改现有代码
- 增加一个互斥锁开销

---

### 方案二：分区显示（快速修复）

如果不希望大幅改动，可以快速修复：

1. **移除清屏操作** - 两个任务都不清屏
2. **固定显示位置** - 确保位置不重叠
3. **添加互斥锁** - 保护LCD访问

**修改示例**：

```c
// 在 mpu6050.c 或合适的位置创建LCD互斥锁
OS_MUTEX LCD_Mutex;

// 初始化时创建
OSMutexCreate(&LCD_Mutex, "LCD_Mutex", &err);

// Task_MPU6050_LCD_Display 修改
void Task_MPU6050_LCD_Display(void *p_arg)
{
    ...
    // LCD_Fill(0,0,LCD_HEIGHT,LCD_WIDTH,WHITE);  // 删除这行！
    ...
    while(1)
    {
        OSSemPend(&MPU6050_Data_Ready_Sem, ...);
        
        if(err == OS_ERR_NONE)
        {
            // 获取互斥锁
            OSMutexPend(&LCD_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
            
            // 显示数据（不清屏，只更新值）
            sprintf(buf, "Roll: %.2f", attitude.roll);
            LCD_ShowString(0, 0, (u8*)buf, BLACK, WHITE, 16, 0);
            ...
            
            // 释放互斥锁
            OSMutexPost(&LCD_Mutex, OS_OPT_POST_NONE, &err);
        }
    }
}

// LCD_Sensor_Task 同样需要加锁
void LCD_Sensor_Task(void*p_arg)
{
    ...
    while(1)
    {
        // 获取互斥锁
        OSMutexPend(&LCD_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
        
        sprintf((char*)buff,"p:%.2f,i:%.2f",p,i);
        LCD_ShowString(0,80,buff,BLACK,WHITE,16,0);
        ...
        
        // 释放互斥锁
        OSMutexPost(&LCD_Mutex, OS_OPT_POST_NONE, &err);
        
        OSTimeDly(20, OS_OPT_TIME_DLY, &err);
    }
}
```

**优点**：
- ✅ 改动小，快速修复
- ✅ 保留原有任务结构

**缺点**：
- ❌ 代码分散，不易维护
- ❌ 仍然有两个任务竞争资源
- ❌ 显示刷新不同步

---

## 三、推荐实施方案

### 步骤1：添加LCD管理器文件

已创建：
- `lcd_manager.c` - LCD管理器实现
- `lcd_manager.h` - LCD管理器头文件
- `lcd_task.c` - 统一LCD任务
- `lcd_task.h` - 统一LCD任务头文件

### 步骤2：修改 start_task

```c
#include "lcd_task.h"

void start_task(void *p_arg)
{
    OS_ERR err;
    
    /* ... 其他初始化 ... */
    
    /* 创建统一LCD任务（替代原来的两个LCD任务） */
    if(LCD_Task_Create() != 0) {
        printf("ERROR: Failed to create LCD task\r\n");
    }
    
    /* 删除原来的LCD任务创建代码 */
    // OSTaskCreate(&LCD_Task_TCB, "LCD_Task", LCD_Sensor_Task, ...);
    // OSTaskCreate(&MPU6050_Task_LCD_Display_TCB, "MPU6050_Task 2", Task_MPU6050_LCD_Display, ...);
    
    /* 删除启动任务 */
    OSTaskDel(NULL, &err);
}
```

### 步骤3：删除或注释掉原有LCD任务

在 `task.c` 中：
- 删除 `LCD_Sensor_Task` 函数
- 删除 `Task_MPU6050_LCD_Display` 函数
- 删除相关的任务定义和TCB声明

### 步骤4：验证

1. 编译检查
2. 观察LCD显示是否正常
3. 检查是否有闪烁或冲突

---

## 四、性能考虑

### 更新频率建议

| 数据类型 | 建议更新频率 | 原因 |
|----------|--------------|------|
| MPU6050数据 | 50-100ms | 传感器数据变化快 |
| PID参数 | 100-200ms | 参数变化相对慢 |
| 静态文本 | 不需要更新 | 只需显示一次 |

当前统一任务使用50ms更新周期，可以满足两种数据需求。

### 资源占用

- **RAM**: 增加约100字节（互斥锁 + 任务栈）
- **CPU**: 几乎无增加（反而减少任务切换开销）
- **代码**: 增加约2KB

---

## 五、扩展建议

### 5.1 添加页面切换功能

可以扩展支持多页面显示：

```c
typedef enum {
    LCD_PAGE_MPU_PID,    /* MPU6050 + PID */
    LCD_PAGE_MPU_RAW,    /* MPU6050原始数据 */
    LCD_PAGE_DEBUG,      /* 调试信息 */
    LCD_PAGE_MAX
} LCD_Page_t;

void LCD_SetPage(LCD_Page_t page);
```

### 5.2 添加背光控制

```c
void LCD_SetBacklight(uint8_t brightness);
```

### 5.3 添加显示开关

```c
void LCD_EnableDisplay(uint8_t enable);
```

---

## 六、总结

| 方案 | 工作量 | 效果 | 推荐度 |
|------|--------|------|--------|
| 统一LCD管理器 | 中 | 彻底解决 | ⭐⭐⭐⭐⭐ |
| 分区显示+互斥锁 | 小 | 临时修复 | ⭐⭐⭐ |
| 保持现状 | 无 | 有冲突 | ⭐ |

**强烈建议采用方案一（统一LCD管理器）**，可以彻底解决显示冲突问题，并且代码结构更清晰，易于后续维护。

---

**创建日期**: 2026-02-15  
**版本**: 1.0

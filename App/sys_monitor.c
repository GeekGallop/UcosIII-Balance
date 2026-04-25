/**
 ******************************************************************************
 * @file    sys_monitor.c
 * @brief   System Performance Monitor for uC/OS-III
 * @note    Provides CPU usage, stack usage, task statistics monitoring
 * @author  User
 * @date    2026-02-15
 ******************************************************************************
 */

#include "sys_monitor.h"
#include "os.h"
#include <stdio.h>
#include <string.h>

/* External declarations for task TCBs defined in task.c */
extern OS_TCB Protocol_Task_TCB;
extern OS_TCB LCD_Task_TCB;
extern OS_TCB MPU6050_Task_READ_TCB;
extern OS_TCB MPU6050_Task_TCB;
extern OS_TCB MPU6050_Task_LCD_Display_TCB;

/* ========== Configuration ========== */

/* Monitor task priority - should be low priority */
#define MONITOR_TASK_PRIO       20
#define MONITOR_TASK_STK_SIZE   512

/* Monitor period - how often to print statistics (in ms) */
#define MONITOR_PERIOD_MS       5000    /* Print every 5 seconds */

/* ========== Task Definition ========== */

OS_TCB      MonitorTask_TCB;
CPU_STK     MonitorTask_Stk[MONITOR_TASK_STK_SIZE];

/* ========== Internal Variables ========== */

static uint32_t s_monitor_period_ticks;

/* ========== Function Prototypes ========== */

static void MonitorTask(void *p_arg);
static void PrintHeader(void);
static void PrintSeparator(void);
static void PrintCPUUsage(void);
static void PrintTaskStats(void);
static void PrintInterruptStats(void);
static void PrintMemoryStats(void);
static void PrintContextSwitchStats(void);

/* ========== Public Functions ========== */

/**
 * @brief  Initialize system monitor
 * @note   Creates the monitor task
 * @retval 0=Success, 1=Failed
 */
uint8_t SysMonitor_Init(void)
{
    OS_ERR err;
    
    /* Calculate monitor period in ticks */
    s_monitor_period_ticks = 1000; //(MONITOR_PERIOD_MS * OSCfg_TickRate_Hz) / 1000;
    if(s_monitor_period_ticks < 1) {
        s_monitor_period_ticks = 1;
    }
    
    /* Create monitor task */
    OSTaskCreate(&MonitorTask_TCB,
                 "MonitorTask",
                 MonitorTask,
                 NULL,
                 MONITOR_TASK_PRIO,
                 &MonitorTask_Stk[0],
                 MONITOR_TASK_STK_SIZE / 10,
                 MONITOR_TASK_STK_SIZE,
                 0, 0, NULL,
                 OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 &err);
    
    if(err != OS_ERR_NONE) {
				OSTaskDel(&MonitorTask_TCB,&err);
        printf("ERROR: Failed to create MonitorTask, err=%d\r\n", err);
        return 1;
    }
    
    return 0;
}

/**
 * @brief  Get CPU usage percentage
 * @note   Uses uC/OS-III built-in statistics
 * @retval CPU usage in percentage (0-100)
 */
CPU_INT16U SysMonitor_GetCPUUsage(void)
{
    return OSStatTaskCPUUsage;
}

/**
 * @brief  Get task stack usage
 * @param  p_tcb: Pointer to task TCB
 * @param  p_free: Pointer to store free stack size
 * @param  p_used: Pointer to store used stack size
 * @retval 0=Success, 1=Failed
 */
uint8_t SysMonitor_GetTaskStackUsage(OS_TCB *p_tcb, CPU_STK_SIZE *p_free, CPU_STK_SIZE *p_used)
{
    OS_ERR err;
    
    if(p_tcb == NULL || p_free == NULL || p_used == NULL) {
        return 1;
    }
    
    OSTaskStkChk(p_tcb, p_free, p_used, &err);
    
    if(err != OS_ERR_NONE) {
        return 1;
    }
    
    return 0;
}

/**
 * @brief  Print single task statistics
 * @param  p_tcb: Pointer to task TCB
 * @param  task_name: Task name string
 */
void SysMonitor_PrintTaskInfo(OS_TCB *p_tcb, const char *task_name)
{
    OS_ERR err;
    CPU_STK_SIZE free_stk;
    CPU_STK_SIZE used_stk;
    CPU_STK_SIZE total_stk;
    uint8_t usage_percent;

    OSTaskStkChk(p_tcb, &free_stk, &used_stk, &err);
    
    if(err == OS_ERR_NONE) {
        total_stk = free_stk + used_stk;
        if(total_stk > 0) {
            usage_percent = (uint8_t)((used_stk * 100) / total_stk);
        } else {
            usage_percent = 0;
        }
        
        printf("%-16s: Stack %4d/%4d (%3d%%)  Prio=%2d  State=%s\r\n",
               task_name ? task_name : p_tcb->NamePtr,
               (int)used_stk,
               (int)total_stk,
               usage_percent,
               (int)p_tcb->Prio,
               SysMonitor_GetTaskStateString(p_tcb->TaskState));
    } else {
        printf("%-16s: Failed to get stack info (err=%d)\r\n", 
               task_name ? task_name : "Unknown", err);
    }
}

/**
 * @brief  Get task state as string
 * @param  state: Task state enum value
 * @retval State string
 */
const char* SysMonitor_GetTaskStateString(OS_STATE state)
{
    switch(state) {
        case OS_TASK_STATE_RDY:      return "Ready";
        case OS_TASK_STATE_DLY:      return "Delayed";
        case OS_TASK_STATE_PEND:     return "Pending";
        case OS_TASK_STATE_PEND_TIMEOUT: return "PendTout";
        case OS_TASK_STATE_SUSPENDED:return "Suspend";
        case OS_TASK_STATE_DLY_SUSPENDED: return "DlySusp";
        case OS_TASK_STATE_PEND_SUSPENDED: return "PendSus";
        case OS_TASK_STATE_PEND_TIMEOUT_SUSPENDED: return "PndTmSu";
        case OS_TASK_STATE_DEL:      return "Deleted";
        default:                     return "Unknown";
    }
}

/* ========== Private Functions ========== */

/**
 * @brief  Monitor task - periodically prints system statistics
 */
static void MonitorTask(void *p_arg)
{
    OS_ERR err;
    
    (void)p_arg;
    
    /* Wait for system to stabilize */
    OSTimeDly(1000, OS_OPT_TIME_DLY, &err);
    
    printf("\r\n");
    printf("=================================================\r\n");
    printf("    System Performance Monitor Started\r\n");
    printf("    Monitor Period: %d ms\r\n", MONITOR_PERIOD_MS);
    printf("=================================================\r\n");
    
    while(1)
    {
        PrintHeader();
        PrintCPUUsage();
        PrintSeparator();
        PrintTaskStats();
        PrintSeparator();
        PrintInterruptStats();
        PrintSeparator();
        PrintContextSwitchStats();
        PrintSeparator();
        PrintMemoryStats();
        printf("=================================================\r\n\r\n");
        
        /* Wait for next period */
        OSTimeDly(1000, OS_OPT_TIME_DLY, &err);
    }
}

/**
 * @brief  Print report header
 */
static void PrintHeader(void)
{
    printf("\r\n========== System Statistics Report ==========\r\n");
    printf("Time: %d ticks\r\n", (int)OSTickCtr);
}

/**
 * @brief  Print separator line
 */
static void PrintSeparator(void)
{
    printf("-----------------------------------------------\r\n");
}

/**
 * @brief  Print CPU usage statistics
 */
static void PrintCPUUsage(void)
{
    CPU_INT16U cpu_usage;
    CPU_INT16U cpu_usage_max;
    
    cpu_usage = OSStatTaskCPUUsage;
    cpu_usage_max = OSStatTaskCPUUsageMax;
    
    printf("  Current: %3d.%02d%%\r\n", 
           cpu_usage / 100, cpu_usage % 100);
    printf("  Peak:    %3d.%02d%%\r\n", 
           cpu_usage_max / 100, cpu_usage_max % 100);
    printf("  Idle:    %3d.%02d%%\r\n", 
           (10000 - cpu_usage) / 100, (10000 - cpu_usage) % 100);
}

/**
 * @brief  Print all task statistics
 */
static void PrintTaskStats(void)
{
    OS_ERR err;
    CPU_STK_SIZE free_stk;
    CPU_STK_SIZE used_stk;
    CPU_STK_SIZE total_stk;
    uint8_t usage_percent;
    
    printf("[Task Statistics]\r\n");
    printf("%-16s %10s %6s %8s\r\n", 
           "Task Name", "Stack", "Prio", "State");
    
    /* Note: Iterating through all tasks requires OS_CFG_DBG_EN > 0
     * For now, we just show that task statistics are available via OSTaskStkChk()
     */
    printf("Use SysMonitor_PrintTaskInfo() for individual task stats\r\n");
    printf("(Requires OS_CFG_DBG_EN > 0 for task list iteration)\r\n");
    
    /* Example of how to check a specific task */
    OSTaskStkChk(&Protocol_Task_TCB, &free_stk, &used_stk, &err);
    if(err == OS_ERR_NONE) {
        total_stk = free_stk + used_stk;
        usage_percent = (total_stk > 0) ? (uint8_t)((used_stk * 100) / total_stk) : 0;
        printf("%-16s %4d/%4d(%2d%%) %3d %8s\r\n",
               Protocol_Task_TCB.NamePtr ? Protocol_Task_TCB.NamePtr : "Protocol",
               (int)used_stk,
               (int)total_stk,
               usage_percent,
               (int)Protocol_Task_TCB.Prio,
               SysMonitor_GetTaskStateString(Protocol_Task_TCB.TaskState));
    }
}

/**
 * @brief  Print interrupt statistics
 */
static void PrintInterruptStats(void)
{
    printf("[Interrupt Statistics]\r\n");
    printf("  Interrupt Disable Time:\r\n");
    /* Note: SystemCoreClock is defined in system_stm32f4xx.c */
    /* If not available, just show raw cycles */
}


/**
 * @brief  Print context switch statistics
 */
static void PrintContextSwitchStats(void)
{
    printf("[Context Switch Statistics]\r\n");
    printf("  Total Context Switches: %d\r\n", (int)OSTaskCtxSwCtr);
    printf("  Context Switches/sec:   %d\r\n", 
           (int)(OSTaskCtxSwCtr / (OSTickCtr / OSCfg_TickRate_Hz + 1)));
}

/**
 * @brief  Print memory statistics
 */
static void PrintMemoryStats(void)
{
    OS_MSG_QTY total_msgs;
    
    printf("[Memory Statistics]\r\n");
    printf("  Free Msg Queue Entries: %d\r\n", 
           (int)OSMsgPool.NbrFree);
    printf("  Used Msg Queue Entries: %d\r\n", 
           (int)OSMsgPool.NbrUsed);
    total_msgs = OSMsgPool.NbrFree + OSMsgPool.NbrUsed;
    printf("  Total Msg Queue Size:   %d\r\n", (int)total_msgs);
}

/* ========== Advanced Monitoring Functions ========== */

#if MONITOR_ENABLE_ADVANCED

/**
 * @brief  Reset all statistics counters
 */
void SysMonitor_ResetStats(void)
{
    OS_ERR err;
    
    OSStatReset(&err);
    
    if(err == OS_ERR_NONE) {
        printf("Statistics reset successfully\r\n");
    } else {
        printf("Failed to reset statistics, err=%d\r\n", err);
    }
}

/**
 * @brief  Get detailed task timing information
 * @note   Requires OS_CFG_STAT_TASK_STK_CHK_EN to be enabled
 */
void SysMonitor_PrintTaskTiming(OS_TCB *p_tcb)
{
#if (OS_CFG_DBG_EN > 0u)
    if(p_tcb == NULL) {
        return;
    }
    
    printf("Task: %s\r\n", p_tcb->NamePtr ? p_tcb->NamePtr : "Unknown");
    printf("  Execution Time: %d cycles\r\n", (int)p_tcb->CyclesTotal);
    printf("  Context Switches: %d\r\n", (int)p_tcb->CtxSwCtr);
    printf("  CPU Usage: %d.%02d%%\r\n", 
           p_tcb->CPUUsage/100, p_tcb->CPUUsage % 100);
#else
    printf("Task timing requires OS_CFG_DBG_EN > 0\r\n");
#endif
}

#endif /* MONITOR_ENABLE_ADVANCED */

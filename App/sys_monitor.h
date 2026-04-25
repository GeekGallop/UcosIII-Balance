/**
 ******************************************************************************
 * @file    sys_monitor.h
 * @brief   System Performance Monitor Header for uC/OS-III
 * @note    Provides CPU usage, stack usage, task statistics monitoring
 * @author  User
 * @date    2026-02-15
 ******************************************************************************
 */

#ifndef __SYS_MONITOR_H
#define __SYS_MONITOR_H

#include "os.h"
#include <stdint.h>

/* ========== Configuration ========== */

/* Enable advanced monitoring features (0=Disable, 1=Enable) */
#define MONITOR_ENABLE_ADVANCED     1

/* ========== Function Prototypes ========== */

/**
 * @brief  Initialize system monitor
 * @retval 0=Success, 1=Failed
 */
uint8_t SysMonitor_Init(void);

/**
 * @brief  Get CPU usage percentage
 * @retval CPU usage in percentage (0-10000, divide by 100 for actual %)
 */
CPU_INT16U SysMonitor_GetCPUUsage(void);

/**
 * @brief  Get task stack usage
 * @param  p_tcb: Pointer to task TCB
 * @param  p_free: Pointer to store free stack size
 * @param  p_used: Pointer to store used stack size
 * @retval 0=Success, 1=Failed
 */
uint8_t SysMonitor_GetTaskStackUsage(OS_TCB *p_tcb, CPU_STK_SIZE *p_free, CPU_STK_SIZE *p_used);

/**
 * @brief  Print single task statistics
 * @param  p_tcb: Pointer to task TCB
 * @param  task_name: Task name string (can be NULL)
 */
void SysMonitor_PrintTaskInfo(OS_TCB *p_tcb, const char *task_name);

/**
 * @brief  Get task state as string
 * @param  state: Task state enum value
 * @retval State string
 */
const char* SysMonitor_GetTaskStateString(OS_STATE state);

#if MONITOR_ENABLE_ADVANCED

/**
 * @brief  Reset all statistics counters
 */
void SysMonitor_ResetStats(void);

/**
 * @brief  Get detailed task timing information
 * @param  p_tcb: Pointer to task TCB
 */
void SysMonitor_PrintTaskTiming(OS_TCB *p_tcb);

#endif /* MONITOR_ENABLE_ADVANCED */

/* External declarations for uC/OS-III global variables */
#if (OS_CFG_STAT_TASK_EN > 0u)
extern CPU_INT16U   OSStatTaskCPUUsage;
extern CPU_INT16U   OSStatTaskCPUUsageMax;
#endif

#if (OS_CFG_TS_EN > 0u)
extern CPU_TS       OSIntDisTimeMax;
#endif

extern OS_CTX_SW_CTR   OSTaskCtxSwCtr;
extern OS_TICK         OSTickCtr;
extern OS_MSG_POOL     OSMsgPool;

#endif /* __SYS_MONITOR_H */

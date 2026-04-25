/**
  ******************************************************************************
  * @file    Project/STM32F4xx_StdPeriph_Templates/stm32f4xx_it.c 
  * @author  MCD Application Team
  * @version V1.4.0
  * @date    04-August-2014
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_it.h"
#include "usart.h"
#include "os.h"
/** @addtogroup Template_Project
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M4 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
/* 1. 汇编入口：必须使用 naked 属性，避免编译器插入代码破坏现场 */
/* ========================================= */
/*  HardFault 诊断处理函数 (C 语言部分)      */
/* ========================================= */
void HardFault_Handler_C(uint32_t *hardfault_args)
{
    /* 1. 从栈帧提取硬件自动保存的寄存器 */
    uint32_t stacked_r0  = hardfault_args[0];
    uint32_t stacked_r1  = hardfault_args[1];
    uint32_t stacked_r2  = hardfault_args[2];
    uint32_t stacked_r3  = hardfault_args[3];
    uint32_t stacked_r12 = hardfault_args[4];
    uint32_t stacked_lr  = hardfault_args[5]; /* 异常发生时的链接寄存器 */
    uint32_t stacked_pc  = hardfault_args[6]; /* 异常发生时的程序计数器，关键！ */
    uint32_t stacked_psr = hardfault_args[7];

    /* 2. 读取故障状态寄存器 */
    uint32_t cfsr = SCB->CFSR;   /* 可配置故障状态寄存器 */
    uint32_t hfsr = SCB->HFSR;   /* 硬故障状态寄存器 */
    uint32_t bfar = SCB->BFAR;   /* 总线故障地址寄存器 (若BFARVALID=1则有效) */
    uint32_t mfar = SCB->MMFAR;  /* 内存管理故障地址寄存器 (若MMARVALID=1则有效) */

    /* 3. 格式化输出 (假设已重定向 printf 到安全串口) */
    printf("\r\n\r\n=== HARD FAULT DIAGNOSTIC ===\r\n");
    printf("Core Registers (from stacked frame):\r\n");
    printf("R0  = 0x%08X\r\n", stacked_r0);
    printf("R1  = 0x%08X\r\n", stacked_r1);
    printf("R2  = 0x%08X\r\n", stacked_r2);
    printf("R3  = 0x%08X\r\n", stacked_r3);
    printf("R12 = 0x%08X\r\n", stacked_r12);
    printf("LR  = 0x%08X (EXC_RETURN)\r\n", stacked_lr);
    printf("PC  = 0x%08X <-- FAULTING INSTRUCTION ADDRESS\r\n", stacked_pc);
    printf("PSR = 0x%08X\r\n", stacked_psr);

    printf("\r\nFault Status Registers:\r\n");
    printf("HFSR = 0x%08X\r\n", hfsr);
    printf("CFSR = 0x%08X\r\n", cfsr);

    /* 4. 解析 CFSR，判断具体错误类型 */
    if (cfsr & 0x00008000) printf("  [BusFault] BFARVALID=1, Fault Address = 0x%08X\r\n", bfar);
    if (cfsr & 0x00000100) printf("  [MemManage] MMARVALID=1, Fault Address = 0x%08X\r\n", mfar);
    if (cfsr & 0x00000080) printf("  [UsageFault] Undefined Instruction\r\n");
    if (cfsr & 0x00000040) printf("  [UsageFault] Invalid State (e.g., EPSR.T bit error)\r\n");
    if (cfsr & 0x00000020) printf("  [UsageFault] Invalid PC Load (EXC_RETURN error)\r\n");
    if (cfsr & 0x00000010) printf("  [UsageFault] Unaligned Access\r\n");
    if (cfsr & 0x00000008) printf("  [UsageFault] Divide by Zero\r\n");
    if (cfsr & 0x00000004) printf("  [BusFault] Imprecise Data Bus Error (may not have valid BFAR)\r\n");
    if (cfsr & 0x00000002) printf("  [BusFault] Precise Data Bus Error (BFARVALID)\r\n");
    if (cfsr & 0x00000001) printf("  [BusFault] Instruction Bus Error (IFETCH)\r\n");

    /* 5. 关键提示：如何定位源码 */
    printf("\r\n>> To find source line, use: arm-none-eabi-addr2line -e <your.elf> -f -C 0x%08X\r\n", stacked_pc);
    printf(">> Or in Keil: View -> Disassembly, then drag PC value (0x%08X) to window.\r\n", stacked_pc);


    while (1);
}

/* ========================================= */
/*  HardFault 汇编入口 (Naked 函数)         */
/* ========================================= */
/* 注意：根据编译器选择语法 */
#if defined ( __CC_ARM ) /* Keil MDK */
    __asm void HardFault_Handler(void)
    {
        IMPORT HardFault_Handler_C
        TST lr, #4
        ITE EQ
        MRSEQ r0, MSP
        MRSNE r0, PSP
        B HardFault_Handler_C
    }
#elif defined ( __GNUC__ ) /* GCC (STM32CubeIDE, IAR 等) */
    __attribute__((naked)) void HardFault_Handler(void)
    {
        __asm volatile(
            "TST lr, #4          \n"
            "ITE EQ              \n"
            "MRSEQ r0, MSP       \n"
            "MRSNE r0, PSP       \n"
            "B HardFault_Handler_C \n"
        );
    }
#endif

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}



/**
 ******************************************************************************
 * @file    pid_position_control_analysis.c
 * @brief   PID位置控制实现分析与改进建议
 * @note    基于uC-OS3-time项目代码分析，说明如何让电机在指定位置停下
 * @author  Code Analysis
 * @date    2026-02-21
 ******************************************************************************
 * 
 * ==================== 当前PID控制架构概述 ====================
 * 
 * 系统组成:
 * 1. PID控制器 (pid.c/pid.h) - 位置式PID算法实现
 * 2. 编码器驱动 (encoder.c) - TIM2/TIM3硬件正交解码
 * 3. 电机驱动 (motor.c) - PWM控制H桥驱动
 * 4. uC/OS-III任务调度 - 100Hz控制频率
 * 
 * 数据流:
 * task_encoder (100Hz) -> g_pid_encoder_queue -> Task_PID_Control (100Hz)
 * task_mpu (100Hz) -> g_pid_mpu_queue -> Task_PID_Control (100Hz)
 * 
 * ==================== 当前代码分析 ====================
 */

/* 
 * 【当前PID执行逻辑 - pid.c 中的 PID_Execute_Control()】
 * 
 * 问题：当前代码使用编码器位置作为输入，但没有设置目标位置！
 */

/* 当前代码（有问题的部分）:
static void PID_Execute_Control(void)
{
    if (g_pid_cached_data.mpu_valid) {
        pid_left.input = g_pid_cached_data.pos_left;  // 当前位置作为输入
        PID_Calculate(&pid_left);
        Motor_SetSpeed(MOTOR_A, pid_left.output);     // 输出到电机
    }
    
    // 注意：这里缺少设置目标位置的代码！
    // pid_left.setpoint 从未被设置，默认为0
}
*/

/*
 * 【问题诊断】
 * 
 * 1. 目标位置未设置：
 *    - PID_Init() 中 setpoint = 0.0f
 *    - 代码中没有调用 PID_SetSetpoint() 来设置目标位置
 *    - 电机总是试图到达位置0
 * 
 * 2. 输入与目标不匹配：
 *    - 输入是编码器位置（累计脉冲数）
 *    - 但setpoint默认为0
 *    - 如果当前位置不是0，电机会全速向0位置移动
 * 
 * 3. 缺少位置到达检测：
 *    - 没有判断何时到达目标位置
 *    - 电机到达后不会自动停止
 */

/*
 * ==================== 改进方案 ====================
 * 
 * 方案一：添加目标位置设置功能
 */

// 添加全局变量存储目标位置
static int32_t g_target_position_left = 0;
static int32_t g_target_position_right = 0;
static uint8_t g_position_control_active = 0;  // 位置控制使能标志

/**
 * @brief 设置目标位置
 * @param target_left: 左电机目标位置（编码器脉冲数）
 * @param target_right: 右电机目标位置（编码器脉冲数）
 * @retval None
 * @note  调用此函数设置电机应该到达的位置
 */
void PID_SetTargetPosition(int32_t target_left, int32_t target_right)
{
    OS_ERR err;
    
    // 进入临界区保护
    CPU_SR_ALLOC();
    CPU_CRITICAL_ENTER();
    
    g_target_position_left = target_left;
    g_target_position_right = target_right;
    g_position_control_active = 1;
    
    // 更新PID控制器的目标值
    pid_left.setpoint = (float)target_left;
    pid_right.setpoint = (float)target_right;
    
    CPU_CRITICAL_EXIT();
    
    printf("PID Target Set: Left=%ld, Right=%ld\r\n", 
           target_left, target_right);
}

/**
 * @brief 停止位置控制（电机停止）
 * @param None
 * @retval None
 */
void PID_StopPositionControl(void)
{
    OS_ERR err;
    CPU_SR_ALLOC();
    
    CPU_CRITICAL_ENTER();
    
    g_position_control_active = 0;
    pid_left.enabled = 0;
    pid_right.enabled = 0;
    
    CPU_CRITICAL_EXIT();
    
    // 停止电机
    Motor_Stop(MOTOR_BOTH);
    
    printf("PID Position Control Stopped\r\n");
}

/**
 * @brief 检查是否到达目标位置
 * @param None
 * @retval 1=到达目标, 0=未到达
 * @note  使用误差阈值判断是否到位
 */
static uint8_t PID_IsPositionReached(void)
{
    float error_left, error_right;
    const float POSITION_THRESHOLD = 10.0f;  // 位置误差阈值（脉冲数）
    
    // 计算位置误差
    error_left = pid_left.setpoint - pid_left.input;
    error_right = pid_right.setpoint - pid_right.input;
    
    // 检查是否在阈值范围内
    if ((fabs(error_left) <= POSITION_THRESHOLD) && 
        (fabs(error_right) <= POSITION_THRESHOLD)) {
        return 1;  // 到达目标
    }
    
    return 0;  // 未到达
}

/**
 * @brief 改进后的PID执行控制函数
 * @param None
 * @retval None
 */
static void PID_Execute_Control_Improved(void)
{
    static u16 loop_cnt = 0;
    
    // 只在位置控制激活时执行
    if (!g_position_control_active) {
        return;
    }
    
    // 确保编码器数据有效
    if (!g_pid_cached_data.encoder_valid) {
        return;
    }
    
    // 更新PID输入（当前位置）
    pid_left.input = (float)g_pid_cached_data.pos_left;
    pid_right.input = (float)g_pid_cached_data.pos_right;
    
    // 检查是否到达目标位置
    if (PID_IsPositionReached()) {
        // 到达目标，停止电机
        Motor_Stop(MOTOR_BOTH);
        
        // 可选：禁用PID或保持位置
        // pid_left.enabled = 0;
        // pid_right.enabled = 0;
        
        if (++loop_cnt >= 100) {  // 每秒打印一次
            loop_cnt = 0;
            printf("Position REACHED! L:%ld R:%ld\r\n",
                   g_pid_cached_data.pos_left,
                   g_pid_cached_data.pos_right);
        }
        return;
    }
    
    // 重新使能PID（如果之前被禁用）
    pid_left.enabled = 1;
    pid_right.enabled = 1;
    
    // 计算PID输出
    PID_Calculate(&pid_left);
    PID_Calculate(&pid_right);
    
    // 应用输出到电机（注意：PID输出范围是-100到100，需要映射到电机速度范围）
    // 假设PID输出是百分比，映射到-1000到1000
    int16_t motor_speed_left = (int16_t)(pid_left.output * 10.0f);
    int16_t motor_speed_right = (int16_t)(pid_right.output * 10.0f);
    
    Motor_SetSpeed(MOTOR_A, motor_speed_left);
    Motor_SetSpeed(MOTOR_B, motor_speed_right);
    
    // 调试输出
    if (++loop_cnt >= 100) {
        loop_cnt = 0;
        printf("PID Target:%.0f,%.0f | Current:%ld,%ld | Out:%.1f,%.1f\r\n",
               pid_left.setpoint, pid_right.setpoint,
               g_pid_cached_data.pos_left, g_pid_cached_data.pos_right,
               pid_left.output, pid_right.output);
    }
}

/*
 * ==================== 方案二：通过串口接收目标位置 ====================
 * 
 * 在 Protocol_Task 中添加命令解析，支持设置目标位置
 */

// 串口命令格式示例：
// "POS:1000,2000\r\n"  - 设置左电机目标1000，右电机目标2000
// "STOP\r\n"           - 停止位置控制

/**
 * @brief 解析位置控制命令
 * @param cmd: 命令字符串
 * @retval 0=成功, -1=失败
 */
int PID_ParsePositionCommand(char *cmd)
{
    int32_t target_left, target_right;
    
    if (strncmp(cmd, "POS:", 4) == 0) {
        // 解析 "POS:left,right" 格式
        if (sscanf(cmd + 4, "%ld,%ld", &target_left, &target_right) == 2) {
            PID_SetTargetPosition(target_left, target_right);
            return 0;
        }
    }
    else if (strcmp(cmd, "STOP") == 0) {
        PID_StopPositionControl();
        return 0;
    }
    else if (strncmp(cmd, "RESET", 5) == 0) {
        // 重置编码器并设置当前位置为0
        Encoder_Reset(ENCODER_BOTH);
        PID_SetTargetPosition(0, 0);
        return 0;
    }
    
    return -1;
}

/*
 * ==================== 方案三：PID参数优化建议 ====================
 * 
 * 当前默认参数：Kp=1.0, Ki=0.1, Kd=0.01
 * 
 * 对于位置控制，建议参数调整：
 */

// 位置控制专用PID初始化
void PID_InitForPositionControl(PID_Controller_t *pid)
{
    // 位置控制通常需要：
    // - 较大的Kp：快速响应位置误差
    // - 较小的Ki：避免积分饱和
    // - 适中的Kd：抑制超调
    
    PID_Init(pid, 
             2.0f,    // Kp: 增大比例增益，加快响应
             0.05f,   // Ki: 减小积分增益，防止超调
             0.5f     // Kd: 增大微分增益，抑制振荡
    );
    
    // 调整输出限制（根据电机特性）
    pid->output_min = -80.0f;   // 限制最大输出，避免过冲
    pid->output_max = 80.0f;
    
    // 积分限幅
    pid->integral_max = 30.0f;
}

/*
 * ==================== 完整的位置控制任务示例 ====================
 */

/**
 * @brief 完整的位置控制PID任务
 * @param p_arg: 任务参数
 * @retval None
 */
void Task_PID_Position_Control(void *p_arg)
{
    OS_ERR err;
    uint32_t loop_cnt = 0;
    
    (void)p_arg;
    
    // 等待系统初始化
    OSTimeDly(400, OS_OPT_TIME_DLY, &err);
    
    // 初始化PID系统
    if (PID_System_Init() != 0) {
        printf("PID System Init Failed!\r\n");
        OSTaskDel(NULL, &err);
        return;
    }
    
    // 初始化为位置控制参数
    PID_InitForPositionControl(&pid_left);
    PID_InitForPositionControl(&pid_right);
    
    // 默认目标为当前位置（保持不动）
    PID_SetTargetPosition(
        g_pid_cached_data.pos_left,
        g_pid_cached_data.pos_right
    );
    
    printf("PID Position Control Task Started\r\n");
    printf("Commands: POS:left,right | STOP | RESET\r\n");
    
    while (1) {
        // 处理编码器队列
        PID_Process_Encoder_Queue();
        
        // 处理MPU队列（如果需要）
        PID_Process_MPU_Queue();
        
        // 执行位置控制
        PID_Execute_Control_Improved();
        
        // 10ms周期 = 100Hz
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/*
 * ==================== 使用示例 ====================
 * 
 * 1. 初始化后，电机会保持在当前位置（因为setpoint=当前位置）
 * 
 * 2. 通过串口发送命令移动电机：
 *    POS:1000,1000\r\n  - 两个电机都移动到位置1000
 *    POS:-500,500\r\n   - 左电机到-500，右电机到500（转弯）
 *    STOP\r\n           - 立即停止
 *    RESET\r\n          - 重置编码器零点
 * 
 * 3. 电机到达目标位置后会自动停止
 * 
 * ==================== 关键修改点总结 ====================
 * 
 * 1. 必须添加 PID_SetTargetPosition() 函数
 * 2. 修改 PID_Execute_Control() 使用改进版本
 * 3. 添加位置到达检测逻辑
 * 4. 可选：添加串口命令解析
 * 5. 调整PID参数适合位置控制
 * 
 * ==================== 注意事项 ====================
 * 
 * 1. 编码器方向：确保编码器计数方向与电机转动方向一致
 *    如果不一致，PID会产生正反馈（越跑越远）
 * 
 * 2. 输出映射：PID输出范围(-100~100)需要正确映射到电机速度
 *    当前motor.c支持-1000~1000，所以乘以10
 * 
 * 3. 死区处理：电机在小电压下可能不转，可以添加死区补偿
 * 
 * 4. 积分饱和：当前代码已有积分限幅，但位置控制可能需要更严格的限制
 */

/*
 * ==================== 附加：死区补偿函数 ====================
 */

/**
 * @brief 电机死区补偿
 * @param speed: 原始速度值
 * @param deadband: 死区阈值
 * @retval 补偿后的速度值
 * @note  小信号时增加输出以克服静摩擦
 */
int16_t Motor_DeadbandCompensation(int16_t speed, int16_t deadband)
{
    if (speed > 0 && speed < deadband) {
        return deadband;
    }
    else if (speed < 0 && speed > -deadband) {
        return -deadband;
    }
    return speed;
}

// 使用示例：
// int16_t compensated_speed = Motor_DeadbandCompensation(raw_speed, 50);
// Motor_SetSpeed(MOTOR_A, compensated_speed);

/* End of File */

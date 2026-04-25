/**
 ******************************************************************************
 * @file    iic_mpu_learn.c
 * @brief   系统性学习软件IIC时序和MPU6050寄存器读写
 * @note    包含完整原理分析、时序图、代码解释和为什么这样写
 * @author  学习者
 * @date    2026-02-14
 ******************************************************************************
 * 
 * 本文档结构：
 * ===========
 * 第一部分：IIC总线基础
 *   1.1 IIC是什么
 *   1.2 IIC物理层
 *   1.3 IIC时序详解
 * 
 * 第二部分：软件IIC实现
 *   2.1 为什么用软件IIC
 *   2.2 起始信号和停止信号
 *   2.3 字节发送
 *   2.4 字节接收
 *   2.5 应答信号
 * 
 * 第三部分：MPU6050详解
 *   3.1 MPU6050是什么
 *   3.2 寄存器地图
 *   3.3 初始化流程
 *   3.4 数据读取
 * 
 * 第四部分：RTOS移植要点
 *   4.1 为什么需要互斥锁
 *   4.2 延时函数替换
 *   4.3 中断处理
 * 
 ******************************************************************************
 */

/* =============================================================================
 * 第一部分：IIC总线基础
 * ============================================================================= */

/**
 * 1.1 IIC是什么？
 * ==============
 * 
 * IIC（Inter-Integrated Circuit，集成电路互联），也叫I2C
 * 
 * 特点：
 * ------
 * 1. 双线制：SCL（时钟线） + SDA（数据线）
 * 2. 半双工：同一时间只能单向传输
 * 3. 多主从：一主多从或多主模式
 * 4. 地址寻址：7位或10位设备地址
 * 
 * 为什么叫"总线"？
 * ---------------
 * 因为多设备共享这两根线，像公交车一样，多个乘客（设备）
 * 共用一条路线（总线），通过地址（站牌）区分
 * 
 * 对比其他通信方式：
 * -----------------
 * | 方式   | 线数 | 速度    | 距离   | 特点          |
 * |--------|------|---------|--------|---------------|
 * | UART   | 2    | 慢      | 远     | 点对点        |
 * | SPI    | 4    | 快      | 短     | 全双工        |
 * | IIC    | 2    | 中等    | 短     | 多设备共享    |
 * | CAN    | 2    | 中等    | 远     | 差分信号      |
 * 
 * 本项目为什么用IIC？
 * ------------------
 * 1. MPU6050只支持IIC接口
 * 2. 只需要2根线，节省IO
 * 3. 速度足够（400kHz > 需要的传感器采样率）
 */

/**
 * 1.2 IIC物理层详解
 * =================
 * 
 * 硬件连接：
 * ---------
 * 
 *         主设备(单片机)                    从设备1(MPU6050)
 *         ┌─────────┐                      ┌─────────┐
 *         │    PB8  ├──────────┬───────────┤ SCL     │
 *         │  (SCL)  │          │           │         │
 *         │    PB9  ├──────────┼───────────┤ SDA     │
 *         │  (SDA)  │          │           │         │
 *         └─────────┘          │           └─────────┘
 *                              │
 *                         ┌────┴────┐
 *                         │ 4.7kΩ   │  上拉电阻
 *                         │  上拉   │
 *                         └────┬────┘
 *                              │
 *                            VCC(3.3V)
 * 
 * 为什么需要上拉电阻？
 * -------------------
 * 1. IIC是开漏输出（Open-Drain）
 * 2. 设备只能拉低（输出0），不能主动拉高（输出1）
 * 3. 上拉电阻提供高电平（线被拉高到3.3V）
 * 4. 任何设备都可以把线拉低（实现"线与"功能）
 * 
 * 开漏输出原理：
 * -------------
 * 正常输出：  推挽输出（Push-Pull）
 *            VCC
 *             │
 *            ┌┴┐   可以输出1或0
 *            │ │
 *            └┬┘
 *             │
 *            GND
 * 
 * 开漏输出：  Open-Drain
 *            
 *            ┌───┐   只能输出0（下拉）
 *            │   │   或高阻态（释放，由上拉电阻决定）
 *            └─┬─┘
 *              │
 *             GND
 * 
 * 阻值选择：4.7kΩ
 * ----------------
 * - 太小：电流大，功耗高
 * - 太大：上升沿慢，影响速度
 * - 4.7kΩ是标准值，平衡速度和功耗
 * 
 * 速度公式：
 * ----------
 * 上升时间 Tr ≈ 0.85 * R * C
 * 
 * R = 4.7kΩ
 * C = 总线电容（约50pF，包括引脚电容、走线电容）
 * Tr = 0.85 * 4700 * 50e-12 = 0.2μs
 * 
 * 标准模式（100kHz）要求 Tr < 1μs
 * 快速模式（400kHz）要求 Tr < 300ns
 * 
 * 实际测量：
 *          ___
 * SCL: ___|   |___
 *          ↑   ↑
 *          │   └── 下降沿（器件下拉，很快）
 *          └────── 上升沿（RC充电，较慢）
 */

/**
 * 1.3 IIC时序详解
 * ===============
 * 
 * 时序是IIC的核心，必须严格按照时序操作！
 * 
 * 总线状态定义：
 * --------------
 * 空闲状态：SCL = 高, SDA = 高
 * 起始信号：SCL = 高, SDA = 高→低
 * 停止信号：SCL = 高, SDA = 低→高
 * 数据传输：SCL = 低时改变SDA，SCL = 高时SDA稳定
 * 
 * 
 * 时序图1：起始信号和停止信号
 * ----------------------------
 * 
 * SCL: ───────┐   ┌───────┐   ┌───────┐   ┌───────────
 *             │   │       │   │       │   │
 *             └───┘       └───┘       └───┘
 * 
 * SDA: ──┐                           ┌───────────────
 *        │      ┌───────┐   ┌───────┘
 *        └──────┘       └───┘
 *         ↑              ↑
 *         │              └── 数据位（MSB first）
 *         └── 起始信号（Start Condition）
 * 
 *        [   发送数据   ]    停止信号
 * 
 * 
 * 关键时序参数（标准模式100kHz）：
 * --------------------------------
 * 符号    参数                      最小值    最大值
 * ------  ------------------------  --------  --------
 * f_SCL   SCL时钟频率               0         100kHz
 * t_BUF   总线空闲时间              4.7μs     -
 * t_HD_STA 起始信号保持时间         4.0μs     -
 * t_LOW   SCL低电平时间             4.7μs     -
 * t_HIGH  SCL高电平时间             4.0μs     -
 * t_SU_STA 起始信号建立时间         4.7μs     -
 * t_HD_DAT 数据保持时间             0         3.45μs
 * t_SU_DAT 数据建立时间             250ns     -
 * t_R     SDA和SCL上升时间          -         1μs
 * t_F     SDA和SCL下降时间          -         300ns
 * t_SU_STO 停止信号建立时间         4.0μs     -
 * 
 * 
 * 时序图2：完整的数据传输
 * -----------------------
 * 
 * SCL: ─────┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───────
 *           │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │
 *           └───┘   └───┘   └───┘   └───┘   └───┘   └───┘   └───┘   └───┘   └───┘
 * 
 * SDA: ─────┐       ┌───────┐   ┌───┐   ┌───────┐   ┌───┐   ┌───┐   ┌───────┐   ┌───────
 *      Start│       │D7     │   │D6 │   │D5     │   │D4 │   │D3 │   │D2...  │   │ACK
 *      ────┘       └───────┘   └───┘   └───────┘   └───┘   └───┘   └───────┘   └───────
 *           ↑       ↑           ↑       ↑           ↑       ↑       ↑           ↑
 *           │       │           │       │           │       │       │           └── 从机应答
 *           │       │           │       │           │       │       └── 数据位0
 *           │       │           │       │           │       └── 数据位1
 *           │       │           │       │           └── 数据位2
 *           │       │           │       └── 数据位3
 *           │       │           └── 数据位4
 *           │       └── 数据位5
 *           └── 数据位6
 * 
 * 
 * 数据在SCL高电平时必须稳定！
 * --------------------------
 * 这是IIC最重要的规则！
 * 
 * 正确：  SCL: ─────────┐     ┌───────────
 *                      │     │
 *                      └─────┘
 *         SDA: ────────────┐         ┌───
 *                          └─────────┘
 *                          ↑ ↑ ↑
 *                          │ │ └── 数据在SCL高电平期间稳定
 *                          │ └──── 数据在SCL下降沿后可变
 *                          └────── 数据在SCL上升沿前必须稳定
 * 
 * 错误：  SCL: ─────────┐     ┌───────────
 *                      │     │
 *                      └─────┘
 *         SDA: ────────┐ └─┐     ┌───────
 *                      └───┘ └───┘
 *                      ↑   ↑ ↑
 *                      │   │ └── 数据在SCL高电平时变化了！❌
 *                      └───┘
 * 
 * 为什么在SCL高电平时SDA不能变？
 * ------------------------------
 * 因为SCL高电平时的SDA变化被定义为：
 * - SCL高 + SDA从高到低 = 起始信号
 * - SCL高 + SDA从低到高 = 停止信号
 * 
 * 如果在传输数据时SDA变化，会被误解为起始或停止信号！
 */

/* =============================================================================
 * 第二部分：软件IIC实现
 * ============================================================================= */

/**
 * 2.1 为什么用软件IIC？
 * ====================
 * 
 * 硬件IIC vs 软件IIC：
 * -------------------
 * 
 * | 特性       | 硬件IIC           | 软件IIC           |
 * |------------|-------------------|-------------------|
 * | 速度       | 快（可达3.4MHz）  | 较慢（通常<1MHz） |
 * | CPU占用    | 低（DMA几乎不占） | 高（需要Bit-bang）|
 * | 灵活性     | 低（固定引脚）    | 高（任意GPIO）    |
 * | 代码量     | 小                | 较大              |
 * | 移植性     | 差（依赖硬件）    | 好（纯软件）      |
 * | 调试难度   | 难（硬件问题）    | 易（软件可控）    |
 * 
 * 本项目选择软件IIC的原因：
 * ------------------------
 * 1. 学习目的：理解IIC底层时序
 * 2. 灵活性：可以用任意GPIO
 * 3. 移植性：换平台只改宏定义
 * 4. 调试：可以用示波器看波形
 * 5. 速度足够：MPU6050最高支持400kHz
 * 
 * 什么时候用硬件IIC？
 * ------------------
 * 1. 速度要求高（>400kHz）
 * 2. CPU资源紧张
 * 3. 多个设备同时通信
 * 4. 产品线量产
 */

/**
 * 2.2 起始信号和停止信号
 * ======================
 * 
 * 代码实现：
 */

/**
 * @brief  Generate IIC start condition
 * @note   Timing: SCL high, SDA: high -> low
 *         
 *         Why this sequence?
 *         Because IIC spec defines start condition as:
 *         "A HIGH to LOW transition on the SDA line while 
 *          SCL is HIGH is a START condition"
 * 
 *         Timing diagram:
 *         SDA: ────┐
 *                  │
 *         ─────────┘
 *         SCL: ───────────────
 */
void IIC_Start_Learn(void)
{
    /* Step 1: Set SDA as output mode */
    IIC_SDA_OUT();
    
    /* Step 2: Both lines high (idle state) */
    IIC_SDA_HIGH();
    IIC_SCL_HIGH();
    
    /* Step 3: Wait for signal stable */
    /* Why delay? 
     * The bus needs time to rise due to RC effect.
     * Minimum 4.7μs for standard mode.
     * We use ~5μs delay here.
     */
    IIC_Delay();
    
    /* Step 4: SDA goes low while SCL is high */
    /* This is the START condition! */
    IIC_SDA_LOW();
    
    /* Step 5: Wait for signal stable */
    IIC_Delay();
    
    /* Step 6: SCL goes low, ready for data transmission */
    /* Why pull SCL low?
     * After start condition, master must pull SCL low
     * to indicate it's going to send data bits.
     */
    IIC_SCL_LOW();
}

/**
 * @brief  Generate IIC stop condition
 * @note   Timing: SCL high, SDA: low -> high
 *         
 *         Why this sequence?
 *         Because IIC spec defines stop condition as:
 *         "A LOW to HIGH transition on the SDA line while
 *          SCL is HIGH is a STOP condition"
 * 
 *         Timing diagram:
 *         SDA:      ┌────────────────
 *                   │
 *         ──────────┘
 *         SCL: ───────────────
 */
void IIC_Stop_Learn(void)
{
    /* Step 1: Set SDA as output mode */
    IIC_SDA_OUT();
    
    /* Step 2: SCL low, SDA low */
    /* Why SCL low first?
     * We need to prepare SDA to be low before SCL goes high.
     * If SCL is already high and we pull SDA low,
     * it might be misinterpreted as a start condition!
     */
    IIC_SCL_LOW();
    IIC_SDA_LOW();
    
    /* Step 3: Wait for signal stable */
    IIC_Delay();
    
    /* Step 4: SCL goes high */
    IIC_SCL_HIGH();
    
    /* Step 5: Wait for setup time (t_SU_STO > 4μs) */
    IIC_Delay();
    
    /* Step 6: SDA goes high while SCL is high */
    /* This is the STOP condition! */
    IIC_SDA_HIGH();
    
    /* Step 7: Wait for bus to be stable */
    /* Bus free time (t_BUF > 4.7μs) */
    IIC_Delay();
}

/**
 * 2.3 字节发送
 * ============
 * 
 * IIC发送字节：MSB（最高位）先发送！
 * 
 * 为什么是MSB first？
 * ------------------
 * 这是IIC协议规定的，所有设备必须遵守。
 * 类似大端模式（Big Endian）。
 * 
 * 例如发送字节 0x53：
 * 
 * Binary:  0  1  0  1  0  0  1  1
 *          ↑              ↑
 *          │              └── Bit 0 (LSB)
 *          └───────────────── Bit 7 (MSB) → 先发送
 * 
 * 发送顺序：Bit7, Bit6, Bit5, Bit4, Bit3, Bit2, Bit1, Bit0
 */

/**
 * @brief  Send one byte via IIC
 * @param  byte: Data to send (0x00 - 0xFF)
 * @note   Data is sent MSB first!
 * 
 *         Timing for each bit:
 *         SCL: ───────┐     ┌────────────────
 *                    │     │
 *                    └─────┘
 *         SDA: ────────────┐         ┌───────
 *                          └─────────┘
 *                          ↑
 *                          └── Data bit is stable when SCL is HIGH
 * 
 *         Why shift left?
 *         We use (byte & 0x80) to check MSB,
 *         then shift left to move next bit to MSB position.
 *         
 *         Example: send 0x53 (01010011)
 *         
 *         Iteration | byte (binary) | byte & 0x80 | SDA output | byte <<= 1
 *         ----------|---------------|-------------|------------|------------
 *         0         | 0101 0011     | 0           | LOW        | 1010 0110
 *         1         | 1010 0110     | 1 (0x80)    | HIGH       | 0100 1100
 *         2         | 0100 1100     | 0           | LOW        | 1001 1000
 *         3         | 1001 1000     | 1 (0x80)    | HIGH       | 0011 0000
 *         4         | 0011 0000     | 0           | LOW        | 0110 0000
 *         5         | 0110 0000     | 0           | LOW        | 1100 0000
 *         6         | 1100 0000     | 1 (0x80)    | HIGH       | 1000 0000
 *         7         | 1000 0000     | 1 (0x80)    | HIGH       | 0000 0000
 */
void IIC_Send_Byte_Learn(u8 byte)
{
    u8 i;
    
    /* Set SDA as output for transmitting */
    IIC_SDA_OUT();
    
    /* Pull SCL low to start data transmission */
    /* This gives us time to set up SDA before SCL goes high */
    IIC_SCL_LOW();
    
    /* Send 8 bits, MSB first */
    for (i = 0; i < 8; i++)
    {
        /* Check MSB (bit 7) */
        if (byte & 0x80)
        {
            IIC_SDA_HIGH();
        }
        else
        {
            IIC_SDA_LOW();
        }
        
        /* Shift left to move next bit to MSB position */
        byte <<= 1;
        
        /* Wait for SDA to stabilize */
        /* Data setup time (t_SU_DAT > 250ns) */
        IIC_Delay();
        
        /* SCL goes high - data is sampled */
        /* The receiver reads SDA when SCL is HIGH */
        IIC_SCL_HIGH();
        
        /* Wait for data to be sampled */
        /* SCL high time (t_HIGH > 4μs) */
        IIC_Delay();
        
        /* SCL goes low - prepare for next bit */
        IIC_SCL_LOW();
        
        /* Wait for SCL to stabilize */
        IIC_Delay();
    }
    
    /* Note: After sending 8 bits, master releases SDA
     * to prepare for ACK/NACK from slave.
     */
}

/**
 * 2.4 字节接收
 * ============
 * 
 * 接收字节：同样需要发送8个时钟脉冲
 * 与发送的区别：
 * 1. SDA设置为输入模式
 * 2. 读取SDA状态
 * 3. 左移接收到的位
 */

/**
 * @brief  Read one byte from IIC
 * @param  ack: 1=Send ACK (continue reading), 0=Send NACK (stop reading)
 * @retval Received byte
 * 
 * @note   Timing for receiving:
 *         SCL: ───────┐     ┌────────────────
 *                    │     │
 *                    └─────┘
 *         SDA: ────────────┐         ┌───────
 *              (slave)     └─────────┘
 *                          ↑
 *                          └── Master reads SDA when SCL is HIGH
 */
u8 IIC_Read_Byte_Learn(u8 ack)
{
    u8 i, byte = 0;
    
    /* Set SDA as input for receiving */
    /* Why input? 
     * Slave is driving SDA now, we need to read it.
     */
    IIC_SDA_IN();
    
    /* Receive 8 bits, MSB first */
    for (i = 0; i < 8; i++)
    {
        /* SCL goes low - slave can change data */
        IIC_SCL_LOW();
        
        /* Wait for slave to set up data */
        IIC_Delay();
        
        /* SCL goes high - data is valid */
        IIC_SCL_HIGH();
        
        /* Wait for data to stabilize */
        IIC_Delay();
        
        /* Read SDA line */
        /* Shift received byte left and add new bit */
        byte <<= 1;
        if (IIC_SDA_READ())
        {
            byte |= 0x01;
        }
        
        /* SCL goes low - prepare for next bit */
        /* SCL will go low at start of next iteration */
    }
    
    /* Send ACK or NACK */
    IIC_SDA_OUT();
    if (ack)
    {
        /* ACK = continue reading next byte */
        IIC_SDA_LOW();  /* Pull SDA low = ACK */
    }
    else
    {
        /* NACK = stop reading */
        IIC_SDA_HIGH();  /* Release SDA = NACK */
    }
    
    /* Generate ACK/NACK clock pulse */
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_Delay();
    IIC_SCL_LOW();
    
    return byte;
}

/**
 * 2.5 应答信号
 * ============
 * 
 * ACK：Acknowledge（应答）
 * NACK：Not Acknowledge（非应答）
 * 
 * 为什么需要应答？
 * ----------------
 * IIC没有专门的错误检测机制（如CRC），
 * 通过ACK/NACK机制实现简单的握手确认。
 * 
 * 应答时钟时序：
 * --------------
 * SCL: ───────┐     ┌────────────────
 *             │     │
 *             └─────┘
 * SDA: ────────────┐         ┌───────  (ACK - slave pulls low)
 *     (or high)    └─────────┘         (NACK - slave doesn't pull low)
 * 
 * ACK:  SDA被拉低（从机主动拉低）
 * NACK: SDA保持高（从机不响应或主机停止）
 */

/**
 * @brief  Wait for ACK from slave
 * @retval 0=ACK received, 1=No ACK (timeout)
 * 
 * @note   After sending address or data, slave must respond with ACK
 *         by pulling SDA low during the 9th clock cycle.
 * 
 *         Timing:
 *         SCL: ───────┐     ┌───────────────────
 *                    │     │
 *                    └─────┘
 *         SDA: ────────────┐         ┌───────────  ← Master releases SDA
 *                          └─────────┘             ← Slave pulls low (ACK)
 *                          
 *                          ↑
 *                          └── Master reads SDA during SCL HIGH
 *                              If SDA=LOW: ACK received
 *                              If SDA=HIGH: NACK (no response)
 */
u8 IIC_Wait_Ack_Learn(void)
{
    u8 timeout = 0;
    
    /* Step 1: Set SDA as input (release SDA) */
    /* Master releases SDA by setting it to high/input */
    /* Slave can now drive SDA if it wants to ACK */
    IIC_SDA_IN();
    IIC_SDA_HIGH();  /* Release the line */
    
    /* Step 2: Wait a bit for slave to respond */
    IIC_Delay();
    
    /* Step 3: SCL goes high - slave should respond now */
    IIC_SCL_HIGH();
    IIC_Delay();
    
    /* Step 4: Check if slave pulled SDA low (ACK) */
    /* If SDA is LOW: ACK received */
    /* If SDA is HIGH: NACK, slave didn't respond */
    while (IIC_SDA_READ())
    {
        timeout++;
        if (timeout > 250)
        {
            /* Timeout - no ACK received */
            /* This could mean:
             * 1. Wrong device address
             * 2. Device not connected
             * 3. Device busy
             */
            IIC_Stop_Learn();  /* Generate stop condition */
            return 1;  /* Error: No ACK */
        }
    }
    
    /* Step 5: SCL goes low, end of ACK cycle */
    IIC_SCL_LOW();
    
    return 0;  /* Success: ACK received */
}

/**
 * @brief  Send ACK (master -> slave)
 * @note   Used when master is receiving data and wants more
 */
void IIC_Ack_Learn(void)
{
    /* Pull SCL low first */
    IIC_SCL_LOW();
    
    /* Set SDA as output and pull low */
    IIC_SDA_OUT();
    IIC_SDA_LOW();  /* ACK = 0 */
    
    /* Wait for setup */
    IIC_Delay();
    
    /* SCL goes high - slave reads ACK */
    IIC_SCL_HIGH();
    IIC_Delay();
    
    /* SCL goes low - end of ACK */
    IIC_SCL_LOW();
}

/**
 * @brief  Send NACK (master -> slave)
 * @note   Used when master is receiving data and wants to stop
 */
void IIC_NAck_Learn(void)
{
    /* Pull SCL low first */
    IIC_SCL_LOW();
    
    /* Set SDA as output and release (high) */
    IIC_SDA_OUT();
    IIC_SDA_HIGH();  /* NACK = 1 */
    
    /* Wait for setup */
    IIC_Delay();
    
    /* SCL goes high - slave reads NACK */
    IIC_SCL_HIGH();
    IIC_Delay();
    
    /* SCL goes low - end of NACK */
    IIC_SCL_LOW();
}

/* =============================================================================
 * 第三部分：MPU6050详解
 * ============================================================================= */

/**
 * 3.1 MPU6050是什么？
 * ===================
 * 
 * MPU6050是InvenSense（现TDK）生产的6轴运动跟踪芯片：
 * - 3轴加速度计（Accelerometer）
 * - 3轴陀螺仪（Gyroscope）
 * - 集成温度传感器
 * - 内置DMP（数字运动处理器）
 * 
 * 为什么叫"6轴"？
 * ---------------
 * 加速度计：X轴、Y轴、Z轴（3轴）
 * 陀螺仪：  滚转(Roll)、俯仰(Pitch)、偏航(Yaw)（3轴）
 * 合计：6轴
 * 
 * 应用场景：
 * ----------
 * 1. 无人机 - 姿态稳定
 * 2. 手机 - 屏幕旋转、计步
 * 3. 游戏手柄 - 体感控制
 * 4. 机器人 - 平衡控制
 * 5. VR设备 - 头部追踪
 * 
 * 为什么用MPU6050？
 * -----------------
 * 1. 性价比高（几块钱）
 * 2. 精度足够（±250°/s 陀螺仪，±2g 加速度）
 * 3. 接口简单（IIC）
 * 4. 资料丰富
 * 5. 有DMP可以解算四元数
 */

/**
 * 3.2 寄存器地图
 * ==============
 * 
 * 重要寄存器说明：
 * 
 * ┌──────────┬─────────┬─────────────────────────────────────────┐
 * │ 地址     │ 名称    │ 说明                                    │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x00-0x0D│ 保留    │ Self-Test（出厂测试用）                 │
 * │ 0x0E-0x12│ 保留    │ 内部使用                                │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x13     │ XG_OFFS │ X轴陀螺仪偏移校准（高8位）              │
 * │ 0x14     │ XG_OFFS │ X轴陀螺仪偏移校准（低8位）              │
 * │ ...      │ ...     │ Y轴、Z轴类似                            │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x19     │ SMPLRT  │ 采样率分频器                            │
 * │          │ _DIV    │ 采样率 = 8kHz / (1 + SMPLRT_DIV)        │
 * │          │         │ 或 = 1kHz / (1 + SMPLRT_DIV)（低通使能）│
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x1A     │ CONFIG  │ 配置寄存器                              │
 * │          │         │ [2:0] DLPF_CFG: 低通滤波器配置          │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x1B     │ GYRO    │ 陀螺仪配置                              │
 * │          │ _CONFIG │ [4:3] FS_SEL: 满量程选择                │
 * │          │         │ 00=±250°/s, 01=±500°/s                 │
 * │          │         │ 10=±1000°/s, 11=±2000°/s               │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x1C     │ ACCEL   │ 加速度计配置                            │
 * │          │ _CONFIG │ [4:3] AFS_SEL: 满量程选择               │
 * │          │         │ 00=±2g, 01=±4g, 10=±8g, 11=±16g       │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x1D     │ FF_THR  │ 自由落体检测阈值                        │
 * │ 0x1E     │ FF_DUR  │ 自由落体检测持续时间                    │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x1F     │ MOT_THR │ 运动检测阈值                            │
 * │ 0x20     │ MOT_DUR │ 运动检测持续时间                        │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x21     │ ZRMOT   │ 零运动检测                              │
 * │          │ _THR    │                                         │
 * │ 0x22     │ ZRMOT   │ 零运动检测                              │
 * │          │ _DUR    │                                         │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x23     │ FIFO_EN │ FIFO使能寄存器                          │
 * │          │         │ 选择哪些数据进入FIFO                    │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x24     │ I2CMST  │ IIC主模式控制（MPU6050作为IIC主机）     │
 * │          │ _CTRL   │ 用于控制外部磁力计等设备                │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x25     │ I2CSLV0 │ IIC从机0地址                            │
 * │          │ _ADDR   │                                         │
 * │ 0x26     │ I2CSLV0 │ IIC从机0寄存器地址                      │
 * │          │ _REG    │                                         │
 * │ 0x27     │ I2CSLV0 │ IIC从机0控制                            │
 * │          │ _CTRL   │                                         │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x31     │ INT_PIN │ 中断引脚配置                            │
 * │          │ _CFG    │ [7] ACT_LO: 1=低电平有效                │
 * │          │         │ [6] OPEN: 1=开漏输出                    │
 * │          │         │ [5] LATCH: 1=直到清除前保持             │
 * │          │         │ [4] RD_CLEAR: 1=读取后清除              │
 * │          │         │ [3:0] 保留                              │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x37     │ INT_ENABLE│ 中断使能                                │
 * │          │         │ [0] DATA_RDY: 数据就绪中断              │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x3B     │ ACCEL   │ 加速度计数据起始地址                    │
 * │          │ _XOUT_H │ X轴高8位                                │
 * │ 0x3C     │ ACCEL   │ X轴低8位                                │
 * │          │ _XOUT_L │                                         │
 * │ 0x3D     │ ACCEL   │ Y轴高8位                                │
 * │          │ _YOUT_H │                                         │
 * │ 0x3E     │ ACCEL   │ Y轴低8位                                │
 * │          │ _YOUT_L │                                         │
 * │ 0x3F     │ ACCEL   │ Z轴高8位                                │
 * │          │ _ZOUT_H │                                         │
 * │ 0x40     │ ACCEL   │ Z轴低8位                                │
 * │          │ _ZOUT_L │                                         │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x41     │ TEMP    │ 温度传感器数据                          │
 * │          │ _OUT_H  │ 高8位                                   │
 * │ 0x42     │ TEMP    │ 低8位                                   │
 * │          │ _OUT_L  │                                         │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x43     │ GYRO    │ 陀螺仪数据起始地址                      │
 * │          │ _XOUT_H │ X轴高8位(Roll)                          │
 * │ 0x44     │ GYRO    │ X轴低8位                                │
 * │          │ _XOUT_L │                                         │
 * │ 0x45     │ GYRO    │ Y轴高8位(Pitch)                         │
 * │          │ _YOUT_H │                                         │
 * │ 0x46     │ GYRO    │ Y轴低8位                                │
 * │          │ _YOUT_L │                                         │
 * │ 0x47     │ GYRO    │ Z轴高8位(Yaw)                           │
 * │          │ _ZOUT_H │                                         │
 * │ 0x48     │ GYRO    │ Z轴低8位                                │
 * │          │ _ZOUT_L │                                         │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x68     │ WHO_AM  │ 设备ID寄存器（复位值0x68）              │
 * │          │ _I      │                                         │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x6B     │ PWR_MGMT│ 电源管理寄存器                          │
 * │          │ _1      │ [7] H_RESET: 1=复位所有                 │
 * │          │         │ [6] SLEEP: 1=睡眠模式                   │
 * │          │         │ [5] CYCLE: 1=循环模式                   │
 * │          │         │ [4:3] TEMP_DIS: 1=禁用温度传感器        │
 * │          │         │ [2:0] CLKSEL: 时钟源选择                │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x6C     │ PWR_MGMT│ 电源管理2                               │
 * │          │ _2      │ 单独控制各轴的待机                      │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x72-0x74│ FIFO    │ FIFO计数和读写                          │
 * ├──────────┼─────────┼─────────────────────────────────────────┤
 * │ 0x75     │ WHO_AM_I│ 设备ID（只读，值应为0x68）              │
 * └──────────┴─────────┴─────────────────────────────────────────┘
 */

/* 常用寄存器地址宏定义 */
#define MPU6050_ADDR            0x68    /* IIC地址（AD0接地） */
#define MPU6050_WHO_AM_I        0x75    /* 设备ID寄存器 */
#define MPU6050_PWR_MGMT_1      0x6B    /* 电源管理1 */
#define MPU6050_PWR_MGMT_2      0x6C    /* 电源管理2 */
#define MPU6050_SMPLRT_DIV      0x19    /* 采样率分频 */
#define MPU6050_CONFIG          0x1A    /* 配置寄存器 */
#define MPU6050_GYRO_CONFIG     0x1B    /* 陀螺仪配置 */
#define MPU6050_ACCEL_CONFIG    0x1C    /* 加速度计配置 */
#define MPU6050_FIFO_EN         0x23    /* FIFO使能 */
#define MPU6050_INT_ENABLE      0x38    /* 中断使能 */
#define MPU6050_INT_STATUS      0x3A    /* 中断状态 */
#define MPU6050_ACCEL_XOUT_H    0x3B    /* 加速度X轴高8位 */
#define MPU6050_TEMP_OUT_H      0x41    /* 温度高8位 */
#define MPU6050_GYRO_XOUT_H     0x43    /* 陀螺仪X轴高8位 */

/**
 * 3.3 MPU6050初始化流程
 * ======================
 * 
 * 为什么需要初始化？
 * -----------------
 * 芯片上电后处于睡眠模式，需要：
 * 1. 唤醒芯片
 * 2. 配置采样率
 * 3. 配置量程
 * 4. 使能传感器
 * 
 * 初始化步骤：
 */

/**
 * @brief  MPU6050 initialization
 * @retval 0=Success, 1=Failed
 * 
 * @note   Initialization sequence:
 *         1. Verify device ID (WHO_AM_I should be 0x68)
 *         2. Reset device
 *         3. Wake up from sleep
 *         4. Set clock source
 *         5. Configure sample rate
 *         6. Configure low-pass filter
 *         7. Configure gyroscope full scale
 *         8. Configure accelerometer full scale
 */
u8 MPU6050_Init_Learn(void)
{
    u8 res;
    
    /* Step 1: Reset MPU6050 */
    /* Why reset?
     * Ensure all registers are in known state
     */
    IIC_Write_Byte(MPU6050_ADDR, MPU6050_PWR_MGMT_1, 0x80);
    delay_ms(100);  /* Wait for reset to complete */
    
    /* Step 2: Wake up from sleep and set clock source */
    /* PWR_MGMT_1 register bits:
     * [7] H_RESET: 0 (no reset)
     * [6] SLEEP: 0 (wake up)
     * [5] CYCLE: 0 (no cycle mode)
     * [3] TEMP_DIS: 0 (temperature sensor enabled)
     * [2:0] CLKSEL: 001 (PLL with X axis gyro reference)
     * 
     * Why use PLL?
     * More stable clock than internal oscillator
     * Value: 0x01
     */
    IIC_Write_Byte(MPU6050_ADDR, MPU6050_PWR_MGMT_1, 0x01);
    delay_ms(10);
    
    /* Step 3: Set sample rate */
    /* Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV)
     * Gyroscope Output Rate = 8kHz (when DLPF is disabled)
     *                        or 1kHz (when DLPF is enabled)
     * 
     * We want 1kHz output rate:
     * 1kHz = 8kHz / (1 + 7)
     * So SMPLRT_DIV = 7
     * 
     * Note: When DLPF is enabled (see CONFIG register),
     *       the base rate becomes 1kHz, and with SMPLRT_DIV=0
     *       we also get 1kHz output rate.
     */
    IIC_Write_Byte(MPU6050_ADDR, MPU6050_SMPLRT_DIV, 0x00);
    
    /* Step 4: Configure low-pass filter */
    /* CONFIG register [2:0] DLPF_CFG:
     * 0 = 260Hz accel, 256Hz gyro (BW), 8kHz rate
     * 1 = 184Hz accel, 188Hz gyro
     * 2 = 94Hz accel, 98Hz gyro
     * 3 = 44Hz accel, 42Hz gyro
     * 4 = 21Hz accel, 20Hz gyro
     * 5 = 10Hz accel, 10Hz gyro
     * 6 = 5Hz accel, 5Hz gyro
     * 7 = RESERVED
     * 
     * Why use filter?
     * Reduce high-frequency noise
     * Value 3 (44Hz) is good for balancing applications
     */
    IIC_Write_Byte(MPU6050_ADDR, MPU6050_CONFIG, 0x03);
    
    /* Step 5: Configure gyroscope full scale */
    /* GYRO_CONFIG register [4:3] FS_SEL:
     * 0 = ±250°/s  -> 灵敏度 = 131 LSB/(°/s)
     * 1 = ±500°/s  -> 灵敏度 = 65.5 LSB/(°/s)
     * 2 = ±1000°/s -> 灵敏度 = 32.8 LSB/(°/s)
     * 3 = ±2000°/s -> 灵敏度 = 16.4 LSB/(°/s)
     * 
     * Larger range = lower resolution
     * For balancing: ±250°/s is usually enough
     * Value: 0x00 (or 0x18 to also set XG_ST, YG_ST, ZG_ST)
     */
    IIC_Write_Byte(MPU6050_ADDR, MPU6050_GYRO_CONFIG, 0x18);
    
    /* Step 6: Configure accelerometer full scale */
    /* ACCEL_CONFIG register [4:3] AFS_SEL:
     * 0 = ±2g  -> 灵敏度 = 16384 LSB/g
     * 1 = ±4g  -> 灵敏度 = 8192 LSB/g
     * 2 = ±8g  -> 灵敏度 = 4096 LSB/g
     * 3 = ±16g -> 灵敏度 = 2048 LSB/g
     * 
     * For angle calculation: ±2g gives best resolution
     * Value: 0x01 (or 0x00 for ±2g)
     */
    IIC_Write_Byte(MPU6050_ADDR, MPU6050_ACCEL_CONFIG, 0x01);
    
    /* Step 7: Verify WHO_AM_I */
    res = IIC_Read_Byte(MPU6050_ADDR, MPU6050_WHO_AM_I, &res);
    if (res != 0x68)
    {
        /* Device ID mismatch - wrong device or communication error */
        return 1;
    }
    
    return 0;  /* Success */
}

/**
 * 3.4 数据读取
 * ============
 * 
 * 数据结构：
 * ACCEL_XOUT_H (0x3B)  →  高8位
 * ACCEL_XOUT_L (0x3C)  →  低8位
 * 
 * 合并：accel_x = ((int16_t)ACCEL_XOUT_H << 8) | ACCEL_XOUT_L
 * 
 * 为什么是14字节？
 * ----------------
 * 加速度X: 2字节
 * 加速度Y: 2字节
 * 加速度Z: 2字节
 * 温度:    2字节
 * 陀螺仪X: 2字节
 * 陀螺仪Y: 2字节
 * 陀螺仪Z: 2字节
 * 总计:    14字节
 */

/**
 * @brief  Read all sensor data from MPU6050
 * @param  accel: Pointer to store accelerometer data [x, y, z]
 * @param  gyro: Pointer to store gyroscope data [x, y, z]
 * @param  temp: Pointer to store temperature
 * @retval 0=Success, 1=Failed
 */
u8 MPU6050_Read_Data_Learn(int16_t *accel, int16_t *gyro, float *temp)
{
    u8 buf[14];  /* Buffer for all sensor data */
    
    /* Read 14 bytes starting from ACCEL_XOUT_H (0x3B) */
    /* This is more efficient than reading each register separately */
    if (IIC_Read_Bytes(MPU6050_ADDR, MPU6050_ACCEL_XOUT_H, buf, 14))
    {
        return 1;  /* Error */
    }
    
    /* Parse accelerometer data (signed 16-bit, Big Endian) */
    /* Formula: value = (high_byte << 8) | low_byte */
    accel[0] = ((int16_t)buf[0] << 8) | buf[1];  /* X */
    accel[1] = ((int16_t)buf[2] << 8) | buf[3];  /* Y */
    accel[2] = ((int16_t)buf[4] << 8) | buf[5];  /* Z */
    
    /* Parse temperature */
    /* Formula: Temp_degC = (raw / 340.0) + 36.53 */
    int16_t raw_temp = ((int16_t)buf[6] << 8) | buf[7];
    *temp = (raw_temp / 340.0) + 36.53;
    
    /* Parse gyroscope data */
    gyro[0] = ((int16_t)buf[8]  << 8) | buf[9];   /* X (Roll) */
    gyro[1] = ((int16_t)buf[10] << 8) | buf[11];  /* Y (Pitch) */
    gyro[2] = ((int16_t)buf[12] << 8) | buf[13];  /* Z (Yaw) */
    
    return 0;  /* Success */
}

/**
 * 数据转换公式
 * =============
 * 
 * 加速度计（配置为±2g）:
 * ---------------------
 * accel_g = accel_raw / 16384.0
 * 
 * 例：accel_raw = 16384 → 1g（地球重力）
 *     accel_raw = 0     → 0g
 *     accel_raw = -8192 → -0.5g
 * 
 * 陀螺仪（配置为±250°/s）:
 * ------------------------
 * gyro_dps = gyro_raw / 131.0
 * 
 * 例：gyro_raw = 131 → 1°/s
 *     gyro_raw = 0   → 0°/s
 * 
 * 温度:
 * -----
 * temp_C = (temp_raw / 340.0) + 36.53
 */

/* =============================================================================
 * 第四部分：RTOS移植要点
 * ============================================================================= */

/**
 * 4.1 为什么需要互斥锁？
 * ======================
 * 
 * 问题场景：
 * ----------
 * 假设有两个任务都想访问MPU6050：
 * 
 * 任务A: 读取加速度
 * 任务B: 读取陀螺仪
 * 
 * 如果没有互斥锁：
 * 
 * 时间 →
 * 任务A: [开始传输] [发送地址] [被打断]          [继续-数据错乱!]
 * 任务B:            [开始传输] [发送地址] [...]
 * 
 * 任务A的数据会包含任务B传输的数据！
 * 
 * IIC总线是共享资源：
 * ------------------
 * - 一条总线可以挂多个设备
 * - 但同一时间只能有一个主设备在传输
 * - 多个任务同时访问会导致数据错乱
 * 
 * 解决方案：互斥锁
 * ----------------
 * 使用互斥锁（Mutex）保护IIC操作：
 * 
 * 任务A: [获取锁] [完整传输] [释放锁]
 * 任务B:          [等待...] [获取锁] [完整传输] [释放锁]
 * 
 * 这样任务B必须等待任务A完成才能开始。
 */

/**
 * @brief  Thread-safe IIC write for RTOS
 * @note   This function wraps the raw IIC write with mutex protection
 * 
 * Why mutex?
 * Because IIC bus is a shared resource. If multiple tasks try
 * to access the bus simultaneously, data will be corrupted.
 */
u8 IIC_Write_Byte_RTOS(u8 dev_addr, u8 reg_addr, u8 data)
{
    OS_ERR err;
    u8 result;
    
    /* Step 1: Acquire mutex */
    /* OSMutexPend blocks until the mutex is available */
    OSMutexPend(&IIC_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if (err != OS_ERR_NONE)
    {
        return 1;  /* Failed to acquire mutex */
    }
    
    /* Step 2: Perform IIC operation */
    /* Now we have exclusive access to the IIC bus */
    result = IIC_Write_Byte(dev_addr, reg_addr, data);
    
    /* Step 3: Release mutex */
    /* Always release mutex, even if operation failed */
    OSMutexPost(&IIC_Mutex, OS_OPT_POST_NONE, &err);
    
    return result;
}

/**
 * 4.2 延时函数替换
 * ================
 * 
 * 裸机 vs RTOS延时对比：
 * 
 * 裸机：
 * void delay_us(u32 us)
 * {
 *     while(us--) {
 *         for(volatile int i = 0; i < 84; i++);  // 忙等！
 *     }
 * }
 * 
 * 问题：CPU在空转，其他任务无法运行！
 * 
 * RTOS：
 * OSTimeDlyHMSM(0, 0, 0, 1, OS_OPT_TIME_HMSM_NON_STRICT, &err);
 * 
 * 优势：让出CPU，其他任务可以运行！
 * 
 * 但是软件IIC需要精确时序，怎么办？
 * ----------------------------------
 * 对于短延时（<1μs），使用空循环（volatile）
 * 对于长延时（>1ms），使用OSTimeDly()
 */

/**
 * @brief  IIC delay function for RTOS
 * @note   Short delays use busy-wait (for timing accuracy)
 *         Long delays use OS delay (for efficiency)
 */
void IIC_Delay_RTOS(void)
{
    /* For 400kHz IIC, bit period is 2.5μs
     * We need ~1μs delay here
     * At 168MHz, ~168 cycles = 1μs
     * 
     * Why volatile?
     * Prevents compiler from optimizing away the loop
     */
    volatile u16 i = 84;
    while(i--);
    
    /* Alternative for very slow IIC (not recommended):
     * OS_ERR err;
     * OSTimeDlyHMSM(0, 0, 0, 1, OS_OPT_TIME_HMSM_STRICT, &err);
     * 
     * This is too slow for IIC timing!
     */
}

/**
 * 4.3 中断处理
 * ============
 * 
 * 场景：MPU6050数据就绪中断
 * 
 * 裸机处理：
 * void EXTI_IRQHandler(void)
 * {
 *     MPU6050_Read_Data(...);  // 在中断中直接读取！❌
 *     /* 问题：
 *      * 1. IIC传输需要较多时间（几百μs）
 *      * 2. 中断中不能阻塞（不能调用OStimeDly）
 *      * 3. 影响其他中断响应
 *      */
 * }
 * 
 * RTOS处理：
 * void EXTI_IRQHandler(void)
 * {
 *     OS_ERR err;
 *     OSIntEnter();  // 通知RTOS进入中断
 *     
 *     /* Just release a semaphore or post a message */
 *     OSSemPost(&MPU6050_DataReady_Sem, OS_OPT_POST_1, &err);
 *     
 *     EXTI_ClearITPendingBit(...);
 *     OSIntExit();  // 通知RTOS退出中断
 * }
 * 
 * void Task_MPU6050_Handler(void *p_arg)
 * {
 *     while(1) {
 *         /* Wait for data ready signal */
 *         OSSemPend(&MPU6050_DataReady_Sem, 0, ...);
 *         
 *         /* Now read data in task context */
 *         MPU6050_Read_Data(...);
 *         
 *         /* Process data... */
 *     }
 * }
 */

/* =============================================================================
 * 附录：完整的数据读取示例
 * ============================================================================= */

/**
 * @brief  Complete example: MPU6050 data acquisition task
 * @note   This is a typical implementation in an RTOS environment
 */
void Task_MPU6050_Read(void *p_arg)
{
    OS_ERR err;
    int16_t accel[3], gyro[3];
    float temp;
    
    (void)p_arg;
    
    /* Wait for system to start */
    OSTimeDly(100, OS_OPT_TIME_DLY, &err);
    
    /* Initialize MPU6050 */
    while (MPU6050_Init_RTOS())
    {
        printf("MPU6050 init failed, retry...\n");
        OSTimeDly(1000, OS_OPT_TIME_DLY, &err);
    }
    
    printf("MPU6050 initialized successfully!\n");
    
    while (1)
    {
        /* Read all sensor data */
        if (MPU6050_Read_Data_RTOS(accel, gyro, &temp) == 0)
        {
            /* Convert raw data to physical units */
            float ax = accel[0] / 8192.0;   /* ±4g range */
            float ay = accel[1] / 8192.0;
            float az = accel[2] / 8192.0;
            
            float gx = gyro[0] / 65.5;      /* ±500°/s range */
            float gy = gyro[1] / 65.5;
            float gz = gyro[2] / 65.5;
            
            /* Print data (or process it) */
            printf("Accel: %.3f, %.3f, %.3f g\r\n", ax, ay, az);
            printf("Gyro:  %.3f, %.3f, %.3f dps\r\n", gx, gy, gz);
            printf("Temp:  %.1f C\r\n", temp);
        }
        
        /* Read at 100Hz (10ms interval) */
        OSTimeDly(10, OS_OPT_TIME_DLY, &err);
    }
}

/* =============================================================================
 * End of file
 ******************************************************************************/

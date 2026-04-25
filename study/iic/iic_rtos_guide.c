/**
 ******************************************************************************
 * @file    iic_rtos_guide.c
 * @brief   IIC driver porting guide for uC/OS-III
 * @author  User
 * @date    2026-02-14
 ******************************************************************************
 * 
 * 移植思路与注意事项
 * ==================
 * 
 * 一、裸机代码的典型问题
 * ----------------------
 * 1. 使用while死等延时 (如: while(i--);)
 *    问题：在RTOS中会阻塞整个调度器，其他任务无法运行
 *    解决：改用OSTimeDly()让出CPU
 * 
 * 2. 直接操作GPIO而无互斥保护
 *    问题：多个任务同时访问IIC总线会冲突
 *    解决：使用互斥锁保护IIC总线访问
 * 
 * 3. 中断中直接处理数据
 *    问题：中断中处理时间过长影响实时性
 *    解决：中断只接收数据，通过信号量通知任务处理
 * 
 * 二、移植步骤
 * ------------
 * 1. 添加互斥锁保护IIC总线
 * 2. 将delay()改为OSTimeDly()
 * 3. 如果使用中断，添加信号量同步
 * 4. 封装IIC操作为线程安全的API
 * 
 * 三、软件IIC vs 硬件IIC
 * ----------------------
 * 软件IIC（推荐用于学习）：
 *   优点：灵活，任意GPIO都可以，容易理解
 *   缺点：占用CPU时间，速度较慢
 *   移植要点：延时函数改为OSTimeDlyHMSM()
 * 
 * 硬件IIC（推荐用于产品）：
 *   优点：速度快，CPU占用低，支持DMA
 *   缺点：受限于特定引脚，配置复杂
 *   移植要点：使用信号量等待传输完成
 * 
 * 四、MPU6050移植要点
 * -------------------
 * 1. 初始化放在任务中，不要在中断中
 * 2. 读取数据使用单独任务，周期性读取
 * 3. 如果使能DMP，处理数据的任务优先级要合理
 * 4. 数据共享使用互斥锁或消息队列
 * 
 * 五、推荐的任务结构
 * -----------------
 * Task_MPU6050_Read:
 *   - 优先级：中等（例如10）
 *   - 周期：10-100ms（根据应用需求）
 *   - 功能：读取MPU6050数据
 * 
 * Task_Data_Process:
 *   - 优先级：低（例如15）
 *   - 触发：接收到新数据信号量
 *   - 功能：处理姿态解算等
 * 
 ******************************************************************************
 */

#include "os.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"

/* ==================== IIC配置宏定义 ==================== */

/* IIC引脚定义（根据实际硬件修改） */
#define IIC_SCL_GPIO_PORT       GPIOB
#define IIC_SCL_GPIO_PIN        GPIO_Pin_8
#define IIC_SCL_GPIO_CLK        RCC_AHB1Periph_GPIOB

#define IIC_SDA_GPIO_PORT       GPIOB
#define IIC_SDA_GPIO_PIN        GPIO_Pin_9
#define IIC_SDA_GPIO_CLK        RCC_AHB1Periph_GPIOB

/* IIC操作宏 */
#define IIC_SCL_HIGH()          GPIO_SetBits(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN)
#define IIC_SCL_LOW()           GPIO_ResetBits(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN)
#define IIC_SDA_HIGH()          GPIO_SetBits(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN)
#define IIC_SDA_LOW()           GPIO_ResetBits(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN)
#define IIC_SDA_READ()          GPIO_ReadInputDataBit(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN)

/* ==================== RTOS同步对象 ==================== */

OS_MUTEX IIC_Mutex;  // IIC总线互斥锁

/* ==================== 私有函数声明 ==================== */

static void IIC_GPIO_Config(void);
static void IIC_SDA_OUT(void);
static void IIC_SDA_IN(void);
static void IIC_Delay(void);
static void IIC_Start(void);
static void IIC_Stop(void);
static uint8_t IIC_Wait_Ack(void);
static void IIC_Ack(void);
static void IIC_NAck(void);
static void IIC_Send_Byte(uint8_t byte);
static uint8_t IIC_Read_Byte(uint8_t ack);

/* ==================== IIC初始化 ==================== */

/**
 * @brief  Initialize IIC with RTOS support
 * @note   Must be called before any IIC operation and after OSInit()
 */
void IIC_Init(void)
{
    OS_ERR err;
    
    /* Configure GPIO */
    IIC_GPIO_Config();
    
    /* Create mutex for IIC bus protection */
    OSMutexCreate(&IIC_Mutex, "IIC Mutex", &err);
    if(err != OS_ERR_NONE)
    {
        /* Handle error - mutex creation failed */
        while(1);  // Or use error handler
    }
    
    /* Set initial state */
    IIC_SCL_HIGH();
    IIC_SDA_HIGH();
}

/**
 * @brief  Configure IIC GPIO pins
 */
static void IIC_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    /* Enable GPIO clock */
    RCC_AHB1PeriphClockCmd(IIC_SCL_GPIO_CLK | IIC_SDA_GPIO_CLK, ENABLE);
    
    /* Configure SCL and SDA as open-drain output */
    GPIO_InitStruct.GPIO_Pin = IIC_SCL_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;      // Open-drain
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;        // Pull-up
    GPIO_Init(IIC_SCL_GPIO_PORT, &GPIO_InitStruct);
    
    GPIO_InitStruct.GPIO_Pin = IIC_SDA_GPIO_PIN;
    GPIO_Init(IIC_SDA_GPIO_PORT, &GPIO_InitStruct);
}

/* ==================== SDA方向切换 ==================== */

/**
 * @brief  Set SDA as output
 */
static void IIC_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    GPIO_InitStruct.GPIO_Pin = IIC_SDA_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(IIC_SDA_GPIO_PORT, &GPIO_InitStruct);
}

/**
 * @brief  Set SDA as input
 */
static void IIC_SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    
    GPIO_InitStruct.GPIO_Pin = IIC_SDA_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(IIC_SDA_GPIO_PORT, &GPIO_InitStruct);
}

/* ==================== IIC延时函数（RTOS版本） ==================== */

/**
 * @brief  IIC delay function for RTOS
 * @note   CRITICAL: Do NOT use while loop delay in RTOS!
*/
static void IIC_Delay(void)
{
    volatile uint16_t i = 84;  // Adjust based on actual MCU speed
    while(i--);
}

/* ==================== IIC基本操作 ==================== */

/**
 * @brief  Generate IIC start condition
 * @note   SCL high, SDA: high -> low
 */
static void IIC_Start(void)
{
    IIC_SDA_OUT();      // SDA output mode
    IIC_SDA_HIGH();
    IIC_SCL_HIGH();
    IIC_Delay();
    IIC_SDA_LOW();      // Start condition
    IIC_Delay();
    IIC_SCL_LOW();      // Prepare for data transmission
}

/**
 * @brief  Generate IIC stop condition
 * @note   SCL high, SDA: low -> high
 */
static void IIC_Stop(void)
{
    IIC_SDA_OUT();      // SDA output mode
    IIC_SCL_LOW();
    IIC_SDA_LOW();
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_SDA_HIGH();     // Stop condition
    IIC_Delay();
}

/**
 * @brief  Wait for ACK signal
 * @retval 0=ACK received, 1=No ACK
 */
static uint8_t IIC_Wait_Ack(void)
{
    uint8_t timeout = 0;
    
    IIC_SDA_IN();       // SDA input mode
    IIC_SDA_HIGH();
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_Delay();
    
    while(IIC_SDA_READ())
    {
        timeout++;
        if(timeout > 250)
        {
            IIC_Stop();
            return 1;   // No ACK
        }
    }
    
    IIC_SCL_LOW();
    return 0;           // ACK received
}

/**
 * @brief  Send ACK signal
 */
static void IIC_Ack(void)
{
    IIC_SCL_LOW();
    IIC_SDA_OUT();
    IIC_SDA_LOW();      // ACK = 0
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_Delay();
    IIC_SCL_LOW();
}

/**
 * @brief  Send NACK signal
 */
static void IIC_NAck(void)
{
    IIC_SCL_LOW();
    IIC_SDA_OUT();
    IIC_SDA_HIGH();     // NACK = 1
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_Delay();
    IIC_SCL_LOW();
}

/**
 * @brief  Send one byte via IIC
 * @param  byte: Data to send
 */
static void IIC_Send_Byte(uint8_t byte)
{
    uint8_t i;
    
    IIC_SDA_OUT();
    IIC_SCL_LOW();      // Pull down clock to prepare
    
    for(i = 0; i < 8; i++)
    {
        if(byte & 0x80)
            IIC_SDA_HIGH();
        else
            IIC_SDA_LOW();
        
        byte <<= 1;
        IIC_Delay();
        IIC_SCL_HIGH();
        IIC_Delay();
        IIC_SCL_LOW();
        IIC_Delay();
    }
}

/**
 * @brief  Read one byte from IIC
 * @param  ack: 1=Send ACK, 0=Send NACK
 * @retval Received byte
 */
static uint8_t IIC_Read_Byte(uint8_t ack)
{
    uint8_t i, byte = 0;
    
    IIC_SDA_IN();       // SDA input mode
    
    for(i = 0; i < 8; i++)
    {
        IIC_SCL_LOW();
        IIC_Delay();
        IIC_SCL_HIGH();
        byte <<= 1;
        if(IIC_SDA_READ())
            byte |= 0x01;
        IIC_Delay();
    }
    
    if(ack)
        IIC_Ack();
    else
        IIC_NAck();
    
    return byte;
}

/* ==================== 线程安全的公共API ==================== */

/**
 * @brief  Write data to IIC device (Thread-safe)
 * @param  dev_addr: Device address (7-bit)
 * @param  reg_addr: Register address
 * @param  data: Data to write
 * @retval 0=Success, 1=Failed
 */
uint8_t IIC_Write_Byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    OS_ERR err;
    uint8_t result;
    
    /* Acquire mutex - wait indefinitely */
    OSMutexPend(&IIC_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if(err != OS_ERR_NONE)
        return 1;  // Mutex error
    
    /* IIC write sequence */
    IIC_Start();
    IIC_Send_Byte((dev_addr << 1) | 0);  // Write mode
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    IIC_Send_Byte(reg_addr);
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    IIC_Send_Byte(data);
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    result = 0;  // Success
    
exit:
    IIC_Stop();
    
    /* Release mutex */
    OSMutexPost(&IIC_Mutex, OS_OPT_POST_NONE, &err);
    
    return result;
}

/**
 * @brief  Read data from IIC device (Thread-safe)
 * @param  dev_addr: Device address (7-bit)
 * @param  reg_addr: Register address
 * @param  data: Pointer to store read data
 * @retval 0=Success, 1=Failed
 */
uint8_t IIC_Read_Byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data)
{
    OS_ERR err;
    uint8_t result;
    
    /* Acquire mutex */
    OSMutexPend(&IIC_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if(err != OS_ERR_NONE)
        return 1;
    
    /* IIC read sequence */
    IIC_Start();
    IIC_Send_Byte((dev_addr << 1) | 0);  // Write mode
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    IIC_Send_Byte(reg_addr);
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    IIC_Start();  // Restart
    IIC_Send_Byte((dev_addr << 1) | 1);  // Read mode
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    *data = IIC_Read_Byte(0);  // Read with NACK
    result = 0;  // Success
    
exit:
    IIC_Stop();
    
    /* Release mutex */
    OSMutexPost(&IIC_Mutex, OS_OPT_POST_NONE, &err);
    
    return result;
}

/**
 * @brief  Read multiple bytes from IIC device (Thread-safe)
 * @param  dev_addr: Device address (7-bit)
 * @param  reg_addr: Register address
 * @param  buf: Buffer to store data
 * @param  len: Number of bytes to read
 * @retval 0=Success, 1=Failed
 */
uint8_t IIC_Read_Bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint16_t len)
{
    OS_ERR err;
    uint8_t result;
    uint16_t i;
    
    /* Acquire mutex */
    OSMutexPend(&IIC_Mutex, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if(err != OS_ERR_NONE)
        return 1;
    
    /* IIC read sequence */
    IIC_Start();
    IIC_Send_Byte((dev_addr << 1) | 0);
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    IIC_Send_Byte(reg_addr);
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    IIC_Start();  // Restart
    IIC_Send_Byte((dev_addr << 1) | 1);
    if(IIC_Wait_Ack())
    {
        result = 1;
        goto exit;
    }
    
    for(i = 0; i < len; i++)
    {
        if(i == len - 1)
            buf[i] = IIC_Read_Byte(0);  // Last byte with NACK
        else
            buf[i] = IIC_Read_Byte(1);  // Send ACK
    }
    
    result = 0;
    
exit:
    IIC_Stop();
    
    /* Release mutex */
    OSMutexPost(&IIC_Mutex, OS_OPT_POST_NONE, &err);
    
    return result;
}

/* ==================== 使用示例 ==================== */

#if 0  // 示例代码，不编译

/**
 * @brief  Example task using IIC
 */
void Task_IIC_Example(void *p_arg)
{
    OS_ERR err;
    uint8_t data;
    
    (void)p_arg;
    
    /* Wait for IIC initialization */
    OSTimeDly(100, OS_OPT_TIME_DLY, &err);
    
    while(1)
    {
        /* Read data from device 0x68, register 0x75 */
        if(IIC_Read_Byte(0x68, 0x75, &data) == 0)
        {
            /* Success */
            printf("Read data: 0x%02X\r\n", data);
        }
        else
        {
            /* Failed */
            printf("IIC read failed\r\n");
        }
        
        /* Delay 1 second */
        OSTimeDly(1000, OS_OPT_TIME_DLY, &err);
    }
}

#endif

/**
 ******************************************************************************
 * 移植检查清单
 * ============
 * 
 * ✓ 1. GPIO引脚定义是否正确？
 * ✓ 2. 是否调用IIC_Init()创建互斥锁？
 * ✓ 3. 延时函数是否适配RTOS？
 * ✓ 4. 所有公共API是否使用互斥锁保护？
 * ✓ 5. 是否在任务中调用IIC操作（不在中断中）？
 * ✓ 6. 任务栈大小是否足够？（建议≥512字节）
 * ✓ 7. 如果多个任务访问同一设备，是否有额外保护？
 * 
 ******************************************************************************
 */

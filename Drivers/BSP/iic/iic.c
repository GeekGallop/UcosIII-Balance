#include "./iic/iic.h"
#include "os.h"
#include "stddef.h"
OS_MUTEX IIC_Mutex;


/**
 * @brief  Initialize Clock and Initialize GPIO port
 * @param  None
 * @retval None
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


/**
 * @brief  IIC delay function for RTOS
 * @note   CRITICAL: Do NOT use while loop delay in RTOS!
*/
static void IIC_Delay(void)
{
    volatile uint16_t i = 84;  // Adjust based on actual MCU speed
    while(i--);
}


/**
 * @brief  Generate IIC start condition
 * @note   SCL high, SDA: high -> low
 */
static void IIC_Start(void)
{
    IIC_SDA_OUT();
    IIC_SCL_HIGH();
    IIC_SDA_HIGH();

    IIC_Delay();
    IIC_SDA_LOW();
    IIC_Delay();
    IIC_SCL_LOW();
}

/**
 * @brief  Generate IIC stop condition
 * @note   SCL high, SDA: low -> high
 */
static void IIC_Stop(void)
{
    IIC_SDA_OUT();
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
static uint8_t IIC_Read_Byte_t(uint8_t ack)
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
    
    *data = IIC_Read_Byte_t(0);  // Read with NACK
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
            buf[i] = IIC_Read_Byte_t(0);  // Last byte with NACK
        else
            buf[i] = IIC_Read_Byte_t(1);  // Send ACK
    }
    
    result = 0;
    
exit:
    IIC_Stop();
    
    /* Release mutex */
    OSMutexPost(&IIC_Mutex, OS_OPT_POST_NONE, &err);
    
    return result;
}

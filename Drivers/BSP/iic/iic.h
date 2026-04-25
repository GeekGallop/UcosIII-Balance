#ifndef __IIC_H
#define __IIC_H
#include "os.h"
#include "stddef.h"
#include "stm32f4xx_gpio.h" 

extern OS_MUTEX IIC_Mutex;




#define IIC_SCL_GPIO_PORT       GPIOB
#define IIC_SCL_GPIO_PIN        GPIO_Pin_6
#define IIC_SCL_GPIO_CLK        RCC_AHB1Periph_GPIOB

#define IIC_SDA_GPIO_PORT       GPIOB
#define IIC_SDA_GPIO_PIN        GPIO_Pin_7
#define IIC_SDA_GPIO_CLK        RCC_AHB1Periph_GPIOB


#define IIC_SCL_HIGH()          GPIO_SetBits(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN)
#define IIC_SCL_LOW()           GPIO_ResetBits(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN)
#define IIC_SDA_HIGH()          GPIO_SetBits(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN)
#define IIC_SDA_LOW()           GPIO_ResetBits(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN)
#define IIC_SDA_READ()          GPIO_ReadInputDataBit(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN)

void IIC_Init(void);
uint8_t IIC_Write_Byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
uint8_t IIC_Read_Byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);
uint8_t IIC_Read_Bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *buf, uint16_t len);

#endif

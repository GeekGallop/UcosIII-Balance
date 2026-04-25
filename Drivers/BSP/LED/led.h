#ifndef __LED_H
#define __LED_H	

#include "sys.h"


#define LED0 PBout(1)       // PA0
#define LED1 PCout(13)       // PA1

void led_config(void);        //初始化


#endif

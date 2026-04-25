#include "bsp.h"
#include "task.h"
#include "os.h"
#include "sys_monitor.h"
#define FPU_ENABLE()  (*((volatile uint32_t *)0xE000ED88) |= (0xF << 20))

int main(void)
{
	  OS_ERR err;
    FPU_ENABLE(); 
    /* Initialize uC/OS-III */
    OSInit(&err);
    /* 板级初始化 */
    bsp_init();
	
		OSStatTaskCPUUsageInit(&err);
    
    /* 任务启动 */
    app_start();
    
    return 0;
}

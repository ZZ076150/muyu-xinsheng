











#ifndef __BSP_SCREEN_H__
#define __BSP_SCREEN_H__

#include <rtthread.h>

#define SCREEN_HEADER 0xAAU
#define SCREEN_TAIL   0x55U







int  screen_feed(rt_uint8_t byte, rt_uint8_t *cmd);




rt_uint8_t screen_normalize_cmd(rt_uint8_t raw);




void screen_init(void);

#endif 

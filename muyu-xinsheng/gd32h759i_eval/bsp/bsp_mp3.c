


#include <rtthread.h>
#include "bsp_uart.h"
#include "bsp_mp3.h"

void bsp_mp3_play_track(rt_uint16_t track_num)
{
    rt_uint8_t buf[8];

    buf[0] = 0x7E;
    buf[1] = 0xFF;
    buf[2] = 0x06;
    buf[3] = 0x03;                      
    buf[4] = 0x00;
    buf[5] = (rt_uint8_t)(track_num >> 8);
    buf[6] = (rt_uint8_t)(track_num & 0xFF);
    buf[7] = 0xEF;

    bsp_uart_send(UART_PORT_MP3, buf, sizeof(buf));
    rt_thread_mdelay(100);              
}

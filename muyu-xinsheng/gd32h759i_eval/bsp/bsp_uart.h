















#ifndef __BSP_UART_H__
#define __BSP_UART_H__

#include <rtthread.h>


typedef enum
{
    UART_PORT_K230   = 0,   
    UART_PORT_ESP    = 1,   
    UART_PORT_SCREEN = 2,   
    UART_PORT_GPS    = 3,   
    UART_PORT_MP3    = 4,   
    UART_PORT_MAX
} uart_port_t;

#define UART_LINE_MAX    512    

int  bsp_uart_init(void);
int  bsp_uart_send(uart_port_t port, const void *buf, rt_size_t len);
int  bsp_uart_send_str(uart_port_t port, const char *str);
int  bsp_uart_read(uart_port_t port, char *buf, int maxlen, rt_int32_t timeout);
int  bsp_uart_read_line(uart_port_t port, char *buf, int maxlen, rt_int32_t timeout);
void bsp_uart_flush(uart_port_t port);

#endif 

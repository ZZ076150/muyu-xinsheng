/**
 * @file    bsp_uart.h
 * @brief   多路串口收发（K230 / ESP8266 / 串口屏 / GPS / DFPlayer / 天问）
 * @note    目标平台：兆易创新 GD32H759I-EVAL（GD32H7xx 固件库）
 *          采用"接收中断 + 环形缓冲 + 按行取帧"的轻量方案，便于解析文本协议
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include "gd32h7xx.h"
#include <stdint.h>
#include <stdbool.h>

/* 串口编号（与 gd32h759i_eval/README.md 的映射表一致） */
typedef enum {
    UART_K230 = 0,      /* USART0：K230 视觉模块 */
    UART_ESP8266,       /* USART1：ESP8266 联网模块 */
    UART_SCREEN,        /* USART2：淘晶驰串口屏 */
    UART_GPS,           /* USART3：ATGM3365 GPS */
    UART_DFPLAYER,      /* USART4：DFPlayer Mini */
    UART_TIANWEN,       /* USART5：天问语音播报 */
    UART_PORT_MAX
} uart_port_t;

void     bsp_uart_init_all(void);
void     bsp_uart_send(uart_port_t port, const uint8_t *data, uint16_t len);
void     bsp_uart_send_str(uart_port_t port, const char *str);
uint16_t bsp_uart_available(uart_port_t port);
/* 读取一整行（以 '\n' 结尾），返回长度；无完整行时返回 0 */
uint16_t bsp_uart_read_line(uart_port_t port, char *out, uint16_t max_len);

/* 供中断服务函数调用 */
void     bsp_uart_irq_handler(uint8_t port_index, uint8_t byte);

#endif /* BSP_UART_H */

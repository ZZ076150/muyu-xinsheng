/**
 * @file    bsp_uart.c
 * @brief   多路串口驱动实现（GD32H759I-EVAL）
 * @note    依赖 GD32H7xx 固件库。编译前请对照《GD32H759xx 用户手册》与所用固件库版本
 *          核对 RCU 时钟、GPIO 复用号（AF）与中断号。
 *
 * GD32 与 STM32 的串口命名对应关系（便于查手册）：
 *   GD32 USART0 ↔ STM32 USART1 ｜ GD32 USART1 ↔ USART2 ｜ GD32 USART2 ↔ USART3
 *   GD32 UART3  ↔ UART4        ｜ GD32 UART4  ↔ UART5  ｜ GD32 USART5 ↔ USART6
 */

#include "bsp_uart.h"
#include <string.h>

/* ---------------- 串口与引脚定义（按实际布线修改） ---------------- */
typedef struct {
    uint32_t usart;
    uint32_t usart_clk;
    /* TX */
    uint32_t tx_clk;
    uint32_t tx_port;
    uint32_t tx_pin;
    /* RX */
    uint32_t rx_clk;
    uint32_t rx_port;
    uint32_t rx_pin;
    uint32_t af;
    uint32_t baud;
} uart_hw_t;

static const uart_hw_t s_hw[UART_PORT_MAX] = {
    /* UART_K230：USART0，PA9 / PA10，AF7 */
    { USART0, RCU_USART0, RCU_GPIOA, GPIOA, GPIO_PIN_9,  RCU_GPIOA, GPIOA, GPIO_PIN_10, GPIO_AF_7, 115200 },
    /* UART_ESP8266：USART1，PA2 / PA3，AF7 */
    { USART1, RCU_USART1, RCU_GPIOA, GPIOA, GPIO_PIN_2,  RCU_GPIOA, GPIOA, GPIO_PIN_3,  GPIO_AF_7, 115200 },
    /* UART_SCREEN：USART2，PB10 / PB11，AF7 */
    { USART2, RCU_USART2, RCU_GPIOB, GPIOB, GPIO_PIN_10, RCU_GPIOB, GPIOB, GPIO_PIN_11, GPIO_AF_7, 115200 },
    /* UART_GPS：UART3，PC10 / PC11，AF8 */
    { UART3,  RCU_UART3,  RCU_GPIOC, GPIOC, GPIO_PIN_10, RCU_GPIOC, GPIOC, GPIO_PIN_11, GPIO_AF_8, 9600  },
    /* UART_DFPLAYER：UART4，PC12 / PD2，AF8 */
    { UART4,  RCU_UART4,  RCU_GPIOC, GPIOC, GPIO_PIN_12, RCU_GPIOD, GPIOD, GPIO_PIN_2,  GPIO_AF_8, 9600  },
    /* UART_TIANWEN：USART5，PC6 / PC7，AF8 */
    { USART5, RCU_USART5, RCU_GPIOC, GPIOC, GPIO_PIN_6,  RCU_GPIOC, GPIOC, GPIO_PIN_7,  GPIO_AF_8, 115200 }
};

/* ---------------- 接收环形缓冲与行缓冲 ---------------- */
#define UART_RX_BUF_SIZE    256
static volatile uint8_t  s_rx_buf[UART_PORT_MAX][UART_RX_BUF_SIZE];
static volatile uint16_t s_rx_head[UART_PORT_MAX];
static volatile uint16_t s_rx_tail[UART_PORT_MAX];
static char              s_line_buf[UART_PORT_MAX][UART_RX_BUF_SIZE];
static uint16_t          s_line_len[UART_PORT_MAX];

static void uart_pin_config(uint32_t clk, uint32_t port, uint32_t pin, uint32_t af, bool is_tx)
{
    rcu_periph_clock_enable(clk);
    gpio_af_set(port, af, pin);
    gpio_mode_set(port, GPIO_MODE_AF, GPIO_PUPD_PULLUP, pin);
    if (is_tx) {
        gpio_output_options_set(port, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, pin);
    }
}

static void uart_config(const uart_hw_t *hw)
{
    rcu_periph_clock_enable(hw->usart_clk);

    uart_pin_config(hw->tx_clk, hw->tx_port, hw->tx_pin, hw->af, true);
    uart_pin_config(hw->rx_clk, hw->rx_port, hw->rx_pin, hw->af, false);

    usart_deinit(hw->usart);
    usart_baudrate_set(hw->usart, hw->baud);
    usart_word_length_set(hw->usart, USART_WL_8BIT);
    usart_stop_bit_set(hw->usart, USART_STB_1BIT);
    usart_parity_config(hw->usart, USART_PM_NONE);
    usart_transmit_config(hw->usart, USART_TRANSMIT_ENABLE);
    usart_receive_config(hw->usart, USART_RECEIVE_ENABLE);
    usart_interrupt_enable(hw->usart, USART_INT_RBNE);   /* 接收缓冲非空中断 */
    usart_enable(hw->usart);
}

void bsp_uart_init_all(void)
{
    memset((void *)s_rx_head, 0, sizeof(s_rx_head));
    memset((void *)s_rx_tail, 0, sizeof(s_rx_tail));
    memset(s_line_len, 0, sizeof(s_line_len));

    for (uint8_t i = 0; i < UART_PORT_MAX; i++) {
        uart_config(&s_hw[i]);
    }

    /* 接收中断优先级低于步进电机脉冲定时器中断 */
    nvic_irq_enable(USART0_IRQn, 2, 0);   /* K230 */
    nvic_irq_enable(USART1_IRQn, 2, 0);   /* ESP8266 */
    nvic_irq_enable(USART2_IRQn, 2, 0);   /* 串口屏 */
    nvic_irq_enable(UART3_IRQn,  2, 0);   /* GPS */
    nvic_irq_enable(UART4_IRQn,  2, 0);   /* DFPlayer */
    nvic_irq_enable(USART5_IRQn, 2, 0);   /* 天问 */
}

void bsp_uart_send(uart_port_t port, const uint8_t *data, uint16_t len)
{
    if (port >= UART_PORT_MAX || data == NULL) { return; }
    uint32_t usart = s_hw[port].usart;
    for (uint16_t i = 0; i < len; i++) {
        usart_data_transmit(usart, data[i]);
        while (RESET == usart_flag_get(usart, USART_FLAG_TBE)) {
            /* 等待发送缓冲空 */
        }
    }
}

void bsp_uart_send_str(uart_port_t port, const char *str)
{
    if (str == NULL) { return; }
    bsp_uart_send(port, (const uint8_t *)str, (uint16_t)strlen(str));
}

uint16_t bsp_uart_available(uart_port_t port)
{
    if (port >= UART_PORT_MAX) { return 0; }
    uint16_t head = s_rx_head[port];
    uint16_t tail = s_rx_tail[port];
    return (uint16_t)((head + UART_RX_BUF_SIZE - tail) % UART_RX_BUF_SIZE);
}

uint16_t bsp_uart_read_line(uart_port_t port, char *out, uint16_t max_len)
{
    if (port >= UART_PORT_MAX || out == NULL || max_len == 0) { return 0; }

    while (bsp_uart_available(port) > 0) {
        char c = (char)s_rx_buf[port][s_rx_tail[port]];
        s_rx_tail[port] = (uint16_t)((s_rx_tail[port] + 1) % UART_RX_BUF_SIZE);

        if (c == '\r') { continue; }
        if (c == '\n') {
            uint16_t n = s_line_len[port];
            if (n == 0) { continue; }
            if (n >= max_len) { n = (uint16_t)(max_len - 1); }
            memcpy(out, s_line_buf[port], n);
            out[n] = '\0';
            s_line_len[port] = 0;
            return n;
        }
        if (s_line_len[port] < UART_RX_BUF_SIZE - 1) {
            s_line_buf[port][s_line_len[port]++] = c;
        } else {
            s_line_len[port] = 0;   /* 行过长则丢弃，防止溢出 */
        }
    }
    return 0;
}

/* 中断统一入口：写入对应端口环形缓冲 */
void bsp_uart_irq_handler(uint8_t port_index, uint8_t byte)
{
    if (port_index >= UART_PORT_MAX) { return; }
    uint16_t next = (uint16_t)((s_rx_head[port_index] + 1) % UART_RX_BUF_SIZE);
    if (next != s_rx_tail[port_index]) {
        s_rx_buf[port_index][s_rx_head[port_index]] = byte;
        s_rx_head[port_index] = next;
    }
}

/* ---------------- 中断服务函数 ---------------- */
void USART0_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE) != RESET) {
        bsp_uart_irq_handler(UART_K230, (uint8_t)usart_data_receive(USART0));
    }
}

void USART1_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART1, USART_INT_FLAG_RBNE) != RESET) {
        bsp_uart_irq_handler(UART_ESP8266, (uint8_t)usart_data_receive(USART1));
    }
}

void USART2_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART2, USART_INT_FLAG_RBNE) != RESET) {
        bsp_uart_irq_handler(UART_SCREEN, (uint8_t)usart_data_receive(USART2));
    }
}

void UART3_IRQHandler(void)
{
    if (usart_interrupt_flag_get(UART3, USART_INT_FLAG_RBNE) != RESET) {
        bsp_uart_irq_handler(UART_GPS, (uint8_t)usart_data_receive(UART3));
    }
}

void UART4_IRQHandler(void)
{
    if (usart_interrupt_flag_get(UART4, USART_INT_FLAG_RBNE) != RESET) {
        bsp_uart_irq_handler(UART_DFPLAYER, (uint8_t)usart_data_receive(UART4));
    }
}

void USART5_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART5, USART_INT_FLAG_RBNE) != RESET) {
        bsp_uart_irq_handler(UART_TIANWEN, (uint8_t)usart_data_receive(USART5));
    }
}

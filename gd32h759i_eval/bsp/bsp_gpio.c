/**
 * @file    bsp_gpio.c
 * @brief   GPIO 外设实现：红外对射（20ms 防抖）与状态指示灯
 */

#include "bsp_gpio.h"
#include "systick_delay.h"   /* 若工程中已有延时实现，可替换为自有 ms 延时 */

#define IR_GPIO_CLK     RCU_GPIOC
#define IR_GPIO_PORT    GPIOC
#define IR_PIN          GPIO_PIN_0

#define LED_GPIO_CLK    RCU_GPIOC
#define LED_GPIO_PORT   GPIOC
#define LED_PIN         GPIO_PIN_1

#define IR_DEBOUNCE_MS  20U

static bool          s_ir_stable = false;   /* 稳定状态 */
static bool          s_ir_last_raw = false; /* 上次采样 */
static uint32_t      s_ir_last_change_ms = 0;

void bsp_gpio_init(void)
{
    /* 红外对射：上拉输入 */
    rcu_periph_clock_enable(IR_GPIO_CLK);
    gpio_mode_set(IR_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, IR_PIN);

    /* LED：推挽输出，默认熄灭 */
    rcu_periph_clock_enable(LED_GPIO_CLK);
    gpio_mode_set(LED_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_output_options_set(LED_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, LED_PIN);
    gpio_bit_set(LED_GPIO_PORT, LED_PIN);
}

bool bsp_gpio_ir_state(void)
{
#if (IR_ACTIVE_LEVEL == 0)
    return (gpio_input_bit_get(IR_GPIO_PORT, IR_PIN) == RESET);
#else
    return (gpio_input_bit_get(IR_GPIO_PORT, IR_PIN) != RESET);
#endif
}

bool bsp_gpio_ir_poll(bool *changed_to)
{
    bool raw = bsp_gpio_ir_state();
    uint32_t now = get_tick_ms();   /* 需提供毫秒计时（SysTick 或定时器） */

    if (raw != s_ir_last_raw) {
        s_ir_last_raw = raw;
        s_ir_last_change_ms = now;
        return false;               /* 抖动中，暂不确认 */
    }

    if (raw != s_ir_stable && (now - s_ir_last_change_ms) >= IR_DEBOUNCE_MS) {
        s_ir_stable = raw;
        if (changed_to != NULL) { *changed_to = s_ir_stable; }
        return true;                /* 状态稳定变化，可上报 */
    }
    return false;
}

void bsp_gpio_led(bool on)
{
    if (on) {
        gpio_bit_reset(LED_GPIO_PORT, LED_PIN);   /* 低电平点亮 */
    } else {
        gpio_bit_set(LED_GPIO_PORT, LED_PIN);
    }
}

void bsp_gpio_led_toggle(void)
{
    if (gpio_output_bit_get(LED_GPIO_PORT, LED_PIN) == RESET) {
        gpio_bit_set(LED_GPIO_PORT, LED_PIN);
    } else {
        gpio_bit_reset(LED_GPIO_PORT, LED_PIN);
    }
}

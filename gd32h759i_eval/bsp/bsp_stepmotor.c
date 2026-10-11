/**
 * @file    bsp_stepmotor.c
 * @brief   步进电机（一维云台）驱动实现（GD32H759I-EVAL）
 * @note    脉冲引脚与方向/使能引脚可按实际布线修改；
 *          这里使用 TIMER1 作为脉冲节拍源，也可改用 TIMER 的 PWM 输出模式。
 */

#include "bsp_stepmotor.h"

/* ---------------- 引脚与定时器定义（按实际布线修改） ---------------- */
#define STEP_GPIO_CLK       RCU_GPIOD
#define STEP_GPIO_PORT      GPIOD
#define STEP_PIN            GPIO_PIN_3      /* 接驱动器 PUL */
#define DIR_PIN             GPIO_PIN_4      /* 接驱动器 DIR */
#define ENA_PIN             GPIO_PIN_5      /* 接驱动器 ENA */

#define STEP_TIMER          TIMER1
#define STEP_TIMER_CLK      RCU_TIMER1
#define STEP_TIMER_IRQn     TIMER1_IRQn

/* 定时器计数频率 1MHz（1 计数 = 1us） */
#define TIMER_TICK_HZ       1000000U

/* ---------------- 运行状态 ---------------- */
static volatile int32_t  s_steps_remain = 0;    /* 剩余脉冲数 */
static volatile uint8_t  s_dir = STEPMOTOR_DIR_CW;
static volatile bool     s_enabled = false;
static volatile bool     s_pulse_high = false;
static volatile uint32_t s_interval_us = PULSE_INTERVAL_MAX_US;

static void stepmotor_gpio_init(void)
{
    rcu_periph_clock_enable(STEP_GPIO_CLK);
    /* 脉冲、方向、使能均为推挽输出 */
    gpio_mode_set(STEP_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                  STEP_PIN | DIR_PIN | ENA_PIN);
    gpio_output_options_set(STEP_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ,
                            STEP_PIN | DIR_PIN | ENA_PIN);
    gpio_bit_reset(STEP_GPIO_PORT, STEP_PIN | DIR_PIN);
    gpio_bit_set(STEP_GPIO_PORT, ENA_PIN);      /* 默认关闭使能（低有效驱动器请改为 reset） */
}

static void stepmotor_timer_init(void)
{
    timer_parameter_struct tp;

    rcu_periph_clock_enable(STEP_TIMER_CLK);
    timer_deinit(STEP_TIMER);

    timer_struct_para_init(&tp);
    tp.prescaler        = (uint16_t)((SystemCoreClock / TIMER_TICK_HZ) - 1U); /* 计数周期 1us */
    tp.period           = PULSE_INTERVAL_MAX_US - 1U;
    tp.clockdivision    = TIMER_CKDIV_DIV1;
    tp.alignedmode      = TIMER_COUNTER_EDGE;
    tp.counterdirection = TIMER_COUNTER_UP;
    timer_init(STEP_TIMER, &tp);

    timer_interrupt_enable(STEP_TIMER, TIMER_INT_UP);
    nvic_irq_enable(STEP_TIMER_IRQn, 0, 0);     /* 脉冲节拍优先级最高 */
    timer_disable(STEP_TIMER);                   /* 空闲时不占 CPU */
}

void stepmotor_init(void)
{
    stepmotor_gpio_init();
    stepmotor_timer_init();
}

void stepmotor_enable(bool en)
{
    s_enabled = en;
    if (en) {
        gpio_bit_reset(STEP_GPIO_PORT, ENA_PIN);    /* 低电平使能（按驱动器实际接法调整） */
    } else {
        gpio_bit_set(STEP_GPIO_PORT, ENA_PIN);
        stepmotor_stop();
    }
}

void stepmotor_set_dir(uint8_t dir)
{
    s_dir = dir;
    if (dir == STEPMOTOR_DIR_CW) {
        gpio_bit_reset(STEP_GPIO_PORT, DIR_PIN);
    } else {
        gpio_bit_set(STEP_GPIO_PORT, DIR_PIN);
    }
}

void stepmotor_move_steps(int32_t steps)
{
    if (!s_enabled) { stepmotor_enable(true); }
    if (steps == 0) { return; }

    if (steps > 0)  { stepmotor_set_dir(STEPMOTOR_DIR_CW);  }
    else            { stepmotor_set_dir(STEPMOTOR_DIR_CCW); steps = -steps; }

    if (steps > MAX_STEPS_PER_CMD) { steps = MAX_STEPS_PER_CMD; }

    s_steps_remain = steps;
    s_interval_us  = PULSE_INTERVAL_MAX_US;      /* 从慢速起步，随后按 ACCEL_STEP_US 加速 */
    timer_disable(STEP_TIMER);
    timer_counter_value_set(STEP_TIMER, 0);
    timer_autoreload_value_set(STEP_TIMER, s_interval_us - 1U);
    timer_interrupt_flag_clear(STEP_TIMER, TIMER_INT_FLAG_UP);
    timer_enable(STEP_TIMER);
}

void stepmotor_stop(void)
{
    timer_disable(STEP_TIMER);
    s_steps_remain = 0;
    s_pulse_high = false;
    gpio_bit_reset(STEP_GPIO_PORT, STEP_PIN);
}

bool stepmotor_is_busy(void)
{
    return (s_steps_remain > 0);
}

/* TIMER 中断：每到达一次触发时间翻转一次脉冲引脚，两个中断完成一个完整脉冲 */
void stepmotor_irq_handler(void)
{
    if (timer_interrupt_flag_get(STEP_TIMER, TIMER_INT_FLAG_UP) == RESET) {
        return;
    }
    timer_interrupt_flag_clear(STEP_TIMER, TIMER_INT_FLAG_UP);

    if (s_steps_remain <= 0) {
        stepmotor_stop();
        return;
    }

    if (!s_pulse_high) {
        gpio_bit_set(STEP_GPIO_PORT, STEP_PIN);
        s_pulse_high = true;
    } else {
        gpio_bit_reset(STEP_GPIO_PORT, STEP_PIN);
        s_pulse_high = false;
        s_steps_remain--;

        /* 启动阶段按固定步长加速，达到最小间隔后匀速运行 */
        if (s_interval_us > PULSE_INTERVAL_MIN_US) {
            s_interval_us = (s_interval_us > PULSE_INTERVAL_MIN_US + ACCEL_STEP_US)
                          ? (s_interval_us - ACCEL_STEP_US)
                          : PULSE_INTERVAL_MIN_US;
            timer_autoreload_value_set(STEP_TIMER, s_interval_us - 1U);
        }
    }
}

void TIMER1_IRQHandler(void)
{
    stepmotor_irq_handler();
}

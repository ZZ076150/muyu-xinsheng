












#include <rtthread.h>
#include "board.h"
#include "bsp_config.h"
#include "bsp_stepper.h"


#if   STEP_GPIO_PORT == GPIOA
    #define STEP_RCU  RCU_GPIOA
#elif STEP_GPIO_PORT == GPIOB
    #define STEP_RCU  RCU_GPIOB
#elif STEP_GPIO_PORT == GPIOC
    #define STEP_RCU  RCU_GPIOC
#elif STEP_GPIO_PORT == GPIOD
    #define STEP_RCU  RCU_GPIOD
#elif STEP_GPIO_PORT == GPIOE
    #define STEP_RCU  RCU_GPIOE
#elif STEP_GPIO_PORT == GPIOF
    #define STEP_RCU  RCU_GPIOF
#else
    #error "STEP_GPIO_PORT 不在支持的端口列表，请在 bsp_config.h 中改用 GPIOA~GPIOF"
#endif

#if   DIR_GPIO_PORT == GPIOA
    #define DIR_RCU   RCU_GPIOA
#elif DIR_GPIO_PORT == GPIOB
    #define DIR_RCU   RCU_GPIOB
#elif DIR_GPIO_PORT == GPIOC
    #define DIR_RCU   RCU_GPIOC
#elif DIR_GPIO_PORT == GPIOD
    #define DIR_RCU   RCU_GPIOD
#elif DIR_GPIO_PORT == GPIOE
    #define DIR_RCU   RCU_GPIOE
#elif DIR_GPIO_PORT == GPIOF
    #define DIR_RCU   RCU_GPIOF
#else
    #error "DIR_GPIO_PORT 不在支持的端口列表"
#endif

#if   ENA_GPIO_PORT == GPIOA
    #define ENA_RCU   RCU_GPIOA
#elif ENA_GPIO_PORT == GPIOB
    #define ENA_RCU   RCU_GPIOB
#elif ENA_GPIO_PORT == GPIOC
    #define ENA_RCU   RCU_GPIOC
#elif ENA_GPIO_PORT == GPIOD
    #define ENA_RCU   RCU_GPIOD
#elif ENA_GPIO_PORT == GPIOE
    #define ENA_RCU   RCU_GPIOE
#elif ENA_GPIO_PORT == GPIOF
    #define ENA_RCU   RCU_GPIOF
#else
    #error "ENA_GPIO_PORT 不在支持的端口列表"
#endif


#if   STEPPER_TIMER == TIMER5
    #define STEPPER_TIMER_RCU  RCU_TIMER5
#elif STEPPER_TIMER == TIMER6
    #define STEPPER_TIMER_RCU  RCU_TIMER6
#else
    #error "请使用 TIMER5/TIMER6 基本定时器，或自行补充 RCU 时钟宏"
#endif


static volatile int32_t  g_remain_pulse = 0;    
static volatile uint32_t g_pulse_interval = MAX_PULSE_INTERVAL;
static volatile uint8_t  g_pulse_level = 0;     




int stepper_init(void)
{
    rcu_periph_clock_enable(STEP_RCU);
    rcu_periph_clock_enable(DIR_RCU);
    rcu_periph_clock_enable(ENA_RCU);

    
    gpio_mode_set(STEP_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, STEP_GPIO_PIN);
    gpio_output_options_set(STEP_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, STEP_GPIO_PIN);
    gpio_bit_reset(STEP_GPIO_PORT, STEP_GPIO_PIN);

    
    gpio_mode_set(DIR_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DIR_GPIO_PIN);
    gpio_output_options_set(DIR_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, DIR_GPIO_PIN);
    gpio_bit_reset(DIR_GPIO_PORT, DIR_GPIO_PIN);

    
    gpio_mode_set(ENA_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ENA_GPIO_PIN);
    gpio_output_options_set(ENA_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_60MHZ, ENA_GPIO_PIN);
    gpio_bit_set(ENA_GPIO_PORT, ENA_GPIO_PIN);   

    
    rcu_periph_clock_enable(STEPPER_TIMER_RCU);
    {
        timer_parameter_struct tp;
        timer_deinit(STEPPER_TIMER);
        tp.prescaler         = (STEPPER_TIMER_CLK_HZ / 1000000U) - 1U;  
        tp.period            = MAX_PULSE_INTERVAL;                       
        tp.clockdivision     = TIMER_CKDIV_DIV1;
        tp.counterdirection  = TIMER_COUNTER_UP;
        tp.alignedmode       = TIMER_COUNTER_EDGE;
        tp.repetitioncounter = 0;
        timer_init(STEPPER_TIMER, &tp);
    }

    timer_interrupt_flag_clear(STEPPER_TIMER, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(STEPPER_TIMER, TIMER_INT_UP);       
    nvic_irq_enable(STEPPER_TIMER_IRQn, 1, 0);                 
    timer_disable(STEPPER_TIMER);
    timer_interrupt_disable(STEPPER_TIMER, TIMER_INT_UP);

    rt_kprintf("[STEPPER] 初始化完成，脉冲定时器 = TIMER%d\n",
               (int)(STEPPER_TIMER == TIMER5 ? 5 : 6));
    return 0;
}




void stepper_run(int32_t pulse, rt_bool_t dir)
{
    if (pulse <= 0)
    {
        return;
    }

    
    gpio_bit_write(DIR_GPIO_PORT, DIR_GPIO_PIN, dir ? 1 : 0);
    gpio_bit_write(ENA_GPIO_PORT, ENA_GPIO_PIN, MOTOR_ENA_ON);

    g_pulse_level    = 0;
    gpio_bit_reset(STEP_GPIO_PORT, STEP_GPIO_PIN);

    g_pulse_interval = MAX_PULSE_INTERVAL;
    timer_autoreload_value_config(STEPPER_TIMER, MAX_PULSE_INTERVAL);

    g_remain_pulse = pulse;                 

    timer_interrupt_flag_clear(STEPPER_TIMER, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(STEPPER_TIMER, TIMER_INT_UP);
    timer_enable(STEPPER_TIMER);
}




void stepper_stop(void)
{
    timer_interrupt_disable(STEPPER_TIMER, TIMER_INT_UP);
    timer_disable(STEPPER_TIMER);
    g_remain_pulse = 0;
    g_pulse_level  = 0;
    gpio_bit_reset(STEP_GPIO_PORT, STEP_GPIO_PIN);
}

rt_bool_t stepper_busy(void)
{
    return (g_remain_pulse > 0) ? RT_TRUE : RT_FALSE;
}

int32_t stepper_remain(void)
{
    return g_remain_pulse;
}




void STEPPER_TIMER_IRQHANDLER(void)
{
    if (timer_interrupt_flag_get(STEPPER_TIMER, TIMER_INT_FLAG_UP) == RESET)
    {
        return;
    }
    timer_interrupt_flag_clear(STEPPER_TIMER, TIMER_INT_FLAG_UP);

    if (g_remain_pulse <= 0)
    {
        
        gpio_bit_reset(STEP_GPIO_PORT, STEP_GPIO_PIN);
        timer_interrupt_disable(STEPPER_TIMER, TIMER_INT_UP);
        g_pulse_interval = MAX_PULSE_INTERVAL;
        return;
    }

    if (g_pulse_level == 0)
    {
        
        gpio_bit_set(STEP_GPIO_PORT, STEP_GPIO_PIN);
        g_pulse_level = 1;
    }
    else
    {
        
        gpio_bit_reset(STEP_GPIO_PORT, STEP_GPIO_PIN);
        g_pulse_level = 0;
        g_remain_pulse--;

        if (g_remain_pulse > 0)
        {
            if (g_pulse_interval > MIN_PULSE_INTERVAL)
            {
                g_pulse_interval -= ACCEL_STEP;
                if (g_pulse_interval < MIN_PULSE_INTERVAL)
                {
                    g_pulse_interval = MIN_PULSE_INTERVAL;
                }
                timer_autoreload_value_config(STEPPER_TIMER, g_pulse_interval);
            }
        }
    }
}

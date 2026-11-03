




#include <rtthread.h>
#include "board.h"
#include "bsp_config.h"
#include "bsp_ir.h"

#if   IR_GPIO_PORT == GPIOA
    #define IR_RCU  RCU_GPIOA
#elif IR_GPIO_PORT == GPIOB
    #define IR_RCU  RCU_GPIOB
#elif IR_GPIO_PORT == GPIOC
    #define IR_RCU  RCU_GPIOC
#elif IR_GPIO_PORT == GPIOD
    #define IR_RCU  RCU_GPIOD
#elif IR_GPIO_PORT == GPIOE
    #define IR_RCU  RCU_GPIOE
#elif IR_GPIO_PORT == GPIOF
    #define IR_RCU  RCU_GPIOF
#else
    #error "IR_GPIO_PORT 不在支持的端口列表"
#endif

int bsp_ir_init(void)
{
    rcu_periph_clock_enable(IR_RCU);
    gpio_mode_set(IR_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, IR_GPIO_PIN);
    return 0;
}

int bsp_ir_read(void)
{
    return (gpio_input_bit_get(IR_GPIO_PORT, IR_GPIO_PIN) == SET) ? 1 : 0;
}

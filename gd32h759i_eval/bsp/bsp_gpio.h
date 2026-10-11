/**
 * @file    bsp_gpio.h
 * @brief   GPIO 外设：红外对射（药盒开合检测，带防抖）与状态指示灯
 */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "gd32h7xx.h"
#include <stdint.h>
#include <stdbool.h>

/* 红外对射：遮挡（药盒打开）时输出低电平；LED 低电平点亮，按实际接法调整 */
#define IR_ACTIVE_LEVEL     0

void bsp_gpio_init(void);
/* 读取红外对射的原始电平状态（已按有效电平归一化：true = 遮挡/药盒打开） */
bool bsp_gpio_ir_state(void);
/* 轮询式防抖：状态稳定变化时返回 true，并通过 changed_to 返回新状态 */
bool bsp_gpio_ir_poll(bool *changed_to);
void bsp_gpio_led(bool on);
void bsp_gpio_led_toggle(void);

#endif /* BSP_GPIO_H */

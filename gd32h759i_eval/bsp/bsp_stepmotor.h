/**
 * @file    bsp_stepmotor.h
 * @brief   步进电机（一维云台）驱动：脉冲输出 + 方向/使能控制
 * @note    目标平台：GD32H759I-EVAL；驱动器：ATK-2MD5050；电机：ATK-S42H40D20
 *          采用"定时器中断翻转脉冲引脚"的方式产生脉冲，脉冲个数决定转角，
 *          脉冲间隔决定转速（数值越小越快）。
 */

#ifndef BSP_STEPMOTOR_H
#define BSP_STEPMOTOR_H

#include "gd32h7xx.h"
#include <stdint.h>
#include <stdbool.h>

/* 机械参数：驱动器为 32 细分时，一圈 6400 脉冲 */
#define STEPS_PER_REV           6400
/* 速度参数：最小/最大脉冲间隔（微秒）与每次加速步长 */
#define PULSE_INTERVAL_MIN_US   12
#define PULSE_INTERVAL_MAX_US   60
#define ACCEL_STEP_US           2
/* 单次指令的最大步数限制（防止异常坐标导致云台飞转） */
#define MAX_STEPS_PER_CMD       1600

/* 方向定义 */
#define STEPMOTOR_DIR_CW        0   /* 顺时针（画面内向右） */
#define STEPMOTOR_DIR_CCW       1   /* 逆时针（画面内向左） */

void    stepmotor_init(void);
void    stepmotor_enable(bool en);
void    stepmotor_set_dir(uint8_t dir);
/* 相对移动：steps > 0 表示按当前方向前进，内部会累加剩余步数 */
void    stepmotor_move_steps(int32_t steps);
void    stepmotor_stop(void);
bool    stepmotor_is_busy(void);
/* 由 TIMER 中断服务函数调用 */
void    stepmotor_irq_handler(void);

#endif /* BSP_STEPMOTOR_H */

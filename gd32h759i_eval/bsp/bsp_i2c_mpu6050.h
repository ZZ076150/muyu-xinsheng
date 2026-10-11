/**
 * @file    bsp_i2c_mpu6050.h
 * @brief   MPU6050 姿态传感器（I2C0）：设备翻倒检测
 */

#ifndef BSP_I2C_MPU6050_H
#define BSP_I2C_MPU6050_H

#include "gd32h7xx.h"
#include <stdint.h>
#include <stdbool.h>

/* 翻倒判定阈值：倾斜角超过该值视为设备翻倒（单位：度） */
#define TILT_ANGLE_THRESHOLD    60.0f

bool mpu6050_init(void);
bool mpu6050_read_accel(float *ax, float *ay, float *az);
/* 返回 true 表示设备处于翻倒状态 */
bool mpu6050_is_overturned(void);

#endif /* BSP_I2C_MPU6050_H */

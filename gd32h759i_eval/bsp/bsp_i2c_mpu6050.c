/**
 * @file    bsp_i2c_mpu6050.c
 * @brief   MPU6050 姿态采集实现（GD32H759I-EVAL，硬件 I2C0）
 * @note    地址 0x68（AD0 接地）；量程 ±2g；无需 DMP，直接读取加速度寄存器
 */

#include "bsp_i2c_mpu6050.h"
#include <math.h>

#define MPU_I2C                 I2C0
#define MPU_I2C_CLK             RCU_I2C0
#define MPU_GPIO_CLK            RCU_GPIOB
#define MPU_GPIO_PORT           GPIOB
#define MPU_SCL_PIN             GPIO_PIN_6
#define MPU_SDA_PIN             GPIO_PIN_7
#define MPU_GPIO_AF             GPIO_AF_4
#define MPU_I2C_ADDR            0x68

#define MPU_REG_WHO_AM_I        0x75
#define MPU_REG_PWR_MGMT_1      0x6B
#define MPU_REG_ACCEL_CONFIG    0x1C
#define MPU_REG_ACCEL_XOUT_H    0x3B

#define MPU_ACCEL_LSB_PER_G     16384.0f    /* ±2g 量程 */
#define MPU_I2C_TIMEOUT         100000U

static bool mpu_write_reg(uint8_t reg, uint8_t val)
{
    uint32_t timeout = MPU_I2C_TIMEOUT;

    i2c_start_on_bus(MPU_I2C);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_SBSEND) && timeout--) { }
    i2c_master_addressing(MPU_I2C, MPU_I2C_ADDR << 1, I2C_TRANSMITTER);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_ADDSEND) && timeout--) { }
    i2c_flag_clear(MPU_I2C, I2C_FLAG_ADDSEND);

    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_TBE) && timeout--) { }
    i2c_data_transmit(MPU_I2C, reg);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_TBE) && timeout--) { }
    i2c_data_transmit(MPU_I2C, val);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_BTC) && timeout--) { }
    i2c_stop_on_bus(MPU_I2C);

    return (timeout > 0);
}

static bool mpu_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint32_t timeout = MPU_I2C_TIMEOUT;

    i2c_start_on_bus(MPU_I2C);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_SBSEND) && timeout--) { }
    i2c_master_addressing(MPU_I2C, MPU_I2C_ADDR << 1, I2C_TRANSMITTER);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_ADDSEND) && timeout--) { }
    i2c_flag_clear(MPU_I2C, I2C_FLAG_ADDSEND);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_TBE) && timeout--) { }
    i2c_data_transmit(MPU_I2C, reg);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_BTC) && timeout--) { }

    i2c_start_on_bus(MPU_I2C);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_SBSEND) && timeout--) { }
    i2c_master_addressing(MPU_I2C, MPU_I2C_ADDR << 1, I2C_RECEIVER);
    while (!i2c_flag_get(MPU_I2C, I2C_FLAG_ADDSEND) && timeout--) { }
    i2c_flag_clear(MPU_I2C, I2C_FLAG_ADDSEND);

    for (uint8_t i = 0; i < len; i++) {
        if (i == (uint8_t)(len - 1)) {
            i2c_ack_config(MPU_I2C, I2C_ACK_DISABLE);   /* 最后一个字节发 NACK */
        }
        while (!i2c_flag_get(MPU_I2C, I2C_FLAG_RBNE) && timeout--) { }
        buf[i] = (uint8_t)i2c_data_receive(MPU_I2C);
    }
    i2c_ack_config(MPU_I2C, I2C_ACK_ENABLE);
    i2c_stop_on_bus(MPU_I2C);

    return (timeout > 0);
}

static void mpu_i2c_gpio_init(void)
{
    rcu_periph_clock_enable(MPU_GPIO_CLK);
    /* 开漏 + 上拉，符合 I2C 电气要求 */
    gpio_af_set(MPU_GPIO_PORT, MPU_GPIO_AF, MPU_SCL_PIN | MPU_SDA_PIN);
    gpio_mode_set(MPU_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, MPU_SCL_PIN | MPU_SDA_PIN);
    gpio_output_options_set(MPU_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_60MHZ, MPU_SCL_PIN | MPU_SDA_PIN);
}

bool mpu6050_init(void)
{
    uint8_t who = 0;

    rcu_periph_clock_enable(MPU_I2C_CLK);
    mpu_i2c_gpio_init();

    i2c_deinit(MPU_I2C);
    i2c_clock_config(MPU_I2C, 400000, I2C_DTCY_2);   /* 400kHz 快速模式 */
    i2c_mode_addr_config(MPU_I2C, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x00);
    i2c_enable(MPU_I2C);
    i2c_ack_config(MPU_I2C, I2C_ACK_ENABLE);

    if (!mpu_read_regs(MPU_REG_WHO_AM_I, &who, 1)) { return false; }
    if (who != 0x68 && who != 0x70) { return false; }   /* 兼容 MPU6050 / MPU6500 */

    mpu_write_reg(MPU_REG_PWR_MGMT_1, 0x00);    /* 解除休眠，内部时钟 */
    mpu_write_reg(MPU_REG_ACCEL_CONFIG, 0x00);  /* ±2g */
    return true;
}

bool mpu6050_read_accel(float *ax, float *ay, float *az)
{
    uint8_t buf[6];

    if (!mpu_read_regs(MPU_REG_ACCEL_XOUT_H, buf, 6)) { return false; }

    int16_t raw_x = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t raw_y = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t raw_z = (int16_t)((buf[4] << 8) | buf[5]);

    if (ax) { *ax = raw_x / MPU_ACCEL_LSB_PER_G; }
    if (ay) { *ay = raw_y / MPU_ACCEL_LSB_PER_G; }
    if (az) { *az = raw_z / MPU_ACCEL_LSB_PER_G; }
    return true;
}

bool mpu6050_is_overturned(void)
{
    float ax, ay, az;

    if (!mpu6050_read_accel(&ax, &ay, &az)) { return false; }

    /* 以重力方向与设备 Z 轴的夹角判断倾斜程度 */
    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm < 0.01f) { return false; }

    float cos_theta = az / norm;
    if (cos_theta > 1.0f) { cos_theta = 1.0f; }
    if (cos_theta < -1.0f) { cos_theta = -1.0f; }

    float tilt_deg = acosf(cos_theta) * 57.29578f;
    return (tilt_deg > TILT_ANGLE_THRESHOLD);
}

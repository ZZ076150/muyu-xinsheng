












#include <rtthread.h>
#include <rtdevice.h>
#include "bsp_config.h"
#include "bsp_mpu6050.h"

#define MPU6050_REG_WHO_AM_I    0x75
#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_CONFIG      0x1A
#define MPU6050_REG_GYRO_CONFIG 0x1B
#define MPU6050_REG_ACCEL_CFG   0x1C
#define MPU6050_REG_ACCEL_XOUT  0x3B


#define MPU6050_ACCEL_SENS      4096.0f

static struct rt_i2c_bus_device *g_i2c = RT_NULL;

static int mpu_write_reg(rt_uint8_t reg, rt_uint8_t val)
{
    rt_uint8_t buf[2] = {reg, val};
    struct rt_i2c_msg msg;

    
    msg.addr  = MPU6050_ADDR;
    msg.flags = RT_I2C_WR;
    msg.len   = 2;
    msg.buf   = buf;
    return (rt_i2c_transfer(g_i2c, &msg, 1) == 1) ? 0 : -1;
}

static int mpu_read_regs(rt_uint8_t reg, rt_uint8_t *buf, rt_uint16_t len)
{
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = MPU6050_ADDR;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;

    msgs[1].addr  = MPU6050_ADDR;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].len   = len;
    msgs[1].buf   = buf;

    return (rt_i2c_transfer(g_i2c, msgs, 2) == 2) ? 0 : -1;
}

int bsp_mpu6050_init(void)
{
    rt_uint8_t who = 0;

    g_i2c = (struct rt_i2c_bus_device *)rt_device_find(MPU6050_I2C_DEV);
    if (g_i2c == RT_NULL)
    {
        rt_kprintf("[MPU6050] 未找到 I2C 设备 %s，请检查 BSP 是否使能该 I2C 总线\n",
                   MPU6050_I2C_DEV);
        return -1;
    }

    if (mpu_read_regs(MPU6050_REG_WHO_AM_I, &who, 1) != 0 || who != 0x68)
    {
        rt_kprintf("[MPU6050] WHO_AM_I=0x%02X，设备未响应或型号不符\n", who);
        return -2;
    }

    
    if (mpu_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00) != 0)
    {
        return -3;
    }
    rt_thread_mdelay(10);

    
    mpu_write_reg(MPU6050_REG_ACCEL_CFG, 0x10);
    mpu_write_reg(MPU6050_REG_GYRO_CONFIG, 0x08);
    mpu_write_reg(MPU6050_REG_CONFIG, 0x04);

    rt_kprintf("[MPU6050] 初始化完成（±8g / ±500dps / DLPF 21Hz）\n");
    return 0;
}

int bsp_mpu6050_read_accel(float *ax, float *ay, float *az)
{
    rt_uint8_t raw[6];
    rt_int16_t rax, ray, raz;

    if (g_i2c == RT_NULL)
    {
        return -1;
    }
    if (mpu_read_regs(MPU6050_REG_ACCEL_XOUT, raw, 6) != 0)
    {
        return -1;
    }

    rax = (rt_int16_t)((raw[0] << 8) | raw[1]);
    ray = (rt_int16_t)((raw[2] << 8) | raw[3]);
    raz = (rt_int16_t)((raw[4] << 8) | raw[5]);

    *ax = (float)rax / MPU6050_ACCEL_SENS;
    *ay = (float)ray / MPU6050_ACCEL_SENS;
    *az = (float)raz / MPU6050_ACCEL_SENS;
    return 0;
}

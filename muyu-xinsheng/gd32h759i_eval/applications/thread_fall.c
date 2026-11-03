







#include <rtthread.h>
#include <math.h>
#include "bsp_uart.h"
#include "bsp_mpu6050.h"

#define TILT_ANGLE_THRESHOLD   45.0f
#define FALL_DURATION_MS       1000U
#define FALL_CHECK_INTERVAL_MS 100U

static rt_bool_t   g_mpu_ready      = RT_FALSE;
static rt_bool_t   g_fall_detected  = RT_FALSE;
static rt_bool_t   g_fall_sent      = RT_FALSE;
static rt_uint32_t g_fall_start     = 0;

void thread_fall_entry(void *param)
{
    (void)param;

    if (bsp_mpu6050_init() == 0)
    {
        g_mpu_ready = RT_TRUE;
    }
    else
    {
        rt_kprintf("【MPU】初始化失败，摔倒检测停用（请检查 I2C 接线与 bus 配置）\n");
    }

    while (1)
    {
        rt_uint32_t now = rt_tick_get_millisecond();
        float ax, ay, az, mag, tilt;

        if (!g_mpu_ready)
        {
            rt_thread_mdelay(FALL_CHECK_INTERVAL_MS);
            continue;
        }

        if (bsp_mpu6050_read_accel(&ax, &ay, &az) != 0)
        {
            rt_thread_mdelay(FALL_CHECK_INTERVAL_MS);
            continue;
        }

        mag = sqrtf(ax * ax + ay * ay + az * az);
        if (mag < 0.001f)
        {
            rt_thread_mdelay(FALL_CHECK_INTERVAL_MS);
            continue;
        }

        tilt = acosf(az / mag) * 180.0f / 3.14159265f;

        if (tilt > TILT_ANGLE_THRESHOLD)
        {
            if (!g_fall_detected)
            {
                g_fall_detected = RT_TRUE;
                g_fall_start    = now;
                g_fall_sent     = RT_FALSE;
            }
            if (!g_fall_sent && (now - g_fall_start >= FALL_DURATION_MS))
            {
                bsp_uart_send_str(UART_PORT_ESP, "DEVICE_FALL_DETECTED\r\n");
                rt_kprintf("【MPU】检测到倾倒，发送 DEVICE_FALL_DETECTED\n");
                g_fall_sent = RT_TRUE;
            }
        }
        else
        {
            g_fall_detected = RT_FALSE;
            g_fall_sent     = RT_FALSE;
        }

        rt_thread_mdelay(FALL_CHECK_INTERVAL_MS);
    }
}

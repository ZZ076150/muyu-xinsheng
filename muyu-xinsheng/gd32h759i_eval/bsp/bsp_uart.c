








#include <rtthread.h>
#include <rtdevice.h>
#include <string.h>
#include "bsp_uart.h"
#include "bsp_config.h"


typedef struct
{
    const char  *dev_name;
    uint32_t     baud;
    rt_device_t  dev;
    rt_sem_t     sem;
    char         line[UART_LINE_MAX];
    rt_uint16_t  len;
} uart_obj_t;

static uart_obj_t g_uart[UART_PORT_MAX] =
{
    {K230_UART_DEV,   K230_UART_BAUDRATE,   RT_NULL, RT_NULL, {0}, 0},  
    {ESP_UART_DEV,    ESP_UART_BAUDRATE,    RT_NULL, RT_NULL, {0}, 0},  
    {SCREEN_UART_DEV, SCREEN_UART_BAUDRATE, RT_NULL, RT_NULL, {0}, 0},  
    {GPS_UART_DEV,    GPS_UART_BAUDRATE,    RT_NULL, RT_NULL, {0}, 0},  
    {MP3_UART_DEV,    MP3_UART_BAUDRATE,    RT_NULL, RT_NULL, {0}, 0},  
};


static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    int i;
    for (i = 0; i < UART_PORT_MAX; i++)
    {
        if (g_uart[i].dev == dev)
        {
            rt_sem_release(g_uart[i].sem);
            break;
        }
    }
    return RT_EOK;
}





int bsp_uart_init(void)
{
    int i;
    for (i = 0; i < UART_PORT_MAX; i++)
    {
        struct serial_configure cfg = RT_SERIAL_CONFIG_DEFAULT;
        char sem_name[16];

        g_uart[i].dev = rt_device_find(g_uart[i].dev_name);
        if (g_uart[i].dev == RT_NULL)
        {
            rt_kprintf("[UART] %s 设备未找到，请检查 BSP menuconfig 是否使能该串口\n",
                       g_uart[i].dev_name);
            return -1;
        }

        cfg.baud_rate = g_uart[i].baud;
        rt_device_control(g_uart[i].dev, RT_DEVICE_CTRL_CONFIG, &cfg);

        rt_snprintf(sem_name, sizeof(sem_name), "sem_%d", i);
        g_uart[i].sem = rt_sem_create(sem_name, 0, RT_IPC_FLAG_FIFO);
        if (g_uart[i].sem == RT_NULL)
        {
            return -2;
        }

        rt_device_set_rx_indicate(g_uart[i].dev, uart_rx_ind);
        rt_device_open(g_uart[i].dev, RT_DEVICE_FLAG_INT_RX);

        rt_kprintf("[UART] %s 就绪 baud=%d\n", g_uart[i].dev_name, g_uart[i].baud);
    }
    return 0;
}




int bsp_uart_send(uart_port_t port, const void *buf, rt_size_t len)
{
    if (port >= UART_PORT_MAX || g_uart[port].dev == RT_NULL)
    {
        return 0;
    }
    return rt_device_write(g_uart[port].dev, 0, buf, len);
}




int bsp_uart_send_str(uart_port_t port, const char *str)
{
    return bsp_uart_send(port, str, rt_strlen(str));
}






int bsp_uart_read(uart_port_t port, char *buf, int maxlen, rt_int32_t timeout)
{
    uart_obj_t *u;
    int n = 0;

    if (port >= UART_PORT_MAX || g_uart[port].dev == RT_NULL)
    {
        return 0;
    }
    u = &g_uart[port];

    while (n < maxlen)
    {
        if (rt_device_read(u->dev, -1, buf + n, 1) != 1)
        {
            break;
        }
        n++;
    }

    if (n == 0 && rt_sem_take(u->sem, timeout) == RT_EOK)
    {
        while (n < maxlen)
        {
            if (rt_device_read(u->dev, -1, buf + n, 1) != 1)
            {
                break;
            }
            n++;
        }
    }
    return n;
}









int bsp_uart_read_line(uart_port_t port, char *buf, int maxlen, rt_int32_t timeout)
{
    uart_obj_t *u = &g_uart[port];
    char ch;

    if (port >= UART_PORT_MAX || u->dev == RT_NULL)
    {
        return -1;
    }

    while (1)
    {
        
        while (rt_device_read(u->dev, -1, &ch, 1) == 1)
        {
            if (ch == '\r')
            {
                continue;
            }
            if (ch == '\n')
            {
                int n = u->len;
                n = (n > maxlen - 1) ? (maxlen - 1) : n;
                memcpy(buf, u->line, n);
                buf[n] = '\0';
                u->len = 0;
                return n;
            }
            if (u->len < UART_LINE_MAX - 1)
            {
                u->line[u->len++] = ch;
            }
            else
            {
                u->len = 0;     
            }
        }

        
        if (rt_sem_take(u->sem, timeout) != RT_EOK)
        {
            return -1;
        }
    }
}




void bsp_uart_flush(uart_port_t port)
{
    uart_obj_t *u = &g_uart[port];
    char ch;

    if (port >= UART_PORT_MAX || u->dev == RT_NULL)
    {
        return;
    }
    while (rt_device_read(u->dev, -1, &ch, 1) == 1)
    {
        
    }
    u->len = 0;
}












#include <rtthread.h>
#include <string.h>
#include "bsp_uart.h"
#include "task_pantilt.h"

#define NET_BUF_SIZE   128
#define NET_TIMEOUT    rt_tick_from_millisecond(100)

static char g_line[NET_BUF_SIZE];

void thread_net_entry(void *param)
{
    (void)param;

    while (1)
    {
        int ret = bsp_uart_read_line(UART_PORT_ESP, g_line, NET_BUF_SIZE, NET_TIMEOUT);

        if (ret > 0)
        {
            if (strcmp(g_line, "start") == 0)
            {
                pantilt_set_auto_search(RT_TRUE);
                rt_kprintf("【ESP命令】收到 start → 开启自动搜索\n");
            }
            else if (strcmp(g_line, "stop") == 0)
            {
                pantilt_set_auto_search(RT_FALSE);
                rt_kprintf("【ESP命令】收到 stop → 关闭自动搜索，停止云台\n");
            }
        }
        else
        {
            rt_thread_mdelay(10);
        }
    }
}

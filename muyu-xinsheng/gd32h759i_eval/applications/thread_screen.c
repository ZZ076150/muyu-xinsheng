









#include <rtthread.h>
#include <string.h>
#include "bsp_uart.h"
#include "bsp_screen.h"
#include "bsp_mp3.h"
#include "task_pantilt.h"

#define SCREEN_BUF_SIZE   64
#define SCREEN_POLL_MS    20


static const char g_need_forward[] =
    {'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
     '1', '2', '3', '4', '5', '6'};

static rt_bool_t is_need_forward(rt_uint8_t c)
{
    rt_size_t i;
    for (i = 0; i < sizeof(g_need_forward); i++)
    {
        if ((rt_uint8_t)g_need_forward[i] == c)
        {
            return RT_TRUE;
        }
    }
    return RT_FALSE;
}


static void handle_screen_cmd(rt_uint8_t raw_cmd)
{
    rt_uint8_t cmd = screen_normalize_cmd(raw_cmd);
    rt_bool_t  local_exec = RT_FALSE;
    char forward[3];

    rt_kprintf("=========================================\n");
    rt_kprintf("【串口屏】原始字节 0x%02X → 归一化后: %c (0x%02X)\n",
               raw_cmd, (char)cmd, cmd);

    switch (cmd)
    {
    case 'a':  rt_kprintf("→MP3 播放 0001 号歌曲\n"); bsp_mp3_play_track(1); local_exec = RT_TRUE; break;
    case 'b':  rt_kprintf("→MP3 播放 0003 号歌曲\n"); bsp_mp3_play_track(3); local_exec = RT_TRUE; break;
    case 'c':  rt_kprintf("→MP3 播放 0002 号歌曲\n"); bsp_mp3_play_track(2); local_exec = RT_TRUE; break;
    case 'd':  rt_kprintf("→MP3 播放 0004 号歌曲\n"); bsp_mp3_play_track(4); local_exec = RT_TRUE; break;
    case 'e':
        pantilt_set_auto_search(RT_TRUE);
        rt_kprintf("→收到串口屏 e，开启自动云台搜索\n");
        local_exec = RT_TRUE;
        break;
    default:
        local_exec = RT_FALSE;
        break;
    }

    if (is_need_forward(cmd))
    {
        forward[0] = (char)cmd;
        forward[1] = '\r';
        forward[2] = '\n';
        bsp_uart_send(UART_PORT_ESP, forward, 3);
        rt_kprintf("→转发 ESP8266: %c\n", (char)cmd);
    }

    if (!local_exec && !is_need_forward(cmd))
    {
        rt_kprintf("无效命令 0x%02X\n", cmd);
    }
    rt_kprintf("=========================================\n");
}

void thread_screen_entry(void *param)
{
    char buf[SCREEN_BUF_SIZE];

    (void)param;
    screen_init();

    while (1)
    {
        int ret = bsp_uart_read(UART_PORT_SCREEN, buf, sizeof(buf),
                                rt_tick_from_millisecond(SCREEN_POLL_MS));
        int i;

        for (i = 0; i < ret; i++)
        {
            rt_uint8_t cmd;
            if (screen_feed((rt_uint8_t)buf[i], &cmd) == 1)
            {
                handle_screen_cmd(cmd);
            }
        }
    }
}

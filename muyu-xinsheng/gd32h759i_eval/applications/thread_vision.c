










#include <rtthread.h>
#include <string.h>
#include <stdlib.h>
#include "bsp_uart.h"
#include "task_pantilt.h"

#define VISION_BUF_SIZE   256
#define VISION_TIMEOUT    rt_tick_from_millisecond(100)

static char g_line[VISION_BUF_SIZE];





static int int_field_after_comma(const char *s, int comma_index, int *out)
{
    const char *p = s;
    int i;

    for (i = 0; i < comma_index; i++)
    {
        p = strchr(p, ',');
        if (p == RT_NULL)
        {
            return 0;
        }
        p++;
    }
    
    if (*p == '\0')
    {
        return 0;
    }
    *out = atoi(p);
    return 1;
}

void thread_vision_entry(void *param)
{
    (void)param;

    while (1)
    {
        int ret = bsp_uart_read_line(UART_PORT_K230, g_line, VISION_BUF_SIZE, VISION_TIMEOUT);

        if (ret > 0)
        {
            if (strncmp(g_line, "FACE,", 5) == 0)
            {
                
                int offset_x;
                if (int_field_after_comma(g_line, 5, &offset_x))
                {
                    pantilt_on_face(offset_x);
                }
            }
        }
        else
        {
            
            rt_thread_mdelay(10);
        }
    }
}

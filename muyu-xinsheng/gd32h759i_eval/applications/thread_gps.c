







#include <rtthread.h>
#include <string.h>
#include "bsp_uart.h"
#include "bsp_gps.h"

#define GPS_LINE_MAX   160

void thread_gps_entry(void *param)
{
    char line[GPS_LINE_MAX];
    int  len = 0;
    char ch;

    (void)param;

    while (1)
    {
        int n = bsp_uart_read(UART_PORT_GPS, &ch, 1, rt_tick_from_millisecond(50));
        gps_info_t gps;

        if (n <= 0)
        {
            rt_thread_mdelay(5);
            continue;
        }

        if (ch == '\r')
        {
            continue;
        }
        if (ch == '\n')
        {
            line[len] = '\0';
            len = 0;

            if (line[0] == '$')
            {
                if (bsp_gps_parse_line(line, &gps) == 1)
                {
                    char json[160];
                    int  jl = bsp_gps_format_json(json, sizeof(json), &gps);

                    if (jl > 0)
                    {
                        
                        bsp_uart_send(UART_PORT_ESP, json, (rt_size_t)jl);
                        bsp_uart_send_str(UART_PORT_ESP, "\r\n");
                        rt_kprintf("GPS:%s\n", json);
                    }
                }
            }
        }
        else if (len < GPS_LINE_MAX - 1)
        {
            line[len++] = ch;
        }
        else
        {
            len = 0;   
        }
    }
}

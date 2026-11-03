






#include <rtthread.h>
#include "bsp_uart.h"
#include "bsp_ir.h"

#define IR_DEBOUNCE_MS     20
#define IR_POLL_MS         10

static rt_uint32_t g_debounce_start = 0;
static int         g_last_raw       = 1;
static int         g_stable_state   = 0;   

void thread_ir_entry(void *param)
{
    (void)param;

    g_debounce_start = rt_tick_get_millisecond();
    g_last_raw       = 1;
    g_stable_state   = 1;

    while (1)
    {
        rt_uint32_t now = rt_tick_get_millisecond();
        int raw = bsp_ir_read();

        if (raw != g_last_raw)
        {
            g_debounce_start = now;
            g_last_raw = raw;
        }

        if ((now - g_debounce_start) > IR_DEBOUNCE_MS)
        {
            if (raw != g_stable_state)
            {
                g_stable_state = raw;
                
                if (g_stable_state == 0)
                {
                    bsp_uart_send_str(UART_PORT_ESP, "medicine\r\n");
                    rt_kprintf("【红外】无遮挡，发送 medicine\n");
                }
            }
        }

        rt_thread_mdelay(IR_POLL_MS);
    }
}

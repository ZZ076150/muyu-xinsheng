










#include <rtthread.h>
#include "bsp_screen.h"

typedef enum
{
    SCREEN_ST_IDLE = 0,   
    SCREEN_ST_CMD,        
    SCREEN_ST_TAIL        
} screen_state_t;

static screen_state_t g_state = SCREEN_ST_IDLE;
static rt_uint8_t     g_cmd = 0;

void screen_init(void)
{
    g_state = SCREEN_ST_IDLE;
    g_cmd = 0;
}

int screen_feed(rt_uint8_t byte, rt_uint8_t *cmd)
{
    switch (g_state)
    {
    case SCREEN_ST_IDLE:
        if (byte == SCREEN_HEADER)
        {
            g_state = SCREEN_ST_CMD;
        }
        return 0;

    case SCREEN_ST_CMD:
        g_cmd = byte;
        g_state = SCREEN_ST_TAIL;
        return 0;

    case SCREEN_ST_TAIL:
        if (byte == SCREEN_TAIL)
        {
            if (cmd != RT_NULL)
            {
                *cmd = g_cmd;
            }
            g_state = SCREEN_ST_IDLE;
            return 1;
        }
        
        if (byte == SCREEN_HEADER)
        {
            g_state = SCREEN_ST_CMD;
        }
        else
        {
            g_state = SCREEN_ST_IDLE;
        }
        return -1;
    }

    g_state = SCREEN_ST_IDLE;
    return -1;
}

rt_uint8_t screen_normalize_cmd(rt_uint8_t raw)
{
    if (raw >= 0x01 && raw <= 0x09)
    {
        return (rt_uint8_t)('0' + raw);
    }
    return raw;
}

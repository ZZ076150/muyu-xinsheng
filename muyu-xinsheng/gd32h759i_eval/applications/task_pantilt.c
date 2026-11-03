









#include <rtthread.h>
#include <stdlib.h>
#include "task_pantilt.h"
#include "bsp_stepper.h"
#include "bsp_uart.h"


static volatile rt_bool_t g_enable_auto_search = RT_TRUE;

static int          g_current_offset_x   = 0;
static rt_bool_t    g_new_target_ready   = RT_FALSE;
static rt_uint32_t  g_last_target_refresh = 0;
static rt_uint32_t  g_last_face_detect    = 0;   
static rt_uint32_t  g_last_search_step    = 0;

static rt_bool_t    g_is_searching       = RT_FALSE;
static rt_bool_t    g_search_direction   = RT_FALSE;
static int          g_smooth_step_counter = 0;
static int          g_search_cycle_count  = 0;
#define SEARCH_TARGET_STEPS  SEARCH_STEPS_180


static void process_face_offset(int offset_x)
{
    int pulse_cnt;
    rt_bool_t dir;

    pantilt_stop_search();

    pulse_cnt = offset_x / 2;              
    dir = (pulse_cnt > 0) ? RT_FALSE : RT_TRUE;   

    if (abs(pulse_cnt) < DEAD_ZONE)
    {
        rt_kprintf("【人脸】在死区，不移动（偏移 %d）\n", offset_x);
        return;
    }

    rt_kprintf("【人脸偏移】%d → 脉冲数：%d\n", offset_x, pulse_cnt);
    stepper_run(abs(pulse_cnt), dir);
}


static void do_smooth_search(void)
{
    int steps_to_move;

    if (!g_is_searching || stepper_busy())
    {
        return;
    }

    steps_to_move = SMOOTH_STEPS_PER_CYCLE;
    if (g_smooth_step_counter + steps_to_move >= SEARCH_TARGET_STEPS)
    {
        steps_to_move = SEARCH_TARGET_STEPS - g_smooth_step_counter;
    }
    if (steps_to_move > 0)
    {
        stepper_run(steps_to_move, g_search_direction);
        g_smooth_step_counter += steps_to_move;
    }
    if (g_smooth_step_counter >= SEARCH_TARGET_STEPS && !stepper_busy())
    {
        g_smooth_step_counter = 0;
        g_search_cycle_count++;
        g_search_direction = !g_search_direction;
        rt_kprintf("【搜索】换向: %s\n", g_search_direction ? "右" : "左");
    }
}

static void start_smooth_search(void)
{
    if (g_is_searching)
    {
        return;
    }
    rt_kprintf("【启动搜索】左右扫描寻找人脸（10s 未检测人脸）\n");
    g_is_searching        = RT_TRUE;
    g_smooth_step_counter = 0;
    g_search_cycle_count  = 0;
    g_last_search_step    = rt_tick_get_millisecond();
}

void pantilt_stop_search(void)
{
    if (g_is_searching)
    {
        rt_kprintf("【停止搜索】检测到人脸 / 收到停止命令\n");
        g_is_searching = RT_FALSE;
    }
}

void pantilt_init(void)
{
    g_enable_auto_search  = RT_TRUE;
    g_current_offset_x    = 0;
    g_new_target_ready    = RT_FALSE;
    g_last_target_refresh = rt_tick_get_millisecond();
    g_last_face_detect    = rt_tick_get_millisecond();
    g_is_searching        = RT_FALSE;
}

void pantilt_on_face(int offset_x)
{
    g_current_offset_x = offset_x;
    g_new_target_ready = RT_TRUE;
    g_last_face_detect = rt_tick_get_millisecond();
}

void pantilt_set_auto_search(rt_bool_t enable)
{
    g_enable_auto_search = enable;
    if (!enable)
    {
        pantilt_stop_search();
    }
}

rt_bool_t pantilt_get_auto_search(void)
{
    return g_enable_auto_search;
}







void pantilt_thread_entry(void *param)
{
    (void)param;

    while (1)
    {
        rt_uint32_t now = rt_tick_get_millisecond();

        
        if (now - g_last_target_refresh >= TARGET_REFRESH_INTERVAL)
        {
            g_last_target_refresh = now;
            if (g_new_target_ready)
            {
                process_face_offset(g_current_offset_x);
                g_new_target_ready = RT_FALSE;
            }
            else
            {
                if (g_enable_auto_search &&
                    (now - g_last_face_detect >= LOST_TIME_THRESHOLD) &&
                    !g_is_searching && !stepper_busy())
                {
                    start_smooth_search();
                }
            }
        }

        
        if (g_is_searching && (now - g_last_search_step >= SEARCH_INTERVAL) && !stepper_busy())
        {
            do_smooth_search();
            g_last_search_step = now;
        }

        
        rt_thread_mdelay(20);
    }
}

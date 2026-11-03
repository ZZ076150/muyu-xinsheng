


















#include <rtthread.h>
#include "bsp_uart.h"
#include "bsp_stepper.h"
#include "bsp_ir.h"
#include "bsp_mpu6050.h"
#include "task_pantilt.h"


void pantilt_thread_entry(void *param);
void thread_vision_entry(void *param);
void thread_net_entry(void *param);
void thread_fall_entry(void *param);
void thread_screen_entry(void *param);
void thread_gps_entry(void *param);
void thread_ir_entry(void *param);


#define THREAD_MOTION_STACK   2048
#define THREAD_MOTION_PRIO    8
#define THREAD_VISION_STACK   1024
#define THREAD_VISION_PRIO    10
#define THREAD_NET_STACK      1024
#define THREAD_NET_PRIO       10
#define THREAD_FALL_STACK     1024
#define THREAD_FALL_PRIO      11
#define THREAD_SCREEN_STACK   1024
#define THREAD_SCREEN_PRIO    12
#define THREAD_GPS_STACK      1024
#define THREAD_GPS_PRIO       12
#define THREAD_IR_STACK       512
#define THREAD_IR_PRIO        13

static rt_thread_t tid_motion = RT_NULL;
static rt_thread_t tid_vision = RT_NULL;
static rt_thread_t tid_net    = RT_NULL;
static rt_thread_t tid_fall   = RT_NULL;
static rt_thread_t tid_screen = RT_NULL;
static rt_thread_t tid_gps    = RT_NULL;
static rt_thread_t tid_ir     = RT_NULL;

int main(void)
{
    rt_kprintf("\n========================================\n");
    rt_kprintf("  舒衡智养 · 适老云台追踪  GD32H759I-EVAL\n");
    rt_kprintf("  RT-Thread %s\n", RT_VERSION);
    rt_kprintf("========================================\n");

    
    if (bsp_uart_init() != 0)
    {
        rt_kprintf("[MAIN] 串口初始化失败，请检查 BSP 是否使能 uart1/2/3/4/6\n");
        return -1;
    }

    
    stepper_init();
    bsp_ir_init();
    pantilt_init();

    
    tid_motion = rt_thread_create("motion", pantilt_thread_entry, RT_NULL,
                                  THREAD_MOTION_STACK, THREAD_MOTION_PRIO, 10);
    tid_vision = rt_thread_create("vision", thread_vision_entry, RT_NULL,
                                  THREAD_VISION_STACK, THREAD_VISION_PRIO, 10);
    tid_net    = rt_thread_create("net",    thread_net_entry,    RT_NULL,
                                  THREAD_NET_STACK,    THREAD_NET_PRIO,    10);
    tid_fall   = rt_thread_create("fall",   thread_fall_entry,   RT_NULL,
                                  THREAD_FALL_STACK,   THREAD_FALL_PRIO,   10);
    tid_screen = rt_thread_create("screen", thread_screen_entry, RT_NULL,
                                  THREAD_SCREEN_STACK, THREAD_SCREEN_PRIO, 10);
    tid_gps    = rt_thread_create("gps",    thread_gps_entry,    RT_NULL,
                                  THREAD_GPS_STACK,    THREAD_GPS_PRIO,    10);
    tid_ir     = rt_thread_create("ir",     thread_ir_entry,     RT_NULL,
                                  THREAD_IR_STACK,     THREAD_IR_PRIO,     10);

    if (tid_motion) rt_thread_startup(tid_motion);
    if (tid_vision) rt_thread_startup(tid_vision);
    if (tid_net)    rt_thread_startup(tid_net);
    if (tid_fall)   rt_thread_startup(tid_fall);
    if (tid_screen) rt_thread_startup(tid_screen);
    if (tid_gps)    rt_thread_startup(tid_gps);
    if (tid_ir)     rt_thread_startup(tid_ir);

    rt_kprintf("[MAIN] 系统启动完成，云台等待 K230 人脸数据\n");
    return 0;
}

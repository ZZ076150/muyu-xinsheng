/**
 * @file    main.c
 * @brief   目语心声 · 下位机主控主程序（兆易创新 GD32H759I-EVAL）
 *
 * 功能：
 *   1. 解析 K230 上报的人脸坐标，计算与画面中心的位差，驱动步进电机云台跟踪；
 *   2. 人脸丢失超时后自动左右搜索；支持经 ESP8266 下发的 start / stop 开关；
 *   3. 红外对射检测药盒开合状态并上报；
 *   4. MPU6050 检测设备翻倒状态并上报；
 *   5. 提供与串口屏、GPS、DFPlayer、天问语音等模块的串口接口。
 *
 * 开发环境：Keil MDK 5 / IAR + GD32H7xx 固件库
 * 硬件平台：GD32H759I-EVAL（ARM Cortex-M7）
 */

#include "gd32h7xx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bsp/bsp_uart.h"
#include "bsp/bsp_stepmotor.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_i2c_mpu6050.h"

/* ==================== 跟踪与搜索参数 ==================== */
#define FRAME_CENTER_X          320         /* 画面中心 X（与 K230 分辨率对应：640/2） */
#define DEAD_ZONE               15          /* 死区（像素），偏差小于该值不动作 */
#define STEPS_PER_PIXEL         3           /* 像素 → 脉冲换算系数（按实测标定） */
#define FACE_LOST_TIMEOUT_MS    10000U      /* 人脸丢失超时，进入自动搜索 */
#define SEARCH_STEPS_180        3200        /* 180° 对应脉冲数（32 细分 6400/圈） */
#define SEARCH_INTERVAL_MS      20U         /* 搜索时每步间隔 */
#define LINE_BUF_LEN            96
#define IR_PERIOD_MS            20U
#define TILT_PERIOD_MS          200U

/* ==================== 运行状态 ==================== */
static volatile uint32_t s_last_face_ms   = 0;      /* 最近一次收到人脸帧的时间 */
static volatile int32_t  s_last_offset    = 0;      /* 最近一次位差 */
static volatile bool     s_auto_search    = true;   /* 自动搜索总开关（可由 MQTT 控制） */
static volatile bool     s_searching      = false;  /* 是否处于搜索状态 */
static int32_t           s_search_moved   = 0;      /* 当前扫描方向已走步数 */

static bool     s_ir_last_reported = false;
static bool     s_tilt_last_reported = false;

/* 毫秒计时（可由 SysTick 中断累加） */
static volatile uint32_t s_tick_ms = 0;
uint32_t get_tick_ms(void) { return s_tick_ms; }
void SysTick_Handler(void) { s_tick_ms++; }

static void systick_config_ms(void)
{
    /* 1ms 中断：SystemCoreClock / 1000 */
    SysTick_Config(SystemCoreClock / 1000U);
}

static void board_init(void)
{
    /* 中断分组与优先级配置（按固件库版本调整） */
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    systick_config_ms();
    bsp_uart_init_all();
    stepmotor_init();
    bsp_gpio_init();
    (void)mpu6050_init();
}

/* ==================== 协议解析 ==================== */

/* K230 → 主控：
 *   F,<cx>,<cy>,<fw>,<fh>   人脸框中心与尺寸
 *   FALL,1                  老人摔倒
 *   GAZE,L / GAZE,R         目语心声注视方向
 * 主控 → ESP8266：IR,0/1（药盒）、TILT,0/1（翻倒）、SEARCH,on/off（搜索状态）
 */
static void parse_k230_line(const char *line)
{
    if (line[0] == 'F' && line[1] == ',') {
        int cx = 0, cy = 0, fw = 0, fh = 0;
        if (sscanf(line + 2, "%d,%d,%d,%d", &cx, &cy, &fw, &fh) >= 1) {
            s_last_face_ms = get_tick_ms();
            s_searching = false;
            s_last_offset = cx - FRAME_CENTER_X;
        }
    } else if (strncmp(line, "FALL,", 5) == 0) {
        char msg[24];
        snprintf(msg, sizeof(msg), "FALL,%s\n", (line[5] == '1') ? "1" : "0");
        bsp_uart_send_str(UART_ESP8266, msg);       /* 交由 ESP8266 发布 MQTT */
    } else if (strncmp(line, "GAZE,", 5) == 0) {
        char msg[24];
        snprintf(msg, sizeof(msg), "GAZE,%c\n", line[5]);
        bsp_uart_send_str(UART_ESP8266, msg);       /* 目语心声选择结果上报 */
    }
}

static void parse_esp_line(const char *line)
{
    if (strcmp(line, "start") == 0) {
        s_auto_search = true;
    } else if (strcmp(line, "stop") == 0) {
        s_auto_search = false;
        s_searching = false;
        stepmotor_stop();
    }
}

/* ==================== 云台跟踪与搜索 ==================== */

static void gimbal_track_task(void)
{
    int32_t offset = s_last_offset;

    if (offset > DEAD_ZONE || offset < -DEAD_ZONE) {
        /* 偏差换算为脉冲数：偏移越大走越多，单次限幅防止甩头 */
        int32_t steps = (offset > 0 ? offset : -offset) * STEPS_PER_PIXEL;
        if (steps > 200) { steps = 200; }

        if (offset > 0) {
            stepmotor_set_dir(STEPMOTOR_DIR_CW);    /* 目标在画面右侧 */
            stepmotor_move_steps(steps);
        } else {
            stepmotor_set_dir(STEPMOTOR_DIR_CCW);
            stepmotor_move_steps(steps);
        }
        s_last_offset = 0;                          /* 本次偏差已消费，等待下一帧 */
    }
}

static void gimbal_search_task(void)
{
    if (!s_auto_search) { return; }
    if ((get_tick_ms() - s_last_face_ms) < FACE_LOST_TIMEOUT_MS) { return; }
    if (stepmotor_is_busy()) { return; }

    if (!s_searching) {
        s_searching = true;
        s_search_moved = 0;
        stepmotor_set_dir(STEPMOTOR_DIR_CW);
        bsp_uart_send_str(UART_ESP8266, "SEARCH,on\n");
    }

    /* 以 180° 为行程左右往复扫描 */
    stepmotor_move_steps(SEARCH_INTERVAL_MS);
    s_search_moved += SEARCH_INTERVAL_MS;

    if (s_search_moved >= SEARCH_STEPS_180) {
        s_search_moved = 0;
        stepmotor_set_dir((s_search_moved % (SEARCH_STEPS_180 * 2)) ? STEPMOTOR_DIR_CCW
                                                                    : STEPMOTOR_DIR_CCW);
    }
}

/* ==================== 传感器任务 ==================== */

static void ir_task(void)
{
    bool changed_to = false;
    if (bsp_gpio_ir_poll(&changed_to)) {
        char msg[16];
        s_ir_last_reported = changed_to;
        snprintf(msg, sizeof(msg), "IR,%d\n", changed_to ? 1 : 0);
        bsp_uart_send_str(UART_ESP8266, msg);       /* 药盒开合状态上报 */
    }
}

static void tilt_task(void)
{
    bool tilted = mpu6050_is_overturned();
    if (tilted != s_tilt_last_reported) {
        char msg[16];
        s_tilt_last_reported = tilted;
        snprintf(msg, sizeof(msg), "TILT,%d\n", tilted ? 1 : 0);
        bsp_uart_send_str(UART_ESP8266, msg);       /* 设备翻倒状态上报 */
    }
}

/* ==================== 主函数 ==================== */
int main(void)
{
    char line[LINE_BUF_LEN];
    uint32_t last_ir_ms = 0;
    uint32_t last_tilt_ms = 0;

    board_init();
    stepmotor_enable(true);

    while (1) {
        /* 1. 处理 K230 帧 */
        while (bsp_uart_read_line(UART_K230, line, sizeof(line)) > 0) {
            parse_k230_line(line);
        }

        /* 2. 处理 ESP8266 指令 */
        while (bsp_uart_read_line(UART_ESP8266, line, sizeof(line)) > 0) {
            parse_esp_line(line);
        }

        /* 3. 云台跟踪 / 丢失搜索 */
        gimbal_track_task();
        gimbal_search_task();

        /* 4. 周期任务 */
        uint32_t now = get_tick_ms();

        if ((now - last_ir_ms) >= IR_PERIOD_MS) {
            last_ir_ms = now;
            ir_task();
        }

        if ((now - last_tilt_ms) >= TILT_PERIOD_MS) {
            last_tilt_ms = now;
            tilt_task();
        }

        /* 5. 空闲时喂看门狗（若启用） */
        /* fwdgt_counter_reload(); */
    }
}

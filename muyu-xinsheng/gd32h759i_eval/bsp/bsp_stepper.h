















#ifndef __BSP_STEPPER_H__
#define __BSP_STEPPER_H__

#include <rtthread.h>


#define STEPS_PER_REV          6400
#define STEP_PER_DEG           (STEPS_PER_REV / 360.0f)


#define MIN_PULSE_INTERVAL     12U
#define MAX_PULSE_INTERVAL     60U
#define ACCEL_STEP             2U

int      stepper_init(void);






void     stepper_run(int32_t pulse, rt_bool_t dir);


rt_bool_t stepper_busy(void);


void     stepper_stop(void);


int32_t  stepper_remain(void);

#endif 

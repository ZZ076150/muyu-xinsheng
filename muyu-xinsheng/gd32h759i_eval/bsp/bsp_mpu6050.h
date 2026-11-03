










#ifndef __BSP_MPU6050_H__
#define __BSP_MPU6050_H__

#include <rtthread.h>





int  bsp_mpu6050_init(void);




int  bsp_mpu6050_read_accel(float *ax, float *ay, float *az);

#endif 

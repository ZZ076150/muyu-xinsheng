
















#ifndef __BSP_CONFIG_H__
#define __BSP_CONFIG_H__











#define K230_UART_DEV        "uart1"
#define K230_UART_BAUDRATE   115200

#define ESP_UART_DEV         "uart2"
#define ESP_UART_BAUDRATE    115200

#define SCREEN_UART_DEV      "uart3"
#define SCREEN_UART_BAUDRATE 9600

#define GPS_UART_DEV         "uart4"
#define GPS_UART_BAUDRATE    9600

#define MP3_UART_DEV         "uart6"
#define MP3_UART_BAUDRATE    9600









#define STEP_GPIO_PORT      GPIOB
#define STEP_GPIO_PIN       GPIO_PIN_0
#define DIR_GPIO_PORT       GPIOB
#define DIR_GPIO_PIN        GPIO_PIN_1
#define ENA_GPIO_PORT       GPIOC
#define ENA_GPIO_PIN        GPIO_PIN_3


#define MOTOR_ENA_ON        SET
#define MOTOR_ENA_OFF       RESET





#define IR_GPIO_PORT        GPIOC
#define IR_GPIO_PIN         GPIO_PIN_2











#define MPU6050_I2C_DEV      "hwi2c0"
#define MPU6050_ADDR         0x68    









#define STEPPER_TIMER            TIMER5
#define STEPPER_TIMER_IRQn       TIMER5_IRQn
#define STEPPER_TIMER_IRQHANDLER TIMER5_IRQHandler
#define STEPPER_TIMER_CLK_HZ     300000000U  

#endif 

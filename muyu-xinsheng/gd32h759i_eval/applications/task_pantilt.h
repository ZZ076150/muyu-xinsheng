








#ifndef __TASK_PANTILT_H__
#define __TASK_PANTILT_H__

#include <rtthread.h>


#define DEAD_ZONE                 15      
#define TARGET_REFRESH_INTERVAL 2000U     
#define LOST_TIME_THRESHOLD     10000U    


#define SEARCH_STEPS_180          3200    
#define SMOOTH_STEPS_PER_CYCLE    5       
#define SEARCH_INTERVAL          20U      




void pantilt_init(void);





void pantilt_on_face(int offset_x);




void pantilt_set_auto_search(rt_bool_t enable);
rt_bool_t pantilt_get_auto_search(void);




void pantilt_stop_search(void);




void pantilt_thread_entry(void *param);

#endif 

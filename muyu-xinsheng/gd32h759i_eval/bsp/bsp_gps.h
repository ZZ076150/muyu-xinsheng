







#ifndef __BSP_GPS_H__
#define __BSP_GPS_H__

#include <rtthread.h>

typedef struct
{
    int    valid;         
    int    hour;          
    int    minute;
    int    second;
    double latitude;      
    double longitude;     
} gps_info_t;




int bsp_gps_parse_line(const char *nmea, gps_info_t *out);





int bsp_gps_format_json(char *buf, rt_size_t buflen, const gps_info_t *gps);

#endif 









#include <rtthread.h>
#include <string.h>
#include <stdint.h>
#include "bsp_gps.h"


static const char *field_at(const char *s, int n)
{
    int i;
    const char *p = s;

    for (i = 0; i < n; i++)
    {
        p = strchr(p, ',');
        if (p == RT_NULL)
        {
            return RT_NULL;
        }
        p++;
    }
    return p;
}


static double parse_deg(const char *field, int deg_digits)
{
    double deg = 0.0;
    double minutes = 0.0;
    double scale = 0.1;
    int i;

    if (field == RT_NULL)
    {
        return -9999.0;
    }
    for (i = 0; i < deg_digits; i++)
    {
        if (field[i] < '0' || field[i] > '9')
        {
            return -9999.0;
        }
        deg = deg * 10.0 + (field[i] - '0');
    }
    if (field[deg_digits] != '.')
    {
        return -9999.0;
    }
    
    for (i = deg_digits + 1; field[i] >= '0' && field[i] <= '9'; i++)
    {
        minutes = minutes * 10.0 + (field[i] - '0');
    }
    if (field[i] != '.')
    {
        return -9999.0;
    }
    
    for (i = i + 1; field[i] >= '0' && field[i] <= '9'; i++)
    {
        minutes += (field[i] - '0') * scale;
        scale *= 0.1;
    }
    return deg + minutes / 60.0;
}

int bsp_gps_parse_line(const char *nmea, gps_info_t *out)
{
    const char *time_f, *lat_f, *lat_dir, *lon_f, *lon_dir;
    int hh, mm, ss, i;

    if (nmea == RT_NULL || out == RT_NULL)
    {
        return 0;
    }
    if (strncmp(nmea, "$GNGGA", 6) != 0)
    {
        return 0;
    }

    time_f  = field_at(nmea, 1);
    lat_f   = field_at(nmea, 2);
    lat_dir = field_at(nmea, 3);
    lon_f   = field_at(nmea, 4);
    lon_dir = field_at(nmea, 5);

    
    if (time_f == RT_NULL || strlen(time_f) < 6)
    {
        return 0;
    }
    for (i = 0; i < 6; i++)
    {
        if (time_f[i] < '0' || time_f[i] > '9')
        {
            return 0;
        }
    }
    hh = (time_f[0] - '0') * 10 + (time_f[1] - '0');
    mm = (time_f[2] - '0') * 10 + (time_f[3] - '0');
    ss = (time_f[4] - '0') * 10 + (time_f[5] - '0');

    
    out->latitude  = parse_deg(lat_f, 2);
    out->longitude = parse_deg(lon_f, 3);
    if (out->latitude == -9999.0 || out->longitude == -9999.0)
    {
        return 0;
    }
    if (lat_dir != RT_NULL && lat_dir[0] == 'S')
    {
        out->latitude = -out->latitude;
    }
    if (lon_dir != RT_NULL && lon_dir[0] == 'W')
    {
        out->longitude = -out->longitude;
    }

    
    hh += 8;
    if (hh >= 24)
    {
        hh -= 24;
    }
    out->hour   = hh;
    out->minute = mm;
    out->second = ss;
    out->valid  = 1;
    return 1;
}


static void fmt_fixed6(char *dst, rt_size_t len, double v)
{
    int64_t scaled, ip, fp;

    if (v >= 0)
    {
        scaled = (int64_t)(v * 1000000.0 + 0.5);
    }
    else
    {
        scaled = (int64_t)(v * 1000000.0 - 0.5);
    }
    ip = scaled / 1000000;
    fp = scaled % 1000000;
    if (fp < 0)
    {
        fp = -fp;
    }
    rt_snprintf(dst, len, "%d.%06d", (int)ip, (int)fp);
}

int bsp_gps_format_json(char *buf, rt_size_t buflen, const gps_info_t *gps)
{
    char lat_s[24], lon_s[24];

    fmt_fixed6(lat_s, sizeof(lat_s), gps->latitude);
    fmt_fixed6(lon_s, sizeof(lon_s), gps->longitude);

    return rt_snprintf(buf, buflen,
                       "{\"type\":\"gps\",\"time\":\"%02d:%02d:%02d\","
                       "\"lat\":%s,\"lon\":%s}",
                       gps->hour, gps->minute, gps->second, lat_s, lon_s);
}

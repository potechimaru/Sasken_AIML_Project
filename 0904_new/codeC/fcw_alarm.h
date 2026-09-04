#ifndef FCW_ALARM_H_
#define FCW_ALARM_H_

#include "fcw_types.h"


typedef void (*FcwAlarmOutputFunc)(
    vx_bool active,
    void *user_data
);


typedef struct
{
    vx_bool initialized;
    vx_bool active;

    FcwAlarmOutputFunc output_func;
    void *user_data;

} FcwAlarmContext;


vx_status fcw_alarm_init(
    FcwAlarmContext *context,
    FcwAlarmOutputFunc output_func,
    void *user_data
);


vx_status fcw_alarm_update(
    FcwAlarmContext *context,
    const FcwCar *cars,
    vx_uint32 num_cars
);


vx_status fcw_alarm_set_active(
    FcwAlarmContext *context,
    vx_bool active
);


void fcw_alarm_deinit(
    FcwAlarmContext *context
);

#endif
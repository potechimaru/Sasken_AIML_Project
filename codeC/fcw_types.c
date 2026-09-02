#include "fcw_types.h"

#include <string.h>

vx_status fcw_frame_result_reset(
    FcwFrameResult *result,
    vx_int32 frame_index,
    vx_float32 time_s
)
{
    if (result == NULL)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memset(result, 0, sizeof(*result));
    result->frame_index = frame_index;
    result->time_s = time_s;
    result->alarm_active = vx_false_e;

    return VX_SUCCESS;
}

vx_status fcw_frame_result_add_car(
    FcwFrameResult *result,
    FcwCar **car
)
{
    FcwCar *new_car;

    if ((result == NULL) || (car == NULL))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    *car = NULL;

    if (result->num_cars >= FCW_MAX_DETECTIONS)
    {
        result->dropped_cars++;
        return VX_ERROR_NO_RESOURCES;
    }

    new_car = &result->cars[result->num_cars];
    memset(new_car, 0, sizeof(*new_car));
    new_car->track_id = -1;
    new_car->roi_valid = vx_false_e;
    new_car->track_valid = vx_false_e;
    new_car->dh_dt_valid = vx_false_e;
    new_car->ttc_valid = vx_false_e;
    new_car->r_squared_valid = vx_false_e;
    new_car->alert = vx_false_e;

    result->num_cars++;
    *car = new_car;

    return VX_SUCCESS;
}

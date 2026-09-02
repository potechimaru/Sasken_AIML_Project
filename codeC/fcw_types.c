#include "fcw_types.h"

#include <string.h>

vx_status fcw_frame_result_reset(FcwFrameResult *result, vx_int32 frame_index)
{
    if (result == NULL)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memset(result, 0, sizeof(*result));
    result->frame_index = frame_index;
    return VX_SUCCESS;
}

vx_status fcw_frame_result_add_car(FcwFrameResult *result, FcwCar **car)
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
    result->num_cars++;
    *car = new_car;
    return VX_SUCCESS;
}

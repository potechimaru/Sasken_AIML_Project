#include "avp_fcw_roi.h"
#include <stddef.h>
#include <stdio.h>

vx_bool fcw_roi_check_car(FcwCar *car)
{
    vx_float32 x;
    vx_float32 y;
    vx_float32 ratio;
    vx_float32 left;
    vx_float32 right;
    
    // printf("===========================ROI==============================");

    if (car == NULL)
    {
        return vx_false_e;
    }

    car->roi_valid = vx_false_e;

    /* BBoxの下辺中央 */
    x = (car->box.xmin + car->box.xmax) * 0.5F;
    y = car->box.ymax;

    if ((y < FCW_ROI_TOP_Y) || (y > FCW_ROI_BOTTOM_Y))
    {
        return vx_false_e;
    }

    ratio = (y - FCW_ROI_TOP_Y) /
            (FCW_ROI_BOTTOM_Y - FCW_ROI_TOP_Y);

    left = FCW_ROI_TOP_LEFT_X +
           ratio * (FCW_ROI_BOTTOM_LEFT_X - FCW_ROI_TOP_LEFT_X);

    right = FCW_ROI_TOP_RIGHT_X +
            ratio * (FCW_ROI_BOTTOM_RIGHT_X - FCW_ROI_TOP_RIGHT_X);

    if ((x >= left) && (x <= right))
    {
        car->roi_valid = vx_true_e;
    }

    return car->roi_valid;
}

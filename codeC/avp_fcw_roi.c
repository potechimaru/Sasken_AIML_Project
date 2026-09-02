#include "avp_fcw_roi.h"

typedef struct
{
    vx_float32 top_left_x;
    vx_float32 top_y;
    vx_float32 top_right_x;
    vx_float32 bottom_left_x;
    vx_float32 bottom_y;
    vx_float32 bottom_right_x;
} FcwRoi;

/* 正規化座標。前方レーンを表す固定台形ROI。 */
static const FcwRoi g_fcw_roi =
{
    0.40F, 0.50F,
    0.60F,
    0.20F, 1.00F,
    0.80F
};

vx_bool fcw_roi_check_car(FcwCar *car)
{
    vx_float32 center_x;
    vx_float32 center_y;
    vx_float32 ratio;
    vx_float32 left_x;
    vx_float32 right_x;

    if (car == NULL)
    {
        return vx_false_e;
    }

    car->roi_valid = vx_false_e;
    center_x = (car->xmin + car->xmax) * 0.5F;
    center_y = car->ymax;
    if ((center_y < g_fcw_roi.top_y) || (center_y > g_fcw_roi.bottom_y))
    {
        return vx_false_e;
    }

    ratio = (center_y - g_fcw_roi.top_y) /
            (g_fcw_roi.bottom_y - g_fcw_roi.top_y);
    left_x = g_fcw_roi.top_left_x +
             ratio * (g_fcw_roi.bottom_left_x - g_fcw_roi.top_left_x);
    right_x = g_fcw_roi.top_right_x +
              ratio * (g_fcw_roi.bottom_right_x - g_fcw_roi.top_right_x);
    if ((center_x >= left_x) && (center_x <= right_x))
    {
        car->roi_valid = vx_true_e;
    }
    return car->roi_valid;
}

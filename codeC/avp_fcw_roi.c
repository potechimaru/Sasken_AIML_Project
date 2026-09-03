#include "avp_fcw_roi.h"

/* Fixed normalized trapezoid: (0.40,0.50)-(0.60,0.50)-(0.80,1.0)-(0.20,1.0). */
vx_bool fcw_roi_check_car(FcwCar *car)
{
    vx_float32 x, y, ratio, left, right;
    if (car == NULL) return vx_false_e;
    car->roi_valid = vx_false_e;
    x = (car->box.xmin + car->box.xmax) * 0.5F;
    y = car->box.ymax;
    if ((y < 0.50F) || (y > 1.00F)) return vx_false_e;
    ratio = (y - 0.50F) / 0.50F;
    left = 0.40F + ratio * (0.20F - 0.40F);
    right = 0.60F + ratio * (0.80F - 0.60F);
    if ((x >= left) && (x <= right)) car->roi_valid = vx_true_e;
    return car->roi_valid;
}

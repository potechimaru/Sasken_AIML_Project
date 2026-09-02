#include "main_pre.h"

#include <string.h>

void fcw_main_pre_config_set_defaults(FcwMainPreConfig *config)
{
    if (config == NULL)
    {
        return;
    }

    /* モデルのclass IDと必要な閾値は、実行時の確認後に呼び出し側で上書きする。 */
    config->score_threshold = 0.0F;
    config->car_class_id = 1;
}

void fcw_main_pre_init(FcwMainPreContext *pre, const FcwMainPreConfig *config)
{
    FcwMainPreConfig defaults;

    if (pre == NULL)
    {
        return;
    }

    memset(pre, 0, sizeof(*pre));
    fcw_main_pre_config_set_defaults(&defaults);
    pre->config = (config == NULL) ? defaults : *config;
    (void)fcw_frame_result_reset(&pre->frame_result, 0);
}

void fcw_main_pre_begin_frame(FcwMainPreContext *pre, vx_int32 frame_index)
{
    if (pre == NULL)
    {
        return;
    }

    (void)fcw_frame_result_reset(&pre->frame_result, frame_index);
    pre->frame_started = vx_true_e;
}

bool fcw_main_pre_add_tidl_object(
    FcwMainPreContext *pre,
    vx_int32 channel,
    const TIDL_ODLayerObjInfo *tidl_object
)
{
    FcwCar *car;

    if ((pre == NULL) || (tidl_object == NULL) ||
        (pre->frame_started != vx_true_e))
    {
        return false;
    }

    if (((vx_float32)tidl_object->score < pre->config.score_threshold) ||
        ((vx_int32)tidl_object->label != pre->config.car_class_id))
    {
        return false;
    }

    if (fcw_frame_result_add_car(&pre->frame_result, &car) != VX_SUCCESS)
    {
        return false;
    }

    car->xmin = (vx_float32)tidl_object->xmin;
    car->ymin = (vx_float32)tidl_object->ymin;
    car->xmax = (vx_float32)tidl_object->xmax;
    car->ymax = (vx_float32)tidl_object->ymax;
    car->score = (vx_float32)tidl_object->score;
    car->class_id = (vx_int32)tidl_object->label;
    car->channel = channel;

    return true;
}

const FcwFrameResult *fcw_main_pre_get_frame_result(const FcwMainPreContext *pre)
{
    return (pre == NULL) ? NULL : &pre->frame_result;
}

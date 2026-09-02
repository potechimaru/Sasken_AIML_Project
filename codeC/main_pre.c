#include "main_pre.h"

#include <string.h>

static vx_float32 fcw_main_pre_clamp(vx_float32 value, vx_float32 min, vx_float32 max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static vx_float32 fcw_main_pre_normalize(vx_float32 value, vx_int32 size,
                                          vx_bool already_normalized)
{
    if (already_normalized == vx_true_e) return value;
    if (size <= 0) return 0.0F;
    return value / (vx_float32)size;
}

void fcw_main_pre_config_set_defaults(FcwMainPreConfig *config)
{
    if (config == NULL) return;

    /* 以下は実モデル・入力動画を確認後にmain側で上書きする暫定値。 */
    config->score_threshold = 0.0F;
    config->car_class_id = 1;
    config->fps = 30;
    config->ttc_channel = 0;
    config->image_width = 1280;
    config->image_height = 720;
    config->tidl_bbox_is_normalized = vx_false_e;
}

void fcw_main_pre_init(FcwMainPreContext *pre, const FcwMainPreConfig *config)
{
    FcwMainPreConfig defaults;

    if (pre == NULL) return;
    memset(pre, 0, sizeof(*pre));
    fcw_main_pre_config_set_defaults(&defaults);
    pre->config = (config == NULL) ? defaults : *config;
    (void)fcw_frame_result_reset(&pre->frame_result, 0);
}

void fcw_main_pre_reset(FcwMainPreContext *pre)
{
    if (pre == NULL) return;
    memset(&pre->ttc_manager, 0, sizeof(pre->ttc_manager));
    (void)fcw_frame_result_reset(&pre->frame_result, 0);
    pre->frame_started = vx_false_e;
}

void fcw_main_pre_begin_frame(FcwMainPreContext *pre, vx_int32 frame_index)
{
    if (pre == NULL) return;
    (void)fcw_frame_result_reset(&pre->frame_result, frame_index);
    pre->frame_started = vx_true_e;
}

vx_bool fcw_main_pre_add_tidl_object(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id,
    const TIDL_ODLayerObjInfo *tidl_object
)
{
    FcwCar *car;
    fcw_ttc_data_t ttc_data;
    vx_float32 xmin, ymin, xmax, ymax, swap;

    if ((pre == NULL) || (tidl_object == NULL) ||
        (pre->frame_started != vx_true_e)) return vx_false_e;
    if (((vx_float32)tidl_object->score < pre->config.score_threshold) ||
        ((vx_int32)tidl_object->label != pre->config.car_class_id)) return vx_false_e;
    if (fcw_frame_result_add_car(&pre->frame_result, &car) != VX_SUCCESS)
    {
        return vx_false_e;
    }

    xmin = fcw_main_pre_normalize((vx_float32)tidl_object->xmin,
        pre->config.image_width, pre->config.tidl_bbox_is_normalized);
    ymin = fcw_main_pre_normalize((vx_float32)tidl_object->ymin,
        pre->config.image_height, pre->config.tidl_bbox_is_normalized);
    xmax = fcw_main_pre_normalize((vx_float32)tidl_object->xmax,
        pre->config.image_width, pre->config.tidl_bbox_is_normalized);
    ymax = fcw_main_pre_normalize((vx_float32)tidl_object->ymax,
        pre->config.image_height, pre->config.tidl_bbox_is_normalized);
    if (xmin > xmax) { swap = xmin; xmin = xmax; xmax = swap; }
    if (ymin > ymax) { swap = ymin; ymin = ymax; ymax = swap; }

    car->xmin = fcw_main_pre_clamp(xmin, 0.0F, 1.0F);
    car->ymin = fcw_main_pre_clamp(ymin, 0.0F, 1.0F);
    car->xmax = fcw_main_pre_clamp(xmax, 0.0F, 1.0F);
    car->ymax = fcw_main_pre_clamp(ymax, 0.0F, 1.0F);
    car->score = (vx_float32)tidl_object->score;
    car->class_id = (vx_int32)tidl_object->label;
    car->channel = channel;
    car->track_id = track_id;

    (void)fcw_roi_check_car(car);
    if ((car->roi_valid != vx_true_e) || (track_id < 0) ||
        (channel != pre->config.ttc_channel))
    {
        return vx_true_e;
    }

    memset(&ttc_data, 0, sizeof(ttc_data));
    ttc_data.frame_id = pre->frame_result.frame_index;
    ttc_data.track_id = track_id;
    fcw_ttc_calculate_height(car->ymin, car->ymax, &ttc_data);
    if (fcw_ttc_update(&ttc_data, &pre->ttc_manager) != vx_true_e)
    {
        return vx_true_e;
    }
    car->history_length = fcw_ttc_get_history_length(&pre->ttc_manager, track_id);
    (void)fcw_ttc_calculate(pre->config.fps, &pre->ttc_manager, track_id, car);
    return vx_true_e;
}

const FcwFrameResult *fcw_main_pre_get_frame_result(const FcwMainPreContext *pre)
{
    return (pre == NULL) ? NULL : &pre->frame_result;
}

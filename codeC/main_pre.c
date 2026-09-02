#include "main_pre.h"

#include <string.h>

static vx_float32 fcw_main_pre_clamp(vx_float32 value, vx_float32 min, vx_float32 max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static vx_float32 fcw_main_pre_normalize(
    vx_float32 coordinate, vx_int32 size, vx_bool is_normalized)
{
    if (is_normalized == vx_true_e) return coordinate;
    if (size <= 0) return 0.0F;
    return coordinate / (vx_float32)size;
}

static vx_int32 fcw_main_pre_to_pixel(vx_float32 coordinate, vx_int32 size)
{
    vx_float32 pixel;

    if (size <= 0) return 0;
    pixel = coordinate * (vx_float32)(size - 1);
    return (vx_int32)(fcw_main_pre_clamp(pixel + 0.5F, 0.0F,
                                         (vx_float32)(size - 1)));
}

static fcw_ttc_track_t *fcw_main_pre_find_ttc_track(
    fcw_ttc_manager_t *manager, vx_int32 track_id)
{
    vx_uint32 i;

    for (i = 0U; i < FCW_TTC_MAX_TRACKS; i++)
    {
        if ((manager->tracks[i].active) &&
            (manager->tracks[i].track_id == track_id))
        {
            return &manager->tracks[i];
        }
    }
    return NULL;
}

void fcw_main_pre_config_set_defaults(FcwMainPreConfig *config)
{
    if (config == NULL) return;

    config->image_width = 1280;
    config->image_height = 720;
    config->fps = 30;
    config->score_threshold = 0.0F;
    config->car_class_id = 1;
    config->tidl_bbox_is_normalized = vx_false_e;
}

void fcw_main_pre_init(FcwMainPreContext *pre, const FcwMainPreConfig *config)
{
    FcwMainPreConfig defaults;

    if (pre == NULL) return;

    memset(pre, 0, sizeof(*pre));
    fcw_main_pre_config_set_defaults(&defaults);
    pre->config = (config == NULL) ? defaults : *config;
    (void)fcw_frame_result_reset(&pre->frame_result, 0, 0.0F);
}

void fcw_main_pre_reset(FcwMainPreContext *pre)
{
    if (pre == NULL) return;

    memset(&pre->ttc_manager, 0, sizeof(pre->ttc_manager));
    (void)fcw_frame_result_reset(&pre->frame_result, 0, 0.0F);
    pre->frame_started = vx_false_e;
}

void fcw_main_pre_begin_frame(FcwMainPreContext *pre, vx_int32 frame_index)
{
    vx_float32 time_s = 0.0F;

    if (pre == NULL) return;
    if (pre->config.fps > 0)
    {
        time_s = (vx_float32)frame_index / (vx_float32)pre->config.fps;
    }
    (void)fcw_frame_result_reset(&pre->frame_result, frame_index, time_s);
    pre->frame_started = vx_true_e;
}

bool fcw_main_pre_add_tidl_object(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id,
    const TIDL_ODLayerObjInfo *tidl_object
)
{
    FcwCar *car;
    fcw_ttc_data_t ttc_data;
    fcw_ttc_track_t *ttc_track;
    vx_float32 xmin, ymin, xmax, ymax, swap, ttc_sec;

    if ((pre == NULL) || (tidl_object == NULL) ||
        (pre->frame_started != vx_true_e)) return false;
    if (((vx_float32)tidl_object->score < pre->config.score_threshold) ||
        ((vx_int32)tidl_object->label != pre->config.car_class_id)) return false;
    if (fcw_frame_result_add_car(&pre->frame_result, &car) != VX_SUCCESS) return false;

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

    car->box.xmin = fcw_main_pre_clamp(xmin, 0.0F, 1.0F);
    car->box.ymin = fcw_main_pre_clamp(ymin, 0.0F, 1.0F);
    car->box.xmax = fcw_main_pre_clamp(xmax, 0.0F, 1.0F);
    car->box.ymax = fcw_main_pre_clamp(ymax, 0.0F, 1.0F);
    car->score = (vx_float32)tidl_object->score;
    car->class_id = (vx_int32)tidl_object->label;
    car->channel = channel;
    car->track_id = track_id;
    car->track_valid = (track_id >= 0) ? vx_true_e : vx_false_e;

    car->pixel_box.x1 = fcw_main_pre_to_pixel(car->box.xmin, pre->config.image_width);
    car->pixel_box.y1 = fcw_main_pre_to_pixel(car->box.ymin, pre->config.image_height);
    car->pixel_box.x2 = fcw_main_pre_to_pixel(car->box.xmax, pre->config.image_width);
    car->pixel_box.y2 = fcw_main_pre_to_pixel(car->box.ymax, pre->config.image_height);
    car->bottom_center.x = (car->pixel_box.x1 + car->pixel_box.x2) / 2;
    car->bottom_center.y = car->pixel_box.y2;
    car->height_px = (vx_float32)(car->pixel_box.y2 - car->pixel_box.y1);

    if ((track_id < 0) || (car->height_px <= 0.0F)) return true;

    memset(&ttc_data, 0, sizeof(ttc_data));
    ttc_data.frame_id = pre->frame_result.frame_index;
    ttc_data.track_id = track_id;
    calculate_height((vx_float32)car->pixel_box.y1,
                     (vx_float32)car->pixel_box.y2, &ttc_data);
    if (!ttc_update(&ttc_data, &pre->ttc_manager)) return true;

    ttc_track = fcw_main_pre_find_ttc_track(&pre->ttc_manager, track_id);
    if (ttc_track != NULL)
    {
        car->history_length = (vx_uint32)ttc_track->history.history_count;
    }
    ttc_sec = 0.0F;
    if (calculate_ttc(pre->config.fps, &pre->ttc_manager, track_id, &ttc_sec))
    {
        car->ttc_sec = ttc_sec;
        car->ttc_valid = vx_true_e;
    }
    return true;
}

const FcwFrameResult *fcw_main_pre_get_frame_result(const FcwMainPreContext *pre)
{
    return (pre == NULL) ? NULL : &pre->frame_result;
}

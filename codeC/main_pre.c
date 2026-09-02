/*
 * 目的:
 * TIDL_ODLayerObjInfoをFCWの共有結果(FcwCar/FcwFrameResult)へコピーし、
 * TTCに必要なfcw_ttc_data_tを作ってtrack_id別の履歴へ追加する。
 *
 * このモジュールはmain.cの中へTIDL→TTC変換処理を直接書かずに済むように
 * するためのアダプタである。TIDLの検出ポインタはtensorのmap中だけ有効
 * なので、呼び出し元はmap中に本モジュールを呼び出すこと。
 */

#include "main_pre.h"

#include <stddef.h>
#include <string.h>

static vx_float32 fcw_main_pre_clamp_float(
    vx_float32 value,
    vx_float32 minimum,
    vx_float32 maximum
)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

static vx_int32 fcw_main_pre_clamp_int(
    vx_int32 value,
    vx_int32 minimum,
    vx_int32 maximum
)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

static vx_float32 fcw_main_pre_normalize_coordinate(
    vx_float32 value,
    vx_int32 source_size,
    bool is_normalized
)
{
    if (is_normalized)
    {
        return value;
    }

    if (source_size <= 0)
    {
        return 0.0F;
    }

    return value / (vx_float32)source_size;
}

static vx_int32 fcw_main_pre_to_pixel(
    vx_float32 normalized_coordinate,
    vx_int32 image_size
)
{
    vx_float32 coordinate;

    if (image_size <= 0)
    {
        return 0;
    }

    coordinate = normalized_coordinate * (vx_float32)(image_size - 1);

    /* 四捨五入してから画像範囲に収める */
    return fcw_main_pre_clamp_int(
        (vx_int32)(coordinate + 0.5F),
        0,
        image_size - 1
    );
}

static bool fcw_main_pre_fill_car(
    FcwCar *car,
    const FcwConfig *config,
    vx_int32 channel,
    vx_int32 track_id,
    const TIDL_ODLayerObjInfo *tidl_object
)
{
    vx_float32 xmin;
    vx_float32 ymin;
    vx_float32 xmax;
    vx_float32 ymax;
    vx_float32 swap_value;

    if ((car == NULL) || (config == NULL) || (tidl_object == NULL))
    {
        return false;
    }

    xmin = fcw_main_pre_normalize_coordinate(
        (vx_float32)tidl_object->xmin,
        config->tidl_box_width,
        config->tidl_bbox_is_normalized
    );
    ymin = fcw_main_pre_normalize_coordinate(
        (vx_float32)tidl_object->ymin,
        config->tidl_box_height,
        config->tidl_bbox_is_normalized
    );
    xmax = fcw_main_pre_normalize_coordinate(
        (vx_float32)tidl_object->xmax,
        config->tidl_box_width,
        config->tidl_bbox_is_normalized
    );
    ymax = fcw_main_pre_normalize_coordinate(
        (vx_float32)tidl_object->ymax,
        config->tidl_box_height,
        config->tidl_bbox_is_normalized
    );

    if (xmin > xmax)
    {
        swap_value = xmin;
        xmin = xmax;
        xmax = swap_value;
    }

    if (ymin > ymax)
    {
        swap_value = ymin;
        ymin = ymax;
        ymax = swap_value;
    }

    xmin = fcw_main_pre_clamp_float(xmin, 0.0F, 1.0F);
    ymin = fcw_main_pre_clamp_float(ymin, 0.0F, 1.0F);
    xmax = fcw_main_pre_clamp_float(xmax, 0.0F, 1.0F);
    ymax = fcw_main_pre_clamp_float(ymax, 0.0F, 1.0F);

    car->valid = true;
    car->channel = channel;
    car->track_id = track_id;
    car->label = (vx_int32)tidl_object->label;
    car->score = (vx_float32)tidl_object->score;

    car->normalized_box.xmin = xmin;
    car->normalized_box.ymin = ymin;
    car->normalized_box.xmax = xmax;
    car->normalized_box.ymax = ymax;

    car->pixel_box.xmin = fcw_main_pre_to_pixel(xmin, config->image_width);
    car->pixel_box.ymin = fcw_main_pre_to_pixel(ymin, config->image_height);
    car->pixel_box.xmax = fcw_main_pre_to_pixel(xmax, config->image_width);
    car->pixel_box.ymax = fcw_main_pre_to_pixel(ymax, config->image_height);

    car->bottom_center.x =
        ((vx_float32)car->pixel_box.xmin +
         (vx_float32)car->pixel_box.xmax) * 0.5F;
    car->bottom_center.y = (vx_float32)car->pixel_box.ymax;

    car->height_px =
        (vx_float32)(car->pixel_box.ymax - car->pixel_box.ymin);

    car->dh_dt = 0.0F;
    car->ttc_sec = 0.0F;
    car->r_squared = 0.0F;
    car->history_length = 0;
    car->ttc_valid = false;
    car->alert = false;

    return car->height_px > 0.0F;
}

static void fcw_main_pre_update_ttc(
    FcwMainPreContext *pre,
    FcwCar *car,
    vx_int32 frame_id,
    vx_int32 channel,
    vx_int32 track_id
)
{
    FcwTtcTrack *ttc_track;
    fcw_ttc_data_t ttc_data;
    vx_float32 ttc = 0.0F;
    bool already_updated;

    if ((pre == NULL) || (car == NULL) || (track_id < 0))
    {
        return;
    }

    if (channel != pre->context.config.ttc_channel)
    {
        return;
    }

    ttc_track = fcw_ttc_get_or_create(
        &pre->context.ttc,
        channel,
        track_id
    );

    if (ttc_track == NULL)
    {
        return;
    }

    already_updated =
        (ttc_track->ttc_data_array.history_count > 0) &&
        (ttc_track->ttc_data_array.frame_id_history[0] == frame_id) &&
        (ttc_track->ttc_data_array.track_id_history[0] == track_id);

    if (!already_updated)
    {
        memset(&ttc_data, 0, sizeof(ttc_data));
        ttc_data.frame_id = frame_id;
        ttc_data.track_id = track_id;

        calculate_height(
            (vx_float32)car->pixel_box.ymin,
            (vx_float32)car->pixel_box.ymax,
            &ttc_data
        );

        /*
         * pixel_boxの高さを履歴へ入れる。
         * FcwCar.height_pxと同じ座標系を使うため、ログ値とも一致する。
         */
        if (ttc_data.height > 0.0F)
        {
            (void)ttc_update(
                &ttc_data,
                &ttc_track->ttc_data_array
            );
        }
    }

    car->history_length =
        ttc_track->ttc_data_array.history_count;

    /* 現在のcalculate_ttc()は10点固定なので、10点蓄積後に計算する */
    if ((pre->context.config.fps > 0) &&
        calculate_ttc(
            pre->context.config.fps,
            &ttc_track->ttc_data_array,
            &ttc
        ))
    {
        car->ttc_sec = ttc;
        car->ttc_valid = true;
    }
}

void fcw_main_pre_init(
    FcwMainPreContext *pre,
    const FcwConfig *config
)
{
    FcwConfig effective_config;

    if (pre == NULL)
    {
        return;
    }

    memset(pre, 0, sizeof(*pre));

    if (config == NULL)
    {
        fcw_config_set_defaults(&effective_config);
    }
    else
    {
        effective_config = *config;
    }

    fcw_context_init(&pre->context, &effective_config);
    fcw_frame_result_reset(&pre->frame_result, 0, 0.0F);
    pre->frame_started = false;
}

void fcw_main_pre_reset(FcwMainPreContext *pre)
{
    if (pre == NULL)
    {
        return;
    }

    fcw_context_reset(&pre->context);
    fcw_frame_result_reset(&pre->frame_result, 0, 0.0F);
    pre->frame_started = false;
}

void fcw_main_pre_begin_frame(
    FcwMainPreContext *pre,
    vx_int32 frame_id
)
{
    vx_float32 time_s = 0.0F;

    if (pre == NULL)
    {
        return;
    }

    if (pre->context.config.fps > 0)
    {
        time_s = frame_id / (vx_float32)pre->context.config.fps;
    }

    fcw_frame_result_reset(&pre->frame_result, frame_id, time_s);
    pre->frame_started = true;
}

bool fcw_main_pre_add_tidl_object(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id,
    const TIDL_ODLayerObjInfo *tidl_object
)
{
    FcwCar candidate;
    FcwCar *car;

    if ((pre == NULL) || (!pre->frame_started) || (tidl_object == NULL))
    {
        return false;
    }

    if ((vx_float32)tidl_object->score <
        pre->context.config.score_threshold)
    {
        return false;
    }

    if ((vx_int32)tidl_object->label !=
        pre->context.config.car_class_id)
    {
        return false;
    }

    if (!fcw_main_pre_fill_car(
            &candidate,
            &pre->context.config,
            channel,
            track_id,
            tidl_object))
    {
        return false;
    }

    car = fcw_frame_result_add_car(&pre->frame_result);
    if (car == NULL)
    {
        return false;
    }

    *car = candidate;

    fcw_main_pre_update_ttc(
        pre,
        car,
        pre->frame_result.frame_id,
        channel,
        track_id
    );

    return true;
}

const FcwFrameResult *fcw_main_pre_get_frame_result(
    const FcwMainPreContext *pre
)
{
    if (pre == NULL)
    {
        return NULL;
    }

    return &pre->frame_result;
}

FcwTtcTrack *fcw_main_pre_get_ttc_track(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id
)
{
    if (pre == NULL)
    {
        return NULL;
    }

    return fcw_ttc_get_or_create(
        &pre->context.ttc,
        channel,
        track_id
    );
}

void fcw_main_pre_remove_ttc_track(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id
)
{
    if (pre == NULL)
    {
        return;
    }

    fcw_ttc_remove(&pre->context.ttc, channel, track_id);
}

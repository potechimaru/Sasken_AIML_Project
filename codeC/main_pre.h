#ifndef FCW_MAIN_PRE_H_
#define FCW_MAIN_PRE_H_

/* TIDL object-detectionの出力を現在フレーム用バッファへコピーするアダプタ。 */

#include <stdbool.h>

#include <VX/vx.h>

#include "avp_fcw_ttc.h"
#include "fcw_types.h"
#include "itidl_ti.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    vx_int32 image_width;
    vx_int32 image_height;
    vx_int32 fps;
    vx_float32 score_threshold;
    vx_int32 car_class_id;

    /* vx_true_e: TIDL bboxは0.0〜1.0、vx_false_e: ピクセル座標。 */
    vx_bool tidl_bbox_is_normalized;
} FcwMainPreConfig;

typedef struct
{
    /* フレーム間で保持するTTC履歴。track_idは全channelで一意とする。 */
    fcw_ttc_manager_t ttc_manager;

    /* begin_frame()から次のbegin_frame()まで有効な一時出力。 */
    FcwFrameResult frame_result;
    FcwMainPreConfig config;
    vx_bool frame_started;
} FcwMainPreContext;

void fcw_main_pre_config_set_defaults(FcwMainPreConfig *config);
void fcw_main_pre_init(FcwMainPreContext *pre, const FcwMainPreConfig *config);
void fcw_main_pre_reset(FcwMainPreContext *pre);
void fcw_main_pre_begin_frame(FcwMainPreContext *pre, vx_int32 frame_index);

/*
 * TIDL検出1件を現在フレームのcars[]にコピーする。
 * track_idはtracker導入前なら-1を渡す。その場合TTC履歴は更新しない。
 * car_class_id/score_thresholdを満たさない検出はfalseを返し保存しない。
 */
bool fcw_main_pre_add_tidl_object(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id,
    const TIDL_ODLayerObjInfo *tidl_object
);

const FcwFrameResult *fcw_main_pre_get_frame_result(
    const FcwMainPreContext *pre
);

#ifdef __cplusplus
}
#endif

#endif /* FCW_MAIN_PRE_H_ */

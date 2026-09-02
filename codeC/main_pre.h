#ifndef FCW_MAIN_PRE_H_
#define FCW_MAIN_PRE_H_

/* main.cから呼ぶTIDL検出→ROI→TTCのアダプタ。 */

#include <VX/vx.h>

#include "avp_fcw_roi.h"
#include "avp_fcw_ttc.h"
#include "fcw_types.h"
#include "itidl_ti.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    vx_float32 score_threshold;
    vx_int32 car_class_id;
    vx_int32 fps;
    vx_int32 ttc_channel;
    vx_int32 image_width;
    vx_int32 image_height;
    vx_bool tidl_bbox_is_normalized;
} FcwMainPreConfig;

typedef struct
{
    FcwMainPreConfig config;
    FcwFrameResult frame_result;
    fcw_ttc_manager_t ttc_manager;
    vx_bool frame_started;
} FcwMainPreContext;

void fcw_main_pre_config_set_defaults(FcwMainPreConfig *config);
void fcw_main_pre_init(FcwMainPreContext *pre, const FcwMainPreConfig *config);
void fcw_main_pre_reset(FcwMainPreContext *pre);
void fcw_main_pre_begin_frame(FcwMainPreContext *pre, vx_int32 frame_index);

/*
 * TIDL検出1件を正規化して保存し、ROIを判定する。
 * track_idはtrackerが割り当てた全channelで一意なIDを渡す。tracker未接続なら-1。
 * ROI内、指定channel、track_id>=0 のときのみTTC履歴・TTCを更新する。
 */
vx_bool fcw_main_pre_add_tidl_object(
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

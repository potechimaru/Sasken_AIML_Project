#ifndef FCW_MAIN_PRE_H_
#define FCW_MAIN_PRE_H_

/* TIDLの検出1件を FcwFrameResult へコピーする入力アダプタ。 */

#include <stdbool.h>

#include <VX/vx.h>

#include "fcw_types.h"
#include "itidl_ti.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    vx_float32 score_threshold;
    vx_int32 car_class_id;
} FcwMainPreConfig;

typedef struct
{
    FcwMainPreConfig config;
    FcwFrameResult frame_result;
    vx_bool frame_started;
} FcwMainPreContext;

void fcw_main_pre_config_set_defaults(FcwMainPreConfig *config);
void fcw_main_pre_init(FcwMainPreContext *pre, const FcwMainPreConfig *config);
void fcw_main_pre_begin_frame(FcwMainPreContext *pre, vx_int32 frame_index);

/*
 * TIDL検出1件を保存する。bbox座標はTIDLの値を変換せずそのままコピーする。
 * scoreまたはclass IDが設定条件を満たさない場合はfalseを返し、保存しない。
 */
bool fcw_main_pre_add_tidl_object(
    FcwMainPreContext *pre,
    vx_int32 channel,
    const TIDL_ODLayerObjInfo *tidl_object
);

const FcwFrameResult *fcw_main_pre_get_frame_result(
    const FcwMainPreContext *pre
);

#ifdef __cplusplus
}
#endif

#endif /* FCW_MAIN_PRE_H_ */

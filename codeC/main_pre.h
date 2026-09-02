#ifndef FCW_MAIN_PRE_H
#define FCW_MAIN_PRE_H

/*
 * 目的:
 * TIDLの1フレーム分の検出結果を、Notionで定義したFCWの共有データと
 * TTCのtrack_id別履歴へ変換するための公開インターフェース。
 *
 * このファイルを使うmain.c側では、TIDLテンソルをmapして得た
 * TIDL_ODLayerObjInfoをfcw_main_pre_add_tidl_object()へ渡すだけにする。
 */

#include <stdbool.h>

#include <VX/vx.h>

#include "fcw_types.h"
#include "itidl_ti.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    /* app_init()で一度初期化し、フレーム間で保持する状態 */
    FcwContext context;

    /* begin_frame()から次のbegin_frame()まで保持する現在フレーム結果 */
    FcwFrameResult frame_result;
    bool frame_started;
} FcwMainPreContext;

void fcw_main_pre_init(
    FcwMainPreContext *pre,
    const FcwConfig *config
);

void fcw_main_pre_reset(FcwMainPreContext *pre);

void fcw_main_pre_begin_frame(
    FcwMainPreContext *pre,
    vx_int32 frame_id
);

/*
 * map中のTIDL_ODLayerObjInfoを1台分受け取り、
 * FcwCar、fcw_ttc_data_t、track_id別fcw_ttc_array_data_tを更新する。
 * track_idはTIDLのObjIdではなく、Trackerが割り当てた値を渡す。
 */
bool fcw_main_pre_add_tidl_object(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id,
    const TIDL_ODLayerObjInfo *tidl_object
);

/*
 * main.c側の使用順序（このヘッダをincludeして呼び出すだけ）:
 *
 *   FcwMainPreContext fcw_pre;
 *   fcw_main_pre_init(&fcw_pre, NULL);       // app_init()で1回
 *   fcw_main_pre_begin_frame(&fcw_pre, frame_id);
 *   fcw_main_pre_add_tidl_object(&fcw_pre, ch, track_id, pObject);
 *
 * pObjectはTIDL tensorのmap中に渡す。track_idがまだ無い場合は、
 * 1台だけの動作確認に限って0を渡し、複数台ではTrackerのIDを渡す。
 */

const FcwFrameResult *fcw_main_pre_get_frame_result(
    const FcwMainPreContext *pre
);

FcwTtcTrack *fcw_main_pre_get_ttc_track(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id
);

void fcw_main_pre_remove_ttc_track(
    FcwMainPreContext *pre,
    vx_int32 channel,
    vx_int32 track_id
);

#ifdef __cplusplus
}
#endif

#endif /* FCW_MAIN_PRE_H */

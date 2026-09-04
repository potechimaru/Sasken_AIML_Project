#ifndef FCW_ALERT_H_
#define FCW_ALERT_H_

#include "fcw_types.h"

#define FCW_ALERT_MAX_TRACKS (128u)

#define FCW_ALERT_DEFAULT_ON_TTC_THRESHOLD (4.0F)
#define FCW_ALERT_DEFAULT_OFF_TTC_THRESHOLD (6.5F)
#define FCW_ALERT_DEFAULT_REQUIRED_COUNT (3u)


/* 1台の車両に対するAlert状態 */
typedef struct
{
    vx_bool in_use;
    vx_int32 track_id;
    vx_bool alert_status;
    vx_uint32 consecutive_danger_count;

} FcwAlertTrackState;


/* Alert全体の管理情報 */
typedef struct
{
    vx_bool initialized;
    vx_float32 on_threshold;
    vx_float32 off_threshold;
    vx_uint32 required_count;

    vx_uint32 num_states;

    FcwAlertTrackState states[FCW_ALERT_MAX_TRACKS];

} FcwAlertController;


/* 初期化 */
vx_status fcw_alert_init(
    FcwAlertController *controller,
    vx_float32 on_threshold,
    vx_float32 off_threshold,
    vx_uint32 required_count
);


/* 全車両の状態をリセット */
vx_status fcw_alert_reset(
    FcwAlertController *controller
);


/* 1台分のAlert判定 */
vx_status fcw_alert_update(
    FcwAlertController *controller,
    FcwCar *car
);


/* 追跡終了車両の状態を破棄 */
vx_status fcw_alert_drop(
    FcwAlertController *controller,
    vx_int32 track_id
);


/* 終了処理 */
void fcw_alert_deinit(
    FcwAlertController *controller
);

#endif

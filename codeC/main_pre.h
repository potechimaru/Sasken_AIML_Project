#ifndef FCW_MAIN_PRE_H_
#define FCW_MAIN_PRE_H_

#include "fcw_alert.h"
#include "fcw_alarm.h"
#include "fcw_tidl_adapter.h"
#include "fcw_tracker.h"
#include "fcw_ttc.h"
#include "avp_fcw_roi.h"

typedef struct
{
    vx_float32 score_threshold;
    vx_int32 car_class_id, fps;
    vx_uint32 ttc_channel;
    vx_float32 tracker_iou_threshold;
    vx_uint32 tracker_max_missed_frames;
    vx_float32 alert_on_ttc_sec, alert_off_ttc_sec;
    vx_uint32 alert_required_count;
} FcwMainPreConfig;

typedef struct
{
    FcwMainPreConfig config;
    FcwFrameResult frame_result;
    FcwTrackerContext tracker;
    fcw_ttc_manager_t ttc_manager;
    FcwAlertController alert_controller;
    FcwAlarmContext alarm;
} FcwMainPreContext;

void fcw_main_pre_config_set_defaults(FcwMainPreConfig *config);
vx_status fcw_main_pre_init(FcwMainPreContext *context,const FcwMainPreConfig *config,FcwAlarmOutputFunc output,void *user);
void fcw_main_pre_deinit(FcwMainPreContext *context);
vx_status fcw_main_pre_begin_frame(FcwMainPreContext *context,vx_int32 frame_index);
vx_status fcw_main_pre_add_detection(FcwMainPreContext *context,const FcwDetection *detection);
vx_status fcw_main_pre_add_tidl_channel(FcwMainPreContext *context,const sTIDL_IOBufDesc_t *io,vx_object_array output,vx_uint32 channel);
vx_status fcw_main_pre_finalize_frame(FcwMainPreContext *context);

/*
 * main.c用の1フレーム統合入口。
 * TIDL outputを全channelから取り込み、ROI → tracker → TTC → alert → alarm
 * を順に実行する。alarm出力は init() で指定したcallbackへ通知される。
 */
vx_status fcw_main_pre_process_tidl_frame(
    FcwMainPreContext *context,
    const sTIDL_IOBufDesc_t *io,
    vx_object_array output,
    vx_uint32 num_channels,
    vx_int32 frame_index
);

const FcwFrameResult *fcw_main_pre_get_frame_result(const FcwMainPreContext *context);

#endif

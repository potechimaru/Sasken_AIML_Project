// 追加
#ifndef _AVP_FCW_MODULE
#define _AVP_FCW_MODULE

#include "avp_common.h"
#include "itidl_ti.h"
#include <math.h>

#define AVP_FCW_MAX_DETECTIONS (100u)
#define AVP_FCW_MAX_TRACKS     (AVP_FCW_MAX_DETECTIONS)
#define AVP_FCW_MAX_HISTORY    (10u)
#define AVP_FCW_INVALID_VALUE  (NAN)

/* A detection uses the same normalized [ymin, xmin, ymax, xmax]
 * representation as the Python detector. */
typedef struct {
    vx_float32 ymin;
    vx_float32 xmin;
    vx_float32 ymax;
    vx_float32 xmax;
    vx_float32 score;
    vx_int32 class_id;
} AvpFwcDetection;

typedef struct {
    vx_float32 fps;
    vx_float32 score_threshold;
    vx_int32 target_class_id;

    vx_float32 iou_threshold;
    vx_uint32 max_missed_frames;

    vx_uint32 history_size;
    vx_uint32 min_history_size;
    vx_float32 r_squared_threshold;

    vx_float32 on_ttc_threshold;
    vx_float32 off_ttc_threshold;
    vx_uint32 required_count;

    /* ROI vertices in normalized image coordinates, in clockwise order. */
    vx_float32 roi[4][2];
} AvpFwcConfig;

typedef struct {
    vx_uint32 track_id;
    vx_float32 score;
    vx_float32 xmin;
    vx_float32 ymin;
    vx_float32 xmax;
    vx_float32 ymax;
    vx_float32 height_px;
    vx_int32 bottom_center_x;
    vx_int32 bottom_center_y;
    vx_float32 iou;

    /* NAN means that Python's None would be returned. */
    vx_float32 dh_dt;
    vx_float32 ttc;
    vx_float32 r_squared;
    vx_uint32 history_length;
    vx_bool alert;
} AvpFwcResult;

typedef struct {
    vx_bool active;
    vx_uint32 track_id;
    vx_float32 box[4];
    vx_uint32 missed_frames;
    vx_uint32 alert_count;
    vx_bool alert;
    vx_uint32 history_count;
    vx_uint32 history_frame[AVP_FCW_MAX_HISTORY];
    vx_float32 history_height[AVP_FCW_MAX_HISTORY];
} AvpFwcTrack;

typedef struct {
    AvpFwcConfig config;
    AvpFwcTrack tracks[AVP_FCW_MAX_TRACKS];
    vx_uint32 next_track_id;
} AvpFwcContext;

typedef struct {
    AvpFwcResult detections[AVP_FCW_MAX_DETECTIONS];
    vx_uint32 num_detections;
    vx_bool alert_active;
} AvpFwcFrameResult;

void avp_fcw_config_set_defaults(AvpFwcConfig *config);
vx_status avp_fcw_init(AvpFwcContext *context, const AvpFwcConfig *config);
void avp_fcw_reset(AvpFwcContext *context);

/* Process already decoded detector outputs. */
vx_status avp_fcw_process(AvpFwcContext *context,
                          const AvpFwcDetection *detections,
                          vx_uint32 num_detections,
                          vx_uint32 frame_index,
                          vx_uint32 image_width,
                          vx_uint32 image_height,
                          AvpFwcFrameResult *result);

/* Decode one TIDL OD output tensor, then run avp_fcw_process(). */
vx_status avp_fcw_process_tidl(AvpFwcContext *context,
                                vx_tensor output_tensor,
                                const sTIDL_IOBufDesc_t *io_buf_desc,
                                vx_uint32 frame_index,
                                vx_uint32 image_width,
                                vx_uint32 image_height,
                                AvpFwcFrameResult *result);

#endif

#ifndef FCW_TYPES_H_
#define FCW_TYPES_H_

#include "avp_common.h"

#define FCW_MAX_DETECTIONS (100U)
#define FCW_MAX_TRACKS     (100U)

typedef struct
{
    vx_float32 ymin;
    vx_float32 xmin;
    vx_float32 ymax;
    vx_float32 xmax;
} FcwNormalizedBox;

typedef struct
{
    vx_int32 x1;
    vx_int32 y1;
    vx_int32 x2;
    vx_int32 y2;
} FcwPixelBox;

typedef struct
{
    vx_int32 x;
    vx_int32 y;
} FcwPoint;

typedef struct
{
    /* detector.py */
    FcwNormalizedBox box;
    vx_float32 score;
    vx_int32 class_id;

    /* roi.py */
    FcwPixelBox pixel_box;
    FcwPoint bottom_center;
    vx_float32 height_px;
    vx_bool roi_valid;

    /* tracker.py */
    vx_int32 track_id;
    vx_float32 iou;
    vx_bool track_valid;

    /* ttc.py */
    vx_float32 dh_dt;
    vx_bool dh_dt_valid;
    vx_float32 ttc_sec;
    vx_bool ttc_valid;
    vx_float32 r_squared;
    vx_bool r_squared_valid;
    vx_uint32 history_length;

    /* alert.py */
    vx_bool alert;

} FcwCar;

typedef struct
{
    /* main.cのframe_idがvx_int32なので合わせる */
    vx_int32 frame_index;
    vx_float32 time_s;

    vx_uint32 num_cars;
    FcwCar cars[FCW_MAX_DETECTIONS];

    /* 全車両のAlertを集約した状態 */
    vx_bool alarm_active;

} FcwFrameResult;
#ifndef FCW_TYPES_H_
#define FCW_TYPES_H_

#include "avp_common.h"


#define FCW_MAX_DETECTIONS   (100u)
#define FCW_MAX_TRACKS       (128u)
#define FCW_MAX_ROI_POINTS   (8u)
#define FCW_TTC_MAX_HISTORY  (16u)


/* Bounding box in normalized coordinates: [ymin, xmin, ymax, xmax]. */
typedef struct
{
    vx_float32 ymin;
    vx_float32 xmin;
    vx_float32 ymax;
    vx_float32 xmax;

} FcwBox;


/* One detection converted from the TIDL OD output. */
typedef struct
{
    FcwBox    box;
    vx_float32 score;
    vx_int32  class_id;
    vx_int32  object_id;

} FcwDetection;


/* Shared car information used by the later FCW modules. */
typedef struct
{
    FcwBox    box;

    vx_float32 score;
    vx_int32  class_id;
    vx_int32  object_id;

    vx_int32  x1;
    vx_int32  y1;
    vx_int32  x2;
    vx_int32  y2;

    vx_int32  bottom_center_x;
    vx_int32  bottom_center_y;
    vx_float32 height_px;

    vx_int32  track_id;
    vx_float32 iou;


    vx_float32 ttc;
    vx_bool    ttc_valid;

    vx_uint32 history_length;
    vx_bool   alert;

} FcwCar;


typedef struct
{
    vx_int32  frame_index;
    vx_uint32 num_cars;
    vx_bool   any_alert;
    FcwCar    cars[FCW_MAX_DETECTIONS];

} FcwFrameResult;

#endif

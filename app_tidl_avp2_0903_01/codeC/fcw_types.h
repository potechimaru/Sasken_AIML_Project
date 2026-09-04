#ifndef FCW_TYPES_H_
#define FCW_TYPES_H_

#include <VX/vx.h>

#define FCW_MAX_DETECTIONS (100U)
#define FCW_MAX_TRACKS (128U)

/* All FCW modules use this normalized bbox: [ymin, xmin, ymax, xmax]. */
typedef struct { vx_float32 ymin, xmin, ymax, xmax; } FcwBox;

typedef struct
{
    FcwBox box;
    vx_float32 score;
    vx_int32 class_id;
    vx_int32 object_id;
    vx_uint32 channel;
} FcwDetection;

/* One current-frame FCW candidate. Fields below are the module hand-off. */
typedef struct
{
    FcwBox box;
    vx_float32 score;
    vx_int32 class_id;
    vx_int32 object_id;
    vx_uint32 channel;

    vx_bool roi_valid;
    vx_int32 track_id;
    vx_float32 iou;

    vx_uint32 history_length;
    vx_float32 ttc_sec;
    vx_bool ttc_valid;
    vx_bool alert;
} FcwCar;

typedef struct
{
    vx_int32 frame_index;
    vx_uint32 num_cars;
    vx_uint32 dropped_cars;
    vx_bool any_alert;
    FcwCar cars[FCW_MAX_DETECTIONS];
} FcwFrameResult;

vx_status fcw_frame_result_reset(FcwFrameResult *result, vx_int32 frame_index);
vx_status fcw_frame_result_add_car(FcwFrameResult *result, FcwCar **car);

#endif /* FCW_TYPES_H_ */

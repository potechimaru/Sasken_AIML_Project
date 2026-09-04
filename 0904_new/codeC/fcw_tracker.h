#ifndef FCW_TRACKER_H_
#define FCW_TRACKER_H_

#include "fcw_types.h"


typedef struct
{
    vx_bool   in_use;
    vx_int32  track_id;

    FcwBox    box;
    vx_uint32 missed_frames;

} FcwTrackerTrack;


typedef struct
{
    vx_bool initialized;

    vx_float32 iou_threshold;
    vx_uint32 max_missed_frames;

    vx_int32 next_track_id;
    vx_uint32 num_tracks;

    FcwTrackerTrack tracks[FCW_MAX_TRACKS];

} FcwTrackerContext;


vx_status fcw_tracker_init(
    FcwTrackerContext *context,
    vx_float32 iou_threshold,
    vx_uint32 max_missed_frames
);


vx_float32 fcw_tracker_calculate_iou(
    const FcwBox *box1,
    const FcwBox *box2
);


vx_status fcw_tracker_update(
    FcwTrackerContext *context,
    FcwCar *cars,
    vx_uint32 num_cars,
    vx_int32 *expired_track_ids,
    vx_uint32 expired_capacity,
    vx_uint32 *expired_count
);


vx_status fcw_tracker_reset(
    FcwTrackerContext *context
);


void fcw_tracker_deinit(
    FcwTrackerContext *context
);

#endif
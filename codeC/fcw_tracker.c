#include "fcw_tracker.h"


static vx_int32 fcw_tracker_find_free_index(
    FcwTrackerContext *context
)
{
    vx_uint32 i;

    for (i = 0u; i < FCW_MAX_TRACKS; i++)
    {
        if (context->tracks[i].in_use != vx_true_e)
        {
            return (vx_int32)i;
        }
    }

    return -1;
}


/*
 * 2つのBounding BoxのIoUを計算する。
 *
 * Python:
 *   calculate_iou(box1, box2)
 */
vx_float32 fcw_tracker_calculate_iou(
    const FcwBox *box1,
    const FcwBox *box2
)
{
    vx_float32 intersection_xmin;
    vx_float32 intersection_ymin;
    vx_float32 intersection_xmax;
    vx_float32 intersection_ymax;

    vx_float32 intersection_width;
    vx_float32 intersection_height;
    vx_float32 intersection_area;

    vx_float32 area1;
    vx_float32 area2;
    vx_float32 union_area;

    if ((box1 == NULL) || (box2 == NULL))
    {
        return 0.0F;
    }

    intersection_xmin =
        (box1->xmin > box2->xmin) ?
        box1->xmin : box2->xmin;

    intersection_ymin =
        (box1->ymin > box2->ymin) ?
        box1->ymin : box2->ymin;

    intersection_xmax =
        (box1->xmax < box2->xmax) ?
        box1->xmax : box2->xmax;

    intersection_ymax =
        (box1->ymax < box2->ymax) ?
        box1->ymax : box2->ymax;

    intersection_width =
        intersection_xmax - intersection_xmin;

    intersection_height =
        intersection_ymax - intersection_ymin;

    if (intersection_width < 0.0F)
    {
        intersection_width = 0.0F;
    }

    if (intersection_height < 0.0F)
    {
        intersection_height = 0.0F;
    }

    intersection_area =
        intersection_width * intersection_height;

    area1 =
        (box1->xmax - box1->xmin) *
        (box1->ymax - box1->ymin);

    area2 =
        (box2->xmax - box2->xmin) *
        (box2->ymax - box2->ymin);

    if ((area1 <= 0.0F) ||
        (area2 <= 0.0F))
    {
        return 0.0F;
    }

    union_area =
        area1 + area2 - intersection_area;

    if (union_area <= 0.0F)
    {
        return 0.0F;
    }

    return intersection_area / union_area;
}


/*
 * Trackerを初期化する。
 */
vx_status fcw_tracker_init(
    FcwTrackerContext *context,
    vx_float32 iou_threshold,
    vx_uint32 max_missed_frames
)
{
    if (context == NULL)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if ((iou_threshold < 0.0F) ||
        (iou_threshold > 1.0F))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memset(context, 0, sizeof(FcwTrackerContext));

    context->iou_threshold = iou_threshold;
    context->max_missed_frames = max_missed_frames;

    /*
     * Python版のnext_track_id = 1に対応。
     */
    context->next_track_id = 1;
    context->num_tracks = 0u;
    context->initialized = vx_true_e;

    return VX_SUCCESS;
}


/*
 * 1フレーム分のTracker処理。
 *
 * Python:
 *   tracker.update(cars)
 */
vx_status fcw_tracker_update(
    FcwTrackerContext *context,
    FcwCar *cars,
    vx_uint32 num_cars,
    vx_int32 *expired_track_ids,
    vx_uint32 expired_capacity,
    vx_uint32 *expired_count
)
{
    vx_uint32 i;
    vx_uint32 j;
    vx_uint32 candidate_count;

    vx_int32 candidate_indices[FCW_MAX_TRACKS];
    vx_bool matched[FCW_MAX_TRACKS];

    vx_int32 best_index;
    vx_int32 free_index;
    vx_float32 best_iou;
    vx_float32 current_iou;

    vx_status status;
    FcwTrackerTrack *track;

    if ((context == NULL) ||
        (expired_count == NULL))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (context->initialized != vx_true_e)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if ((num_cars > 0u) &&
        (cars == NULL))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if ((expired_capacity > 0u) &&
        (expired_track_ids == NULL))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (num_cars > FCW_MAX_DETECTIONS)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    *expired_count = 0u;
    status = VX_SUCCESS;

    memset(matched, 0, sizeof(matched));

    /*
     * 既存Trackを一旦「未検出」とする。
     *
     * Python:
     *   track["missed_frames"] += 1
     */
    candidate_count = 0u;

    for (i = 0u; i < FCW_MAX_TRACKS; i++)
    {
        track = &context->tracks[i];

        if (track->in_use == vx_true_e)
        {
            track->missed_frames++;

            /*
             * この時点で存在していたTrackだけを候補にする。
             */
            candidate_indices[candidate_count] = (vx_int32)i;
            candidate_count++;
        }
    }

    /*
     * 現フレームの各車両を既存Trackへ割り当てる。
     */
    for (i = 0u; i < num_cars; i++)
    {
        cars[i].track_id = -1;
        cars[i].iou = 0.0F;

        best_index = -1;
        best_iou = 0.0F;

        for (j = 0u; j < candidate_count; j++)
        {
            vx_int32 candidate_index;

            candidate_index = candidate_indices[j];

            if (matched[candidate_index] == vx_true_e)
            {
                continue;
            }

            track = &context->tracks[candidate_index];

            current_iou =
                fcw_tracker_calculate_iou(
                    &track->box,
                    &cars[i].box
                );

            /*
             * Python版と同じく、最大IoUを持つTrackを選ぶ。
             */
            if (current_iou > best_iou)
            {
                best_iou = current_iou;
                best_index = candidate_index;
            }
        }

        /*
         * 十分なIoUがある場合は既存Trackを引き継ぐ。
         */
        if ((best_index >= 0) &&
            (best_iou >= context->iou_threshold))
        {
            track = &context->tracks[best_index];

            cars[i].track_id = track->track_id;
            cars[i].iou = best_iou;

            track->box = cars[i].box;
            track->missed_frames = 0u;

            matched[best_index] = vx_true_e;
        }
        else
        {
            /*
             * 対応するTrackがない場合は新規Trackを作る。
             */
            free_index =
                fcw_tracker_find_free_index(context);

            if (free_index < 0)
            {
                /*
                 * 同時追跡数が上限を超えた。
                 */
                status = VX_FAILURE;
                continue;
            }

            track = &context->tracks[free_index];

            memset(track, 0, sizeof(FcwTrackerTrack));

            track->in_use = vx_true_e;
            track->track_id = context->next_track_id;
            track->box = cars[i].box;
            track->missed_frames = 0u;

            cars[i].track_id = track->track_id;
            cars[i].iou = best_iou;

            context->next_track_id++;
            context->num_tracks++;
        }
    }

    /*
     * 一定フレーム以上見失ったTrackを削除する。
     *
     * Python:
     *   missed_frames > max_missed_frames
     */
    for (i = 0u; i < FCW_MAX_TRACKS; i++)
    {
        track = &context->tracks[i];

        if ((track->in_use == vx_true_e) &&
            (track->missed_frames >
             context->max_missed_frames))
        {
            if (*expired_count < expired_capacity)
            {
                expired_track_ids[*expired_count] =
                    track->track_id;

                (*expired_count)++;
            }
            else
            {
                status = VX_FAILURE;
            }

            memset(track, 0, sizeof(FcwTrackerTrack));

            if (context->num_tracks > 0u)
            {
                context->num_tracks--;
            }
        }
    }

    return status;
}


/*
 * Trackerの状態を全てリセットする。
 */
vx_status fcw_tracker_reset(
    FcwTrackerContext *context
)
{
    if (context == NULL)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (context->initialized != vx_true_e)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memset(context->tracks, 0, sizeof(context->tracks));

    context->next_track_id = 1;
    context->num_tracks = 0u;

    return VX_SUCCESS;
}


void fcw_tracker_deinit(
    FcwTrackerContext *context
)
{
    if (context != NULL)
    {
        memset(context, 0, sizeof(FcwTrackerContext));
    }
}
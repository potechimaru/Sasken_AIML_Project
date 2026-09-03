#include "avp_fcw_tracker.h"

/*
 * 外部ファイル（main.c）から呼び出す公開関数。
 * 詳細な機能、引数、戻り値についてはavp_fcw_tracker.hにも記載している。
 *
 * main.cでの基本的な呼び出し順は、
 *   1. fcw_iou_tracker_init()   : メインループの前に1回呼び出す
 *   2. fcw_iou_tracker_update() : Frameごとに呼び出す
 * である。
 *
 * fcw_calculate_iou()は通常fcw_iou_tracker_update()内部から呼ばれる。
 * IoUの単体確認を行う場合は、main.cから直接呼び出してもよい。
 */
// ここから
void fcw_iou_tracker_init(
    FcwIouTracker *tracker,
    float iou_threshold,
    int max_missed_frames
);

float fcw_calculate_iou(
    const float box1[4],
    const float box2[4]
);

int fcw_iou_tracker_update(
    FcwIouTracker *tracker,
    const float boxes[][4],
    int num_boxes,
    int track_ids[],
    float best_ious[],
    int expired_ids[],
    int max_expired
);
// ここまでの3つの関数が外部ファイルから呼び出せる公開関数

#include <string.h>

/* 2つの値のうち大きい方を返す内部関数。 */
static float fcw_tracker_max_float(float value1, float value2)
{
    return (value1 > value2) ? value1 : value2;
}

/* 2つの値のうち小さい方を返す内部関数。 */
static float fcw_tracker_min_float(float value1, float value2)
{
    return (value1 < value2) ? value1 : value2;
}

/* 未使用のTrack配列要素を探す。見つからない場合は-1を返す。 */
static int fcw_tracker_find_free_track(const FcwIouTracker *tracker)
{
    int i;

    for(i = 0; i < FCW_MAX_TRACKS; i++)
    {
        if(tracker->tracks[i].active == 0)
        {
            return i;
        }
    }

    return -1;
}

/*
 * Trackerを初期化する。
 *
 * tracker          : 初期化対象のTracker。
 * iou_threshold    : 同一車両とみなすIoUの下限。
 * max_missed_frames: Trackを削除するまで許容する未検出Frame数。
 */
void fcw_iou_tracker_init(
    FcwIouTracker *tracker,
    float iou_threshold,
    int max_missed_frames
)
{
    if(tracker == NULL)
    {
        return;
    }

    memset(tracker, 0, sizeof(*tracker));
    tracker->next_track_id = 1;
    tracker->iou_threshold = iou_threshold;
    tracker->max_missed_frames = max_missed_frames;
}

/*
 * 2つのBBoxのIoUを計算する。
 *
 * box1 : 1つ目のBBox。要素は[xmin,ymin,xmax,ymax]。
 * box2 : 2つ目のBBox。要素は[xmin,ymin,xmax,ymax]。
 *
 * 戻り値 : 0.0〜1.0のIoU。不正な面積の場合は0.0。
 */
float fcw_calculate_iou(
    const float box1[4],
    const float box2[4]
)
{
    float ixmin;
    float iymin;
    float ixmax;
    float iymax;
    float iw;
    float ih;
    float intersection;
    float area1;
    float area2;
    float union_area;

    if((box1 == NULL) || (box2 == NULL))
    {
        return 0.0F;
    }

    ixmin = fcw_tracker_max_float(box1[FCW_XMIN], box2[FCW_XMIN]);
    iymin = fcw_tracker_max_float(box1[FCW_YMIN], box2[FCW_YMIN]);
    ixmax = fcw_tracker_min_float(box1[FCW_XMAX], box2[FCW_XMAX]);
    iymax = fcw_tracker_min_float(box1[FCW_YMAX], box2[FCW_YMAX]);

    iw = ixmax - ixmin;
    ih = iymax - iymin;

    if(iw < 0.0F)
    {
        iw = 0.0F;
    }
    if(ih < 0.0F)
    {
        ih = 0.0F;
    }

    intersection = iw * ih;
    area1 = (box1[FCW_XMAX] - box1[FCW_XMIN]) *
            (box1[FCW_YMAX] - box1[FCW_YMIN]);
    area2 = (box2[FCW_XMAX] - box2[FCW_XMIN]) *
            (box2[FCW_YMAX] - box2[FCW_YMIN]);
    union_area = area1 + area2 - intersection;

    if(union_area <= 0.0F)
    {
        return 0.0F;
    }

    return intersection / union_area; // IoUを計算する
}

/*
 * 現在FrameのBBoxへtrack_idを付与し、期限切れTrackを削除する。
 *
 * Python版IouTracker.update()と同じく、
 * 既存Trackを検出順に最大IoUへ貪欲に割り当てる。
 */
int fcw_iou_tracker_update(
    FcwIouTracker *tracker,
    const float boxes[][4],
    int num_boxes,
    int track_ids[],
    float best_ious[],
    int expired_ids[],
    int max_expired
)
{
    int candidate_indices[FCW_MAX_TRACKS];
    int matched[FCW_MAX_TRACKS];
    int candidate_count = 0;
    int expired_count = 0;
    int i;
    int j;

    if((tracker == NULL) || (num_boxes < 0) || (max_expired < 0) ||
       ((num_boxes > 0) && (boxes == NULL)) ||
       ((num_boxes > 0) && (track_ids == NULL)) ||
       ((num_boxes > 0) && (best_ious == NULL)) ||
       ((max_expired > 0) && (expired_ids == NULL)))
    {
        return -1;
    }

    memset(matched, 0, sizeof(matched));

    /* 既存Trackを候補として保存し、未検出Frame数を増やす。 */
    for(i = 0; i < FCW_MAX_TRACKS; i++)
    {
        if(tracker->tracks[i].active != 0)
        {
            tracker->tracks[i].missed_frames++;
            candidate_indices[candidate_count] = i;
            candidate_count++;
        }
    }

    /* 現在FrameのBBoxを検出順に処理する。 */
    for(i = 0; i < num_boxes; i++)
    {
        float best_iou = 0.0F;
        int best_candidate = -1;
        int free_track;

        /* 未使用の既存Trackの中から最大IoUのTrackを探す。 */
        for(j = 0; j < candidate_count; j++)
        {
            int track_index = candidate_indices[j];
            float iou;

            if(matched[track_index] != 0)
            {
                continue;
            }

            iou = fcw_calculate_iou(
                tracker->tracks[track_index].box,
                boxes[i]
            );

            if(iou > best_iou)
            {
                best_iou = iou;
                best_candidate = track_index;
            }
        }

        best_ious[i] = best_iou;

        /* IoUが閾値以上なら既存IDを引き継ぐ。 */
        if((best_candidate >= 0) &&
           (best_iou >= tracker->iou_threshold))
        {
            FcwTrackState *track = &tracker->tracks[best_candidate];

            track_ids[i] = track->track_id;
            memcpy(track->box, boxes[i], sizeof(track->box));
            track->missed_frames = 0;
            matched[best_candidate] = 1;
        }
        else
        {
            /* 対応する既存Trackがなければ新規IDを発行する。 */
            free_track = fcw_tracker_find_free_track(tracker);
            if((free_track < 0) || (tracker->next_track_id <= 0))
            {
                /* 固定長Track領域が満杯の場合は、この検出を未割当とする。 */
                track_ids[i] = -1;
                continue;
            }

            tracker->tracks[free_track].active = 1;
            tracker->tracks[free_track].track_id = tracker->next_track_id;
            memcpy(tracker->tracks[free_track].box,
                   boxes[i],
                   sizeof(tracker->tracks[free_track].box));
            tracker->tracks[free_track].missed_frames = 0;

            track_ids[i] = tracker->next_track_id;
            tracker->next_track_id++;
        }
    }

    /* 許容Frame数を超えて未検出のTrackを削除する。 */
    for(i = 0; i < FCW_MAX_TRACKS; i++)
    {
        if((tracker->tracks[i].active != 0) &&
           (tracker->tracks[i].missed_frames >
            tracker->max_missed_frames))
        {
            if(expired_count < max_expired)
            {
                expired_ids[expired_count] = tracker->tracks[i].track_id;
                expired_count++;
            }

            memset(&tracker->tracks[i], 0, sizeof(tracker->tracks[i]));
        }
    }

    return expired_count;
}

#ifndef AVP_FCW_TRACKER_H_
#define AVP_FCW_TRACKER_H_

/*
 * IoU Trackingの単体モジュール。
 *
 * このヘッダはTI/OpenVX/TIDLに依存しない。
 * TIDLの検出結果は、呼び出し側でboxes[][4]へコピーして渡す。
 */

#define FCW_MAX_TRACKS (32)

#define FCW_XMIN (0)
#define FCW_YMIN (1)
#define FCW_XMAX (2)
#define FCW_YMAX (3)

/*
 * Trackerが前FrameのBBoxとtrack_idを保持するための内部状態。
 *
 * active        : この要素が現在使用中なら1、未使用なら0。
 * track_id     : 車両へ発行したID。
 * box          : 直近で検出されたBBox。
 * missed_frames: 連続して検出できなかったFrame数。
 */
typedef struct
{
    int active;
    int track_id;
    float box[4];
    int missed_frames;
} FcwTrackState;

/* IoU Tracker全体の状態と設定。 */
typedef struct
{
    FcwTrackState tracks[FCW_MAX_TRACKS];
    int next_track_id;
    float iou_threshold;
    int max_missed_frames;
} FcwIouTracker;

/*
 * Trackerを初期化する。
 *
 * tracker          : 初期化対象のTracker。
 * iou_threshold    : 同じ車両と判定するIoUの下限。
 * max_missed_frames: この値を超えて未検出のTrackを削除する。
 */
void fcw_iou_tracker_init(
    FcwIouTracker *tracker,
    float iou_threshold,
    int max_missed_frames
);

/*
 * 2つのBBoxのIoUを計算する。
 * boxの要素は、[xmin, ymin, xmax, ymax]の順で指定する。
 */
float fcw_calculate_iou(
    const float box1[4],
    const float box2[4]
);

/*
 * 現在FrameのBBoxへtrack_idを付与し、期限切れTrackを削除する。
 *
 * tracker    : 前FrameまでのTrack状態。関数内で更新される。
 * boxes      : 現在FrameのBBox配列。各要素は[xmin,ymin,xmax,ymax]。
 * num_boxes  : boxesの要素数。
 * track_ids  : 各BBoxに対応するtrack_idの出力配列。
 * best_ious  : 各BBoxと対応した既存Trackとの最大IoUの出力配列。
 *              新規Trackの場合も、探索した最大IoUを保存する。
 * expired_ids: 削除されたTrackのIDを書き込む出力配列。
 * max_expired: expired_idsへ書き込める最大要素数。
 *
 * 戻り値      : 削除されたTrack IDの数。
 *               引数が不正な場合は-1。
 *
 * 既存Trackとの最大IoUが閾値以上ならIDを引き継ぎ、閾値未満なら
 * 新しいIDを発行する。同じ既存Trackは同一Frame内で一度だけ使用する。
 */
int fcw_iou_tracker_update(
    FcwIouTracker *tracker,
    const float boxes[][4],
    int num_boxes,
    int track_ids[],
    float best_ious[],
    int expired_ids[],
    int max_expired
);

#endif /* AVP_FCW_TRACKER_H_ */

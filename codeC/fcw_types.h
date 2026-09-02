#ifndef FCW_TYPES_H_
#define FCW_TYPES_H_

#include <VX/vx.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TIDL の1フレームに含まれ得る最大検出数に合わせる。 */
#define FCW_MAX_DETECTIONS (100U)

/* 0.0〜1.0 の正規化 bbox。 */
typedef struct
{
    vx_float32 ymin;
    vx_float32 xmin;
    vx_float32 ymax;
    vx_float32 xmax;
} FcwNormalizedBox;

/* 入力画像基準のピクセル bbox。 */
typedef struct
{
    vx_int32 x1;
    vx_int32 y1;
    vx_int32 x2;
    vx_int32 y2;
} FcwPixelBox;

/* 入力画像基準のピクセル座標。 */
typedef struct
{
    vx_int32 x;
    vx_int32 y;
} FcwPixelPoint;

/*
 * 現在フレームの車両1台分の結果。
 * detector、ROI、tracker、TTC、alert が同じインスタンスへ結果を追記する。
 */
typedef struct
{
    /* TIDL object detection output */
    FcwNormalizedBox box;
    vx_float32 score;
    vx_int32 class_id;
    vx_int32 channel;

    /* ROI output */
    FcwPixelBox pixel_box;
    FcwPixelPoint bottom_center;
    vx_float32 height_px;
    vx_bool roi_valid;

    /* Tracker output */
    vx_int32 track_id;
    vx_float32 iou;
    vx_bool track_valid;

    /* TTC output */
    vx_float32 dh_dt;
    vx_bool dh_dt_valid;
    vx_float32 ttc_sec;
    vx_bool ttc_valid;
    vx_float32 r_squared;
    vx_bool r_squared_valid;
    vx_uint32 history_length;

    /* Alert output */
    vx_bool alert;
} FcwCar;

/*
 * 1フレーム分の一時出力バッファ。
 * 継続状態は保持しない。次フレームの開始時に reset して再利用する。
 */
typedef struct
{
    vx_int32 frame_index;
    vx_float32 time_s;

    vx_uint32 num_cars;
    vx_uint32 dropped_cars;
    FcwCar cars[FCW_MAX_DETECTIONS];

    vx_bool alarm_active;
} FcwFrameResult;

/* フレーム開始時に呼び、前フレームの一時結果を破棄する。 */
vx_status fcw_frame_result_reset(
    FcwFrameResult *result,
    vx_int32 frame_index,
    vx_float32 time_s
);

/* 空き要素を1つ確保する。成功時に *car を初期化済み要素へ設定する。 */
vx_status fcw_frame_result_add_car(
    FcwFrameResult *result,
    FcwCar **car
);

#ifdef __cplusplus
}
#endif

#endif /* FCW_TYPES_H_ */

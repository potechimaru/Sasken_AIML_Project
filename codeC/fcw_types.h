#ifndef FCW_TYPES_H_
#define FCW_TYPES_H_

/* SSDで検出された車両を、1フレーム分だけ一時保存するための型。 */

#include <VX/vx.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FCW_MAX_DETECTIONS (100U)

/*
 * SSDが出力した車両1台分のデータ。
 * bbox座標の単位（正規化値かピクセル値か）は、入力側で統一してから格納する。
 */
typedef struct
{
    vx_float32 xmin;
    vx_float32 ymin;
    vx_float32 xmax;
    vx_float32 ymax;
    vx_float32 score;
    vx_int32 class_id;
    vx_int32 channel;
} FcwCar;

/* 現在フレームのSSD車両検出結果。次フレーム開始時に再利用する。 */
typedef struct
{
    vx_int32 frame_index;
    vx_uint32 num_cars;
    vx_uint32 dropped_cars;
    FcwCar cars[FCW_MAX_DETECTIONS];
} FcwFrameResult;

/* 前フレームの一時データを消去して、指定フレーム用に初期化する。 */
vx_status fcw_frame_result_reset(FcwFrameResult *result, vx_int32 frame_index);

/* 空き要素を1件確保する。成功時だけ *car に格納先を返す。 */
vx_status fcw_frame_result_add_car(FcwFrameResult *result, FcwCar **car);

#ifdef __cplusplus
}
#endif

#endif /* FCW_TYPES_H_ */

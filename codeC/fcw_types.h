#ifndef FCW_TYPES_H_
#define FCW_TYPES_H_

/* SSD検出結果と、その結果に対するROI/TTCの出力を1フレーム分保存する型。 */

#include <VX/vx.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FCW_MAX_DETECTIONS (100U)

typedef struct
{
    /* main_preが0.0〜1.0へ正規化して格納するSSD bbox。 */
    vx_float32 xmin;
    vx_float32 ymin;
    vx_float32 xmax;
    vx_float32 ymax;
    vx_float32 score;
    vx_int32 class_id;
    vx_int32 channel;

    /* ROIとtracker/TTCの結果。 */
    vx_bool roi_valid;
    vx_int32 track_id;       /* tracker未接続時は -1 */
    vx_uint32 history_length;
    vx_float32 ttc_sec;
    vx_bool ttc_valid;
} FcwCar;

typedef struct
{
    vx_int32 frame_index;
    vx_uint32 num_cars;
    vx_uint32 dropped_cars;
    FcwCar cars[FCW_MAX_DETECTIONS];
} FcwFrameResult;

vx_status fcw_frame_result_reset(FcwFrameResult *result, vx_int32 frame_index);
vx_status fcw_frame_result_add_car(FcwFrameResult *result, FcwCar **car);

#ifdef __cplusplus
}
#endif

#endif /* FCW_TYPES_H_ */

#ifndef FCW_ROI_H_
#define FCW_ROI_H_

#include "fcw_types.h"

/* 正規化座標。ROI判定と画面描画で共通使用する */
// #define FCW_ROI_TOP_Y          (0.62F)
#define FCW_ROI_TOP_Y          (0.63F)
#define FCW_ROI_BOTTOM_Y       (1.00F)

// #define FCW_ROI_TOP_LEFT_X     (0.45F)
#define FCW_ROI_TOP_LEFT_X     (0.45F)
// #define FCW_ROI_TOP_RIGHT_X    (0.55F)
#define FCW_ROI_TOP_RIGHT_X    (0.55F)

// #define FCW_ROI_BOTTOM_LEFT_X  (0.33F)
#define FCW_ROI_BOTTOM_LEFT_X  (0.30F)
// #define FCW_ROI_BOTTOM_RIGHT_X (0.67F)
#define FCW_ROI_BOTTOM_RIGHT_X (0.70F)

vx_bool fcw_roi_check_car(FcwCar *car);

#endif

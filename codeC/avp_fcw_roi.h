#ifndef FCW_ROI_H_
#define FCW_ROI_H_

#include <VX/vx.h>

#include "fcw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 正規化bboxの下辺中心が固定の前方台形ROIに入るか判定する。 */
vx_bool fcw_roi_check_car(FcwCar *car);

#ifdef __cplusplus
}
#endif

#endif /* FCW_ROI_H_ */

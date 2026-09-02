#ifndef FCW_ROI_H_
#define FCW_ROI_H_

#include <VX/vx.h>
#include "avp_fcw_types.h"


#ifdef __cplusplus
extern "C" {
#endif


/* 車両BBoxの下辺中央がROI内か判定し、
   結果をcar->roi_validへ保存する */
vx_bool fcw_roi_check_car(
    FcwCar *car
);


#ifdef __cplusplus
}
#endif

#endif /* FCW_ROI_H_ */
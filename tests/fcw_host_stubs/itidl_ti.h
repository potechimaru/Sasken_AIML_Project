#ifndef FCW_TEST_ITIDL_TI_H
#define FCW_TEST_ITIDL_TI_H

#include "fcw_test_stubs.h"

typedef struct sTIDL_IOBufDesc_t {
    vx_uint32 outWidth[1];
    vx_uint32 outHeight[1];
    vx_uint32 outNumChannels[1];
    vx_uint32 outPadL[1];
    vx_uint32 outPadR[1];
    vx_uint32 outPadT[1];
    vx_uint32 outPadB[1];
} sTIDL_IOBufDesc_t;

typedef struct {
    vx_float32 numDetObjects;
    vx_uint32 objInfoOffset;
    vx_uint32 objInfoSize;
} TIDL_ODLayerHeaderInfo;

typedef struct {
    vx_float32 ymin;
    vx_float32 xmin;
    vx_float32 ymax;
    vx_float32 xmax;
    vx_float32 score;
    vx_int32 label;
} TIDL_ODLayerObjInfo;

#endif

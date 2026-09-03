#ifndef FCW_TIDL_ADAPTER_H_
#define FCW_TIDL_ADAPTER_H_

#include "fcw_types.h"
#include "itidl_ti.h"


/*
 * Convert one channel of a TIDL 2D OD output tensor to FCW detections.
 *
 * The output box uses normalized coordinates in the same order as the
 * Python detector: [ymin, xmin, ymax, xmax].
 */
vx_status fcw_tidl_adapter_extract(
    const sTIDL_IOBufDesc_t *io_buf_desc,
    vx_object_array output_tensor_arr,
    vx_uint32 channel,
    FcwDetection *detections,
    vx_uint32 detection_capacity,
    vx_uint32 *num_detections
);

#endif
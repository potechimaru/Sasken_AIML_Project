#include "fcw_tidl_adapter.h"

vx_status fcw_tidl_adapter_extract(
    const sTIDL_IOBufDesc_t *io_buf_desc,
    vx_object_array output_tensor_arr,
    vx_uint32 channel,
    FcwDetection *detections,
    vx_uint32 detection_capacity,
    vx_uint32 *num_detections
)
{
    (void)io_buf_desc;
    (void)output_tensor_arr;
    (void)channel;
    (void)detections;
    (void)detection_capacity;
    if (num_detections != NULL)
    {
        *num_detections = 0U;
    }
    return VX_ERROR_NOT_SUPPORTED;
}

#include "fcw_tidl_adapter.h"
#include <stddef.h>

/* Host integration-test fixture: represents a front-camera car that grows
 * by one percent of image height on each synthetic video frame. */
static vx_bool simulated_video_enabled = vx_false_e;
static vx_uint32 simulated_frame_index = 0U;

void fcw_test_tidl_adapter_start_simulated_video(void)
{
    simulated_video_enabled = vx_true_e;
    simulated_frame_index = 0U;
}

void fcw_test_tidl_adapter_stop_simulated_video(void)
{
    simulated_video_enabled = vx_false_e;
}

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

    if (simulated_video_enabled == vx_true_e)
    {
        if ((channel == 0U) && (detections != NULL) &&
            (detection_capacity > 0U) && (num_detections != NULL))
        {
            vx_float32 height = 0.10F +
                (0.01F * (vx_float32)simulated_frame_index);

            detections[0].box.xmin = 0.45F;
            detections[0].box.xmax = 0.55F;
            detections[0].box.ymax = 0.90F;
            detections[0].box.ymin = detections[0].box.ymax - height;
            detections[0].score = 0.95F;
            detections[0].class_id = 3;
            detections[0].object_id = (vx_int32)simulated_frame_index;
            detections[0].channel = channel;
            *num_detections = 1U;
        }
        else if (num_detections != NULL)
        {
            *num_detections = 0U;
        }

        /* main.c calls channel 0, 1, then 2 once per input frame. */
        if (channel == 2U)
        {
            simulated_frame_index++;
        }
        return VX_SUCCESS;
    }

    (void)channel;
    (void)detections;
    (void)detection_capacity;
    if (num_detections != NULL)
    {
        *num_detections = 0U;
    }
    return VX_ERROR_NOT_SUPPORTED;
}

#include <stdio.h>

#include "main_pre.h"

static vx_bool alarm_state = vx_false_e;

static void alarm_output(vx_bool active, void *user_data)
{
    (void)user_data;
    alarm_state = active;
}

static FcwDetection detection_for_frame(vx_int32 frame_index)
{
    FcwDetection detection = {0};
    vx_float32 height = 0.10F + 0.01F * (vx_float32)frame_index;

    detection.box.xmin = 0.45F;
    detection.box.xmax = 0.55F;
    detection.box.ymax = 0.90F;
    detection.box.ymin = detection.box.ymax - height;
    detection.score = 0.9F;
    detection.class_id = 1;
    detection.object_id = frame_index;
    detection.channel = 0U;
    return detection;
}

int main(void)
{
    FcwMainPreContext context;
    FcwMainPreConfig config;
    FcwDetection detection;
    const FcwFrameResult *result;
    vx_int32 frame;

    fcw_main_pre_config_set_defaults(&config);
    config.alert_required_count = 1U;
    if (fcw_main_pre_init(&context, &config, alarm_output, NULL) != VX_SUCCESS)
        return 1;

    for (frame = 0; frame < 10; frame++)
    {
        detection = detection_for_frame(frame);
        if (fcw_main_pre_begin_frame(&context, frame) != VX_SUCCESS ||
            fcw_main_pre_add_detection(&context, &detection) != VX_SUCCESS ||
            fcw_main_pre_finalize_frame(&context) != VX_SUCCESS)
            return 1;
    }

    result = fcw_main_pre_get_frame_result(&context);
    if ((result == NULL) || (result->num_cars != 1U) ||
        (result->cars[0].ttc_valid != vx_true_e) ||
        (result->cars[0].alert != vx_true_e) ||
        (alarm_state != vx_true_e))
        return 1;

    printf("main_pre integration test passed.\n");
    fcw_main_pre_deinit(&context);
    return 0;
}

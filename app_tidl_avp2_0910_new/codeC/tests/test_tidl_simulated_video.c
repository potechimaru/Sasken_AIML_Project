#include <stdio.h>

#include "main_pre.h"

void fcw_test_tidl_adapter_start_simulated_video(void);
void fcw_test_tidl_adapter_stop_simulated_video(void);

static vx_bool alarm_state = vx_false_e;

static void alarm_output(vx_bool active, void *user_data)
{
    (void)user_data;
    alarm_state = active;
}

int main(void)
{
    FcwMainPreContext context;
    FcwMainPreConfig config;
    const FcwFrameResult *result;
    vx_int32 frame;

    fcw_main_pre_config_set_defaults(&config);
    config.car_class_id = 3;
    config.fps = 25;
    config.alert_required_count = 1U;

    if (fcw_main_pre_init(&context, &config, alarm_output, NULL) != VX_SUCCESS)
        return 1;

    fcw_test_tidl_adapter_start_simulated_video();
    for (frame = 0; frame < 10; frame++)
    {
        if (fcw_main_pre_process_tidl_frame(
                &context,
                (const sTIDL_IOBufDesc_t *)1,
                (vx_object_array)1,
                3U,
                frame) != VX_SUCCESS)
            return 1;
    }
    fcw_test_tidl_adapter_stop_simulated_video();

    result = fcw_main_pre_get_frame_result(&context);
    if ((result == NULL) || (result->num_cars != 1U) ||
        (result->cars[0].channel != 0U) ||
        (result->cars[0].roi_valid != vx_true_e) ||
        (result->cars[0].ttc_valid != vx_true_e) ||
        (result->cars[0].alert != vx_true_e) ||
        (alarm_state != vx_true_e))
        return 1;

    printf("simulated TIDL video FCW test passed. ttc=%.3f sec\n",
           result->cars[0].ttc_sec);
    fcw_main_pre_deinit(&context);
    return 0;
}

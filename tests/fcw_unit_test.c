#include "fcw_host_stubs/fcw_test_stubs.h"
#include "../app_tidl_avp2/avp_fcw_module.h"

#include <stdio.h>
#include <stdlib.h>

#define TEST_WIDTH  (1280U)
#define TEST_HEIGHT (720U)

static void require_condition(int condition, const char *message)
{
    if(!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static AvpFwcDetection make_detection(vx_float32 ymin,
                                      vx_float32 xmin,
                                      vx_float32 ymax,
                                      vx_float32 xmax,
                                      vx_float32 score,
                                      vx_int32 class_id)
{
    AvpFwcDetection detection = {ymin, xmin, ymax, xmax, score, class_id};
    return detection;
}

static void test_approaching_vehicle(void)
{
    AvpFwcConfig config;
    AvpFwcContext context;
    AvpFwcFrameResult result;
    AvpFwcDetection detection;
    vx_uint32 frame;

    avp_fcw_config_set_defaults(&config);
    require_condition(avp_fcw_init(&context, &config) == VX_SUCCESS,
                      "approaching: context initialization");

    for(frame = 0U; frame < 7U; frame++)
    {
        vx_float32 ymin = 0.80f - (0.01f * (vx_float32)frame);
        detection = make_detection(ymin, 0.45f, 0.90f, 0.55f, 0.95f, 3);
        require_condition(avp_fcw_process(&context, &detection, 1U, frame,
                                           TEST_WIDTH, TEST_HEIGHT, &result) == VX_SUCCESS,
                          "approaching: process status");
        require_condition(result.num_detections == 1U,
                          "approaching: detection accepted");
        require_condition(result.detections[0].track_id == 1U,
                          "approaching: track ID remains stable");
    }

    require_condition(result.detections[0].history_length == 5U,
                      "approaching: rolling history reaches five frames");
    require_condition(result.detections[0].dh_dt > 0.0f,
                      "approaching: height growth is positive");
    require_condition(isfinite(result.detections[0].ttc),
                      "approaching: TTC is finite");
    require_condition(result.detections[0].ttc < config.on_ttc_threshold,
                      "approaching: TTC is below ON threshold");
    require_condition(result.alert_active == vx_true_e,
                      "approaching: alert becomes active");

    printf("PASS: approaching vehicle: TTC=%.3f s, dh/dt=%.3f px/s, track=%u\n",
           result.detections[0].ttc,
           result.detections[0].dh_dt,
           result.detections[0].track_id);
}

static void test_non_approaching_vehicle(void)
{
    AvpFwcConfig config;
    AvpFwcContext context;
    AvpFwcFrameResult result;
    AvpFwcDetection detection = make_detection(0.70f, 0.45f, 0.90f, 0.55f, 0.95f, 3);
    vx_uint32 frame;

    avp_fcw_config_set_defaults(&config);
    require_condition(avp_fcw_init(&context, &config) == VX_SUCCESS,
                      "non-approaching: context initialization");

    for(frame = 0U; frame < 6U; frame++)
    {
        require_condition(avp_fcw_process(&context, &detection, 1U, frame,
                                           TEST_WIDTH, TEST_HEIGHT, &result) == VX_SUCCESS,
                          "non-approaching: process status");
    }

    require_condition(!isfinite(result.detections[0].ttc),
                      "non-approaching: TTC is not reported");
    require_condition(result.alert_active == vx_false_e,
                      "non-approaching: alert remains inactive");
    printf("PASS: non-approaching vehicle: no TTC and no alert\n");
}

static void test_roi_and_confidence_filtering(void)
{
    AvpFwcConfig config;
    AvpFwcContext context;
    AvpFwcFrameResult result;
    AvpFwcDetection detections[3];

    avp_fcw_config_set_defaults(&config);
    require_condition(avp_fcw_init(&context, &config) == VX_SUCCESS,
                      "filtering: context initialization");

    detections[0] = make_detection(0.10f, 0.05f, 0.20f, 0.15f, 0.95f, 3);
    detections[1] = make_detection(0.70f, 0.45f, 0.90f, 0.55f, 0.20f, 3);
    detections[2] = make_detection(0.70f, 0.45f, 0.90f, 0.55f, 0.95f, 1);
    require_condition(avp_fcw_process(&context, detections, 3U, 0U,
                                       TEST_WIDTH, TEST_HEIGHT, &result) == VX_SUCCESS,
                      "filtering: process status");
    require_condition(result.num_detections == 0U,
                      "filtering: ROI, score, and class filters reject detections");
    printf("PASS: ROI, score, and class filtering\n");
}

int main(void)
{
    test_approaching_vehicle();
    test_non_approaching_vehicle();
    test_roi_and_confidence_filtering();
    printf("All FCW unit tests passed.\n");
    return EXIT_SUCCESS;
}

#include <stdio.h>

#include "avp_fcw_roi.h"
#include "fcw_alarm.h"
#include "fcw_alert.h"
#include "fcw_tracker.h"
#include "fcw_ttc.h"
#include "fcw_types.h"

#define CHECK(condition)                                                     \
    do                                                                       \
    {                                                                        \
        if (!(condition))                                                    \
        {                                                                    \
            printf("FAILED: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
            return 1;                                                        \
        }                                                                    \
    } while (0)

static vx_bool g_alarm_output = vx_false_e;

static void test_alarm_output(vx_bool active, void *user_data)
{
    (void)user_data;
    g_alarm_output = active;
}

static FcwBox make_box(vx_float32 xmin, vx_float32 ymin,
                       vx_float32 xmax, vx_float32 ymax)
{
    FcwBox box;
    box.xmin = xmin;
    box.ymin = ymin;
    box.xmax = xmax;
    box.ymax = ymax;
    return box;
}

static int test_frame_result(void)
{
    FcwFrameResult result;
    FcwCar *car;

    CHECK(fcw_frame_result_reset(&result, 12) == VX_SUCCESS);
    CHECK(result.frame_index == 12);
    CHECK(result.num_cars == 0U);
    CHECK(fcw_frame_result_add_car(&result, &car) == VX_SUCCESS);
    CHECK(car != NULL);
    CHECK(car->track_id == -1);
    CHECK(result.num_cars == 1U);
    return 0;
}

static int test_roi(void)
{
    FcwCar car = {0};

    car.box = make_box(0.45F, 0.60F, 0.55F, 0.90F);
    CHECK(fcw_roi_check_car(&car) == vx_true_e);
    car.box = make_box(0.00F, 0.60F, 0.10F, 0.90F);
    CHECK(fcw_roi_check_car(&car) == vx_false_e);
    return 0;
}

static int test_tracker(void)
{
    FcwTrackerContext tracker;
    FcwCar car = {0};
    vx_int32 expired[FCW_MAX_TRACKS];
    vx_uint32 expired_count;
    vx_int32 first_track_id;

    CHECK(fcw_tracker_init(&tracker, 0.30F, 1U) == VX_SUCCESS);
    car.box = make_box(0.40F, 0.50F, 0.60F, 0.90F);
    CHECK(fcw_tracker_update(&tracker, &car, 1U, expired,
                             FCW_MAX_TRACKS, &expired_count) == VX_SUCCESS);
    first_track_id = car.track_id;
    CHECK(first_track_id > 0);
    car.box = make_box(0.41F, 0.50F, 0.61F, 0.90F);
    CHECK(fcw_tracker_update(&tracker, &car, 1U, expired,
                             FCW_MAX_TRACKS, &expired_count) == VX_SUCCESS);
    CHECK(car.track_id == first_track_id);
    return 0;
}

static int test_ttc(void)
{
    fcw_ttc_manager_t manager = {0};
    fcw_ttc_data_t data;
    FcwCar car = {0};
    vx_int32 i;

    for (i = 0; i < 10; i++)
    {
        data.frame_id = i;
        data.track_id = 1;
        data.height = 0.10F + 0.01F * (vx_float32)i;
        CHECK(fcw_ttc_update(&data, &manager) == vx_true_e);
    }
    CHECK(fcw_ttc_get_history_length(&manager, 1) == 10U);
    CHECK(fcw_ttc_calculate(30, &manager, 1, &car) == vx_true_e);
    CHECK(car.ttc_valid == vx_true_e);
    CHECK(car.ttc_sec > 0.0F);
    return 0;
}

static int test_alert_and_alarm(void)
{
    FcwAlertController alert;
    FcwAlarmContext alarm;
    FcwCar car = {0};
    vx_uint32 i;

    CHECK(fcw_alert_init(&alert, 4.0F, 6.5F, 3U) == VX_SUCCESS);
    car.track_id = 1;
    car.ttc_valid = vx_true_e;
    car.ttc_sec = 3.0F;
    for (i = 0U; i < 3U; i++)
    {
        CHECK(fcw_alert_update(&alert, &car) == VX_SUCCESS);
    }
    CHECK(car.alert == vx_true_e);
    CHECK(fcw_alarm_init(&alarm, test_alarm_output, NULL) == VX_SUCCESS);
    CHECK(fcw_alarm_update(&alarm, &car, 1U) == VX_SUCCESS);
    CHECK(g_alarm_output == vx_true_e);
    return 0;
}

int main(void)
{
    CHECK(test_frame_result() == 0);
    CHECK(test_roi() == 0);
    CHECK(test_tracker() == 0);
    CHECK(test_ttc() == 0);
    CHECK(test_alert_and_alarm() == 0);
    printf("All FCW unit tests passed.\n");
    return 0;
}

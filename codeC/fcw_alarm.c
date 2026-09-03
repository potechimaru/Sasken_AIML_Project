#include "fcw_alarm.h"


/*
 * 警報出力を更新する。
 *
 * output_funcにはGPIO、CAN、ブザー制御などを接続する。
 */
vx_status fcw_alarm_set_active(
    FcwAlarmContext *context,
    vx_bool active
)
{
    if (context == NULL)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (context->initialized != vx_true_e)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    /*
     * 状態が変化したときだけハードウェアへ通知する。
     */
    if (context->active != active)
    {
        context->active = active;

        if (context->output_func != NULL)
        {
            context->output_func(
                active,
                context->user_data
            );
        }
    }

    return VX_SUCCESS;
}


/*
 * Alarmを初期化する。
 */
vx_status fcw_alarm_init(
    FcwAlarmContext *context,
    FcwAlarmOutputFunc output_func,
    void *user_data
)
{
    if (context == NULL)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memset(context, 0, sizeof(FcwAlarmContext));

    context->output_func = output_func;
    context->user_data = user_data;
    context->active = vx_false_e;
    context->initialized = vx_true_e;

    return VX_SUCCESS;
}


/*
 * 1台でもAlert中ならAlarmをONにする。
 *
 * Python:
 *   alarm.set_active(any(car["alert"] for car in forward_cars))
 */
vx_status fcw_alarm_update(
    FcwAlarmContext *context,
    const FcwCar *cars,
    vx_uint32 num_cars
)
{
    vx_uint32 i;
    vx_bool any_alert;

    if (context == NULL)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (context->initialized != vx_true_e)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if ((num_cars > 0u) &&
        (cars == NULL))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    any_alert = vx_false_e;

    for (i = 0u; i < num_cars; i++)
    {
        if (cars[i].alert == vx_true_e)
        {
            any_alert = vx_true_e;
            break;
        }
    }

    return fcw_alarm_set_active(
        context,
        any_alert
    );
}


void fcw_alarm_deinit(
    FcwAlarmContext *context
)
{
    if (context == NULL)
    {
        return;
    }

    if ((context->initialized == vx_true_e) &&
        (context->active == vx_true_e) &&
        (context->output_func != NULL))
    {
        context->output_func(
            vx_false_e,
            context->user_data
        );
    }

    memset(context, 0, sizeof(FcwAlarmContext));
}
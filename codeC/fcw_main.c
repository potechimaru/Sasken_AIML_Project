/*
 * 1. Trackerを更新
 */
status = fcw_tracker_update(
    &obj->tracker,
    cars,
    num_cars,
    expired_track_ids,
    &expired_count
);


/*
 * 2. 追跡終了車両のTTC・Alert状態を削除
 */
for (i = 0u; i < expired_count; i++)
{
    fcw_ttc_drop(
        &obj->ttc_controller,
        expired_track_ids[i]
    );

    fcw_alert_drop(
        &obj->alert_controller,
        expired_track_ids[i]
    );
}


/*
 * 3. TTC計算後にAlert判定
 */
for (i = 0u; i < num_cars; i++)
{
    /*
     * この時点でTTCモジュールが
     * cars[i].ttc
     * cars[i].ttc_valid
     * を設定済みであること。
     */
    status = fcw_alert_update(
        &obj->alert_controller,
        &cars[i]
    );

    if (status != VX_SUCCESS)
    {
        printf(
            "fcw_alert_update failed: track_id=%d\n",
            cars[i].track_id
        );
    }
}


/*
 * 4. 1台でもAlert中ならalarmへ通知
 */
any_alert = vx_false_e;

for (i = 0u; i < num_cars; i++)
{
    if (cars[i].alert == vx_true_e)
    {
        any_alert = vx_true_e;
        break;
    }
}

fcw_alarm_set_active(any_alert);
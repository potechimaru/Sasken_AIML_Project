#include "avp_fcw_ttc.h"

#include <string.h>

static fcw_ttc_track_t *fcw_ttc_find_track(fcw_ttc_manager_t *manager,
                                            vx_int32 track_id)
{
    vx_uint32 i;
    if (manager == NULL) return NULL;
    for (i = 0U; i < FCW_TTC_MAX_TRACKS; i++)
    {
        if ((manager->tracks[i].active == vx_true_e) &&
            (manager->tracks[i].track_id == track_id))
        {
            return &manager->tracks[i];
        }
    }
    return NULL;
}

static const fcw_ttc_track_t *fcw_ttc_find_track_const(
    const fcw_ttc_manager_t *manager, vx_int32 track_id)
{
    vx_uint32 i;
    if (manager == NULL) return NULL;
    for (i = 0U; i < FCW_TTC_MAX_TRACKS; i++)
    {
        if ((manager->tracks[i].active == vx_true_e) &&
            (manager->tracks[i].track_id == track_id))
        {
            return &manager->tracks[i];
        }
    }
    return NULL;
}

static fcw_ttc_track_t *fcw_ttc_create_track(fcw_ttc_manager_t *manager,
                                              vx_int32 track_id)
{
    vx_uint32 i;
    if (manager == NULL) return NULL;
    for (i = 0U; i < FCW_TTC_MAX_TRACKS; i++)
    {
        if (manager->tracks[i].active == vx_false_e)
        {
            memset(&manager->tracks[i], 0, sizeof(manager->tracks[i]));
            manager->tracks[i].active = vx_true_e;
            manager->tracks[i].track_id = track_id;
            return &manager->tracks[i];
        }
    }
    return NULL;
}

void fcw_ttc_calculate_height(vx_float32 ymin, vx_float32 ymax,
                              fcw_ttc_data_t *data)
{
    if (data != NULL)
    {
        data->height = ymax - ymin;
    }
}

vx_bool fcw_ttc_update(const fcw_ttc_data_t *data, fcw_ttc_manager_t *manager)
{
    fcw_ttc_track_t *track;
    vx_uint32 i;
    vx_uint32 count;

    if ((data == NULL) || (manager == NULL) || (data->height <= 0.0F))
    {
        return vx_false_e;
    }
    track = fcw_ttc_find_track(manager, data->track_id);
    if (track == NULL)
    {
        track = fcw_ttc_create_track(manager, data->track_id);
    }
    if (track == NULL) return vx_false_e;

    count = track->history.history_count;
    if (count >= FCW_TTC_HISTORY_SIZE) count = FCW_TTC_HISTORY_SIZE - 1U;
    for (i = count; i > 0U; i--)
    {
        track->history.frame_id_history[i] = track->history.frame_id_history[i - 1U];
        track->history.height_history[i] = track->history.height_history[i - 1U];
    }
    track->history.frame_id_history[0] = data->frame_id;
    track->history.height_history[0] = data->height;
    if (track->history.history_count < FCW_TTC_HISTORY_SIZE)
    {
        track->history.history_count++;
    }
    return vx_true_e;
}

vx_uint32 fcw_ttc_get_history_length(const fcw_ttc_manager_t *manager,
                                     vx_int32 track_id)
{
    const fcw_ttc_track_t *track = fcw_ttc_find_track_const(manager, track_id);
    return (track == NULL) ? 0U : track->history.history_count;
}

vx_bool fcw_ttc_calculate(vx_int32 fps, const fcw_ttc_manager_t *manager,
                          vx_int32 track_id, FcwCar *car)
{
    const fcw_ttc_track_t *track;
    vx_float32 time[FCW_TTC_HISTORY_SIZE];
    vx_float32 inverse_height[FCW_TTC_HISTORY_SIZE];
    vx_float32 mean_time = 0.0F, mean_inverse_height = 0.0F;
    vx_float32 s_tt = 0.0F, s_th = 0.0F, slope, intercept;
    vx_float32 ss_res = 0.0F, ss_tot = 0.0F, r_squared;
    vx_uint32 i;

    if (car != NULL) car->ttc_valid = vx_false_e;
    if ((manager == NULL) || (car == NULL) || (fps <= 0)) return vx_false_e;
    track = fcw_ttc_find_track_const(manager, track_id);
    if ((track == NULL) || (track->history.history_count < FCW_TTC_HISTORY_SIZE))
    {
        return vx_false_e;
    }

    for (i = 0U; i < FCW_TTC_HISTORY_SIZE; i++)
    {
        if (track->history.height_history[i] <= 0.0F) return vx_false_e;
        time[i] = (vx_float32)(track->history.frame_id_history[i] -
                  track->history.frame_id_history[FCW_TTC_HISTORY_SIZE - 1U]) /
                  (vx_float32)fps;
        inverse_height[i] = 1.0F / track->history.height_history[i];
        mean_time += time[i];
        mean_inverse_height += inverse_height[i];
    }
    mean_time /= (vx_float32)FCW_TTC_HISTORY_SIZE;
    mean_inverse_height /= (vx_float32)FCW_TTC_HISTORY_SIZE;
    for (i = 0U; i < FCW_TTC_HISTORY_SIZE; i++)
    {
        s_tt += (time[i] - mean_time) * (time[i] - mean_time);
        s_th += (time[i] - mean_time) * (inverse_height[i] - mean_inverse_height);
    }
    if (s_tt <= 0.0F) return vx_false_e;
    slope = s_th / s_tt;
    intercept = mean_inverse_height - slope * mean_time;
    for (i = 0U; i < FCW_TTC_HISTORY_SIZE; i++)
    {
        vx_float32 predicted = slope * time[i] + intercept;
        vx_float32 residual = inverse_height[i] - predicted;
        vx_float32 from_mean = inverse_height[i] - mean_inverse_height;
        ss_res += residual * residual;
        ss_tot += from_mean * from_mean;
    }
    if (ss_tot <= 0.0F) return vx_false_e;
    r_squared = 1.0F - (ss_res / ss_tot);
    if ((r_squared < FCW_TTC_MIN_R_SQUARED) || (slope >= 0.0F))
    {
        return vx_false_e;
    }
    car->ttc_sec = -inverse_height[0] / slope;
    car->ttc_valid = vx_true_e;
    return vx_true_e;
}

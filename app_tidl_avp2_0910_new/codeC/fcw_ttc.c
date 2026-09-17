#include "fcw_ttc.h"
#include <string.h>

static fcw_ttc_track_t *find_track(fcw_ttc_manager_t *m, vx_int32 id) {
    vx_uint32 i;
    if (!m)
        return NULL;
    for (i = 0U; i < FCW_TTC_MAX_TRACKS; i++)
        if (m->tracks[i].active && m->tracks[i].track_id == id)
            return &m->tracks[i];
    return NULL;
}
static const fcw_ttc_track_t *find_track_const(const fcw_ttc_manager_t *m, vx_int32 id) {
    vx_uint32 i;
    if (!m)
        return NULL;
    for (i = 0U; i < FCW_TTC_MAX_TRACKS; i++)
        if (m->tracks[i].active && m->tracks[i].track_id == id)
            return &m->tracks[i];
    return NULL;
}
static fcw_ttc_track_t *create_track(fcw_ttc_manager_t *m, vx_int32 id) {
    vx_uint32 i;
    if (!m)
        return NULL;
    for (i = 0U; i < FCW_TTC_MAX_TRACKS; i++)
        if (!m->tracks[i].active) {
            memset(&m->tracks[i], 0, sizeof(m->tracks[i]));
            m->tracks[i].active = vx_true_e;
            m->tracks[i].track_id = id;
            return &m->tracks[i];
        }
    return NULL;
}
void fcw_ttc_calculate_height(vx_float32 ymin, vx_float32 ymax, fcw_ttc_data_t *d) {
    if (d)
        d->height = ymax - ymin;
}
vx_bool fcw_ttc_update(const fcw_ttc_data_t *d, fcw_ttc_manager_t *m) {
    fcw_ttc_track_t *t;
    vx_uint32 i, n;
    if (!d || !m || d->height <= 0.0F)
        return vx_false_e;
    t = find_track(m, d->track_id);
    if (!t)
        t = create_track(m, d->track_id);
    if (!t)
        return vx_false_e;
    n = t->history.count;
    if (n >= FCW_TTC_HISTORY_SIZE)
        n = FCW_TTC_HISTORY_SIZE - 1U;
    for (i = n; i > 0U; i--) {
        t->history.frame_id[i] = t->history.frame_id[i - 1U];
        t->history.height[i] = t->history.height[i - 1U];
    }
    t->history.frame_id[0] = d->frame_id;
    t->history.height[0] = d->height;
    if (t->history.count < FCW_TTC_HISTORY_SIZE)
        t->history.count++;
    return vx_true_e;
}
vx_uint32 fcw_ttc_get_history_length(const fcw_ttc_manager_t *m, vx_int32 id) {
    const fcw_ttc_track_t *t = find_track_const(m, id);
    return t ? t->history.count : 0U;
}
void fcw_ttc_drop(fcw_ttc_manager_t *m, vx_int32 id) {
    fcw_ttc_track_t *t = find_track(m, id);
    if (t)
        memset(t, 0, sizeof(*t));
}
vx_bool fcw_ttc_calculate(vx_int32 fps, const fcw_ttc_manager_t *m, vx_int32 id, FcwCar *c) {
    const fcw_ttc_track_t *t;
    vx_float32 time[FCW_TTC_HISTORY_SIZE];
    vx_float32 height[FCW_TTC_HISTORY_SIZE];
    vx_float32 mean_time = 0.0F;
    vx_float32 mean_height = 0.0F;
    vx_float32 time_variance = 0.0F;
    vx_float32 covariance = 0.0F;
    vx_float32 slope;
    vx_float32 intercept;
    vx_float32 residual = 0.0F;
    vx_float32 total = 0.0F;
    vx_float32 r2;
    vx_uint32 i;

    if (c)
        c->ttc_valid = vx_false_e;
    if (!m || !c || fps <= 0)
        return vx_false_e;
    t = find_track_const(m, id);
    if (!t || t->history.count < FCW_TTC_HISTORY_SIZE)
        return vx_false_e;

    for (i = 0U; i < FCW_TTC_HISTORY_SIZE; i++) {
        if (t->history.height[i] <= 0.0F)
            return vx_false_e;

        time[i] =
            (vx_float32)(t->history.frame_id[i] - t->history.frame_id[FCW_TTC_HISTORY_SIZE - 1U]) /
            (vx_float32)fps;
        height[i] = t->history.height[i];
        mean_time += time[i];
        mean_height += height[i];
    }

    mean_time /= (vx_float32)FCW_TTC_HISTORY_SIZE;
    mean_height /= (vx_float32)FCW_TTC_HISTORY_SIZE;

    for (i = 0U; i < FCW_TTC_HISTORY_SIZE; i++) {
        time_variance += (time[i] - mean_time) * (time[i] - mean_time);
        covariance += (time[i] - mean_time) * (height[i] - mean_height);
    }

    if (time_variance <= 0.0F)
        return vx_false_e;

    slope = covariance / time_variance;
    intercept = mean_height - slope * mean_time;

    for (i = 0U; i < FCW_TTC_HISTORY_SIZE; i++) {
        vx_float32 error = height[i] - (slope * time[i] + intercept);
        vx_float32 deviation = height[i] - mean_height;
        residual += error * error;
        total += deviation * deviation;
    }

    if (total <= 0.0F)
        return vx_false_e;

    r2 = 1.0F - residual / total;
    if (r2 < FCW_TTC_MIN_R_SQUARED || slope <= 0.0F)
        return vx_false_e;

    /* Height-domain expansion TTC: TTC = current height / dh/dt. */
    c->ttc_sec = height[0] / slope;
    c->ttc_valid = vx_true_e;
    return vx_true_e;
}

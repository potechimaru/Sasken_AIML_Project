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
    vx_float32 time[FCW_TTC_HISTORY_SIZE], inv[FCW_TTC_HISTORY_SIZE], mt = 0.0F, mh = 0.0F, st = 0.0F, sh = 0.0F, slope, intercept,
                                  res = 0.0F, total = 0.0F, r2;
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
        inv[i] = 1.0F / t->history.height[i];
        mt += time[i];
        mh += inv[i];
    }
    mt /= (vx_float32)FCW_TTC_HISTORY_SIZE;
    mh /= (vx_float32)FCW_TTC_HISTORY_SIZE;
    for (i = 0U; i < FCW_TTC_HISTORY_SIZE; i++) {
        st += (time[i] - mt) * (time[i] - mt);
        sh += (time[i] - mt) * (inv[i] - mh);
    }
    if (st <= 0.0F)
        return vx_false_e;
    slope = sh / st;
    intercept = mh - slope * mt;
    for (i = 0U; i < FCW_TTC_HISTORY_SIZE; i++) {
        vx_float32 e = inv[i] - (slope * time[i] + intercept), d = inv[i] - mh;
        res += e * e;
        total += d * d;
    }
    if (total <= 0.0F)
        return vx_false_e;
    r2 = 1.0F - res / total;
    if (r2 < FCW_TTC_MIN_R_SQUARED || slope >= 0.0F)
        return vx_false_e;
    c->ttc_sec = -inv[0] / slope;
    c->ttc_valid = vx_true_e;
    return vx_true_e;
}

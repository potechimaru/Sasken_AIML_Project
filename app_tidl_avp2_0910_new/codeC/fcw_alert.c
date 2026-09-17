#include "fcw_alert.h"
#include <string.h>

static FcwAlertTrackState *find_state(FcwAlertController *c, vx_int32 id, vx_bool create) {
    vx_uint32 i;
    vx_int32 free_slot = -1;
    for (i = 0U; i < FCW_ALERT_MAX_TRACKS; i++) {
        FcwAlertTrackState *s = &c->states[i];
        if (s->in_use && s->track_id == id)
            return s;
        if (!s->in_use && free_slot < 0)
            free_slot = (vx_int32)i;
    }
    if (!create || free_slot < 0)
        return NULL;
    c->num_states++;
    memset(&c->states[free_slot], 0, sizeof(c->states[free_slot]));
    c->states[free_slot].in_use = vx_true_e;
    c->states[free_slot].track_id = id;
    return &c->states[free_slot];
}
vx_status fcw_alert_init(FcwAlertController *c, vx_float32 on, vx_float32 off, vx_uint32 count) {
    if (!c || on >= off || count == 0U)
        return VX_ERROR_INVALID_PARAMETERS;
    memset(c, 0, sizeof(*c));
    c->on_threshold = on;
    c->off_threshold = off;
    c->required_count = count;
    c->initialized = vx_true_e;
    return VX_SUCCESS;
}
vx_status fcw_alert_reset(FcwAlertController *c) {
    if (!c || !c->initialized)
        return VX_ERROR_INVALID_PARAMETERS;
    memset(c->states, 0, sizeof(c->states));
    c->num_states = 0U;
    return VX_SUCCESS;
}
vx_status fcw_alert_update(FcwAlertController *c, FcwCar *car) {
    FcwAlertTrackState *s;
    if (!c || !car || !c->initialized)
        return VX_ERROR_INVALID_PARAMETERS;
    car->alert = vx_false_e;
    if (car->track_id < 0)
        return VX_SUCCESS;
    s = find_state(c, car->track_id, vx_true_e);
    if (!s)
        return VX_FAILURE;
    if (s->alert_status) {
        if (!car->ttc_valid || car->ttc_sec >= c->off_threshold) {
            s->alert_status = vx_false_e;
            s->consecutive_danger_count = 0U;
        }
    } else if (car->ttc_valid && car->ttc_sec <= c->on_threshold) {
        if (s->consecutive_danger_count < c->required_count)
            s->consecutive_danger_count++;
        if (s->consecutive_danger_count >= c->required_count)
            s->alert_status = vx_true_e;
    } else {
        s->consecutive_danger_count = 0U;
    }
    car->alert = s->alert_status;
    return VX_SUCCESS;
}
vx_status fcw_alert_drop(FcwAlertController *c, vx_int32 id) {
    FcwAlertTrackState *s;
    if (!c || !c->initialized)
        return VX_ERROR_INVALID_PARAMETERS;
    s = find_state(c, id, vx_false_e);
    if (!s)
        return VX_SUCCESS;
    memset(s, 0, sizeof(*s));
    if (c->num_states)
        c->num_states--;
    return VX_SUCCESS;
}
void fcw_alert_deinit(FcwAlertController *c) {
    if (c)
        memset(c, 0, sizeof(*c));
}

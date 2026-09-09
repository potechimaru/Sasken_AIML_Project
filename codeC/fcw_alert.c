#include "fcw_alert.h"
#include <string.h>

<<<<<<< HEAD
#include <string.h>

static FcwAlertTrackState *find_state(
    FcwAlertController *controller,
    vx_int32 track_id,
    vx_bool create)
{
    vx_uint32 i;
    vx_int32 free = -1;

    FcwAlertTrackState *state = NULL;

    for(i=0u; i<FCW_ALERT_MAX_TRACKS; i++){
        state = &controller->states[i];

        if(state->in_use == vx_true_e){
            if(state->track_id == track_id){
                return state;
            }
        }
        else{
            if(free < 0){
                free = (vx_int32)i;
            }
        }
    }
    if(create == vx_false_e){
        return NULL;
    }
    if(free<0){
        return NULL;
    }
    state = &controller->states[free_index];

    memset(state, 0, sizeof(FcwAlertTrackState));

    state->in_use = vx_true_e;
    state->track_id = track_id;
    state->alert_status = vx_false_e;
    state->consecutive_danger_count = 0u;

    controller->num_states++;

    return state;
}

vx_status fcw_alert_init(
    FcwAlertController *controller,
    vx_float32 on_threshold,
    vx_float32 off_threshold,
    vx_uint32 required_count)
{
    if(controller == NULL)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }
    
    if(on_threshold >= off_threshold)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (required_count == 0U)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memset(controller, 0, sizeof(*controller));

    controller->on_threshold = on_threshold;
    controller->off_threshold = off_threshold;
    controller->required_count = required_count;
    controller->num_states = 0u;
    controller->initialized = vx_true_e;

    return VX_SUCCESS;
}

vx_status fcw_alert_update(
    FcwAlertController *controller,
    FcwCar *car)
{
    if((controller==NULL) || (car==NULL)){
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if(controller->initialized != vx_true_e){
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (car->track_id < 0){
        car->alert = vx_false_e;
        return VX_ERROR_INVALID_PARAMETERS;
    }

    car->alert = vx_false_e;

    state = fcw_alert_find_state(controller, car->track_id, vx_true_e);

    if(state == NULL){
        return VX_FAILURE;
    }

    if(state->alert_status == vx_true_e){
        if((car->ttc_valid != vx_true_e) || (car->ttc >= controller->off_threshold)){
            state->alert_status = vx_false_e;
            state->consecutive_danger_count = 0u;
        }
        else{
            state->alert_status = vx_true_e;
        }
    } else {
        if ((car->ttc_valid == vx_true_e) && (car->ttc <= controller->on_threshold)){
            if (state->consecutive_danger_count < controller->required_count){
                state->consecutive_danger_count++;
            }
            if (state->consecutive_danger_count >= controller->required_count){
                state->alert_status = vx_true_e;
            } else {
                state->alert_status = vx_false_e;
            }
        } else {
            state->consecutive_danger_count = 0u;
            state->alert_status = vx_false_e;
        }
    }

    car->alert = state->alert_status;

    return VX_SUCCESS;
}

vx_status fcw_alert_drop(
    FcwAlertController *controller,
    vx_int32 track_id
)
{
    FcwAlertTrackState *state;


    if (controller == NULL){
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (controller->initialized != vx_true_e){
        return VX_ERROR_INVALID_PARAMETERS;
    }

    state = fcw_alert_find_state(
        controller,
        track_id,
        vx_false_e
    );

    if (state == NULL){
        return VX_SUCCESS;
    }

    memset(state, 0, sizeof(FcwAlertTrackState));

    if (controller->num_states > 0u){
        controller->num_states--;
    }

    return VX_SUCCESS;
}

void fcw_alert_deinit(
    FcwAlertController *controller
)
{
    if (controller == NULL){
        return;
    }

    memset(controller, 0, sizeof(FcwAlertController));
}
=======
static FcwAlertTrackState *find_state(FcwAlertController*c,vx_int32 id,vx_bool create)
{vx_uint32 i;vx_int32 free_slot=-1;for(i=0U;i<FCW_ALERT_MAX_TRACKS;i++){FcwAlertTrackState*s=&c->states[i];if(s->in_use&&s->track_id==id)return s;if(!s->in_use&&free_slot<0)free_slot=(vx_int32)i;}if(!create||free_slot<0)return NULL;c->num_states++;memset(&c->states[free_slot],0,sizeof(c->states[free_slot]));c->states[free_slot].in_use=vx_true_e;c->states[free_slot].track_id=id;return &c->states[free_slot];}
vx_status fcw_alert_init(FcwAlertController*c,vx_float32 on,vx_float32 off,vx_uint32 count){if(!c||on>=off||count==0U)return VX_ERROR_INVALID_PARAMETERS;memset(c,0,sizeof(*c));c->on_threshold=on;c->off_threshold=off;c->required_count=count;c->initialized=vx_true_e;return VX_SUCCESS;}
vx_status fcw_alert_reset(FcwAlertController*c){if(!c||!c->initialized)return VX_ERROR_INVALID_PARAMETERS;memset(c->states,0,sizeof(c->states));c->num_states=0U;return VX_SUCCESS;}
vx_status fcw_alert_update(FcwAlertController*c,FcwCar*car){FcwAlertTrackState*s;if(!c||!car||!c->initialized)return VX_ERROR_INVALID_PARAMETERS;car->alert=vx_false_e;if(car->track_id<0)return VX_SUCCESS;s=find_state(c,car->track_id,vx_true_e);if(!s)return VX_FAILURE;if(s->alert_status){if(!car->ttc_valid||car->ttc_sec>=c->off_threshold){s->alert_status=vx_false_e;s->consecutive_danger_count=0U;}}else if(car->ttc_valid&&car->ttc_sec<=c->on_threshold){if(s->consecutive_danger_count<c->required_count)s->consecutive_danger_count++;if(s->consecutive_danger_count>=c->required_count)s->alert_status=vx_true_e;}else{s->consecutive_danger_count=0U;}car->alert=s->alert_status;return VX_SUCCESS;}
vx_status fcw_alert_drop(FcwAlertController*c,vx_int32 id){FcwAlertTrackState*s;if(!c||!c->initialized)return VX_ERROR_INVALID_PARAMETERS;s=find_state(c,id,vx_false_e);if(!s)return VX_SUCCESS;memset(s,0,sizeof(*s));if(c->num_states)c->num_states--;return VX_SUCCESS;}
void fcw_alert_deinit(FcwAlertController*c){if(c)memset(c,0,sizeof(*c));}
>>>>>>> main

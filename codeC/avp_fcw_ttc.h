#ifndef FCW_TTC_H_
#define FCW_TTC_H_

#include <stdbool.h>

#include <VX/vx.h>

#include "fcw_types.h"

#define FCW_TTC_HISTORY_SIZE (10U)
#define FCW_TTC_MAX_TRACKS (20U)
#define FCW_TTC_MIN_R_SQUARED (0.80F)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    vx_int32 frame_id;
    vx_int32 track_id;
    vx_float32 height;
} fcw_ttc_data_t;

typedef struct
{
    vx_int32 frame_id_history[FCW_TTC_HISTORY_SIZE];
    vx_float32 height_history[FCW_TTC_HISTORY_SIZE];
    vx_uint32 history_count;
} fcw_ttc_history_t;

typedef struct
{
    vx_bool active;
    vx_int32 track_id;
    fcw_ttc_history_t history;
} fcw_ttc_track_t;

typedef struct
{
    fcw_ttc_track_t tracks[FCW_TTC_MAX_TRACKS];
} fcw_ttc_manager_t;

void fcw_ttc_calculate_height(vx_float32 ymin, vx_float32 ymax,
                              fcw_ttc_data_t *data);
vx_bool fcw_ttc_update(const fcw_ttc_data_t *data, fcw_ttc_manager_t *manager);
vx_uint32 fcw_ttc_get_history_length(const fcw_ttc_manager_t *manager,
                                     vx_int32 track_id);
vx_bool fcw_ttc_calculate(vx_int32 fps, const fcw_ttc_manager_t *manager,
                          vx_int32 track_id, FcwCar *car);

#ifdef __cplusplus
}
#endif

#endif /* FCW_TTC_H_ */

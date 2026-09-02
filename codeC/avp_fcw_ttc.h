#ifndef FCW_TTC_H
#define FCW_TTC_H

#include <stdbool.h>

#include <VX/vx.h>

#define FCW_TTC_HISTORY_SIZE 10
#define FCW_TTC_MAX_TRACKS 20

typedef struct {
    vx_int32 frame_id;
    vx_int32 track_id;

    // vx_float32 center_x;
    // vx_float32 center_y;
    vx_float32 height;
} fcw_ttc_data_t;

typedef struct {
    vx_int32 frame_id_history[FCW_TTC_HISTORY_SIZE];
    vx_int32 track_id_history[FCW_TTC_HISTORY_SIZE];
    vx_float32 array_height[FCW_TTC_HISTORY_SIZE];

    int history_count;

} fcw_ttc_array_data_t;

/*
 * TTC履歴をtrack_idごとに保持する管理構造体。
 * avp_fcw_ttc.c の ttc_update() / calculate_ttc() が使用する。
 */
typedef struct {
    bool active;
    vx_int32 track_id;
    fcw_ttc_array_data_t history;
} fcw_ttc_track_t;

typedef struct {
    fcw_ttc_track_t tracks[FCW_TTC_MAX_TRACKS];
} fcw_ttc_manager_t;

void calculate_height(
    vx_float32 y_min,
    vx_float32 y_max,
    fcw_ttc_data_t *ttc_data
);

bool ttc_update(
    const fcw_ttc_data_t *ttc_data,
    fcw_ttc_manager_t *manager
);

bool calculate_ttc(
    vx_int32 fps,
    const fcw_ttc_manager_t *manager,
    vx_int32 track_id,
    vx_float32 *ttc
);

#endif /* FCW_TTC_H */

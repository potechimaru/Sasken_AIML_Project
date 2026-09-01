#ifndef FCW_TTC_H
#define FCW_TTC_H

#include <stdbool.h>

#define FCW_TTC_HISTORY_SIZE 10

/*SSDなどで取得したデータを入れる*/
typedef struct {
    vx_int32 frame_id;
    vx_int32 track_id;

    vx_float32 center_x;
    vx_float32 center_y;
    vx_float32 height;

} fcw_ttc_data_t;

/*取得したデータを履歴としてリストに格納する*/
typedef struct {
    vx_int32 frame_id_history[FCW_TTC_HISTORY_SIZE];
    vx_int32 track_id_history[FCW_TTC_HISTORY_SIZE];

    vx_float32 array_center_x[FCW_TTC_HISTORY_SIZE];
    vx_float32 array_center_y[FCW_TTC_HISTORY_SIZE];
    vx_float32 array_height[FCW_TTC_HISTORY_SIZE];

    int history_count;

} fcw_ttc_array_data_t;


void calculate_center_height(
    vx_float32 x_mini,
    vx_float32 x_max,
    vx_float32 y_mini,
    vx_float32 y_max,
    fcw_ttc_data_t *ttc_data
);


bool ttc_update(
    const fcw_ttc_data_t *ttc_data,
    fcw_ttc_array_data_t *ttc_array_data
);


bool calculate_ttc(
    vx_int32 fps,
    const fcw_ttc_array_data_t *ttc_array_data,
    vx_float32 *ttc
);

#endif
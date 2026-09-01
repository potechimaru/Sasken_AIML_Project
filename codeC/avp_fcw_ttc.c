#include "fcw_ttc.h"
#include <stddef.h>

/*bboxの座標と高さを計算する*/
void calculate_center_height(
    vx_float32 x_mini, 
    vx_float32 x_max, 
    vx_float32 y_mini, 
    vx_float32 y_max, 
    fcw_ttc_data_t *ttc_data
){
    // ttc_data -> center_x = (x_mini + x_max) / 2.0;
    // ttc_data -> center_y = (y_mini + y_max) / 2.0;
    ttc_data -> height = y_max - y_mini;
}

/*最新のデータを入れ、最古のデータを削除する*/
static void append_ttc_data_float(
    vx_float32 array[],
    vx_float32 value,
    const fcw_ttc_array_data_t *ttc_array_data
)
{
    int last_index = ttc_array_data->history_count;

    if (last_index >= FCW_TTC_HISTORY_SIZE)
    {
        last_index = FCW_TTC_HISTORY_SIZE - 1;
    }

    for (int i = last_index; i > 0; i--)
    {
        array[i] = array[i - 1];
    }

    array[0] = value;
}

static void append_ttc_data_int(
    vx_int32 array[],
    vx_int32 value, 
    const fcw_ttc_array_data_t *ttc_array_data
){
    int last_index = ttc_array_data->history_count;

    if (last_index >= FCW_TTC_HISTORY_SIZE)
    {
        last_index = FCW_TTC_HISTORY_SIZE - 1;
    }

    for (int i = last_index; i > 0; i--)
    {
        array[i] = array[i - 1];
    }
    array[0] = value;
}

/*main内で呼び出し、dataを最新にする*/
bool ttc_update(
    const fcw_ttc_data_t *ttc_data,
    fcw_ttc_array_data_t *ttc_array_data
)
{
    append_ttc_data_int(
        ttc_array_data->frame_id_history,
        ttc_data->frame_id,
        ttc_array_data
    );

    append_ttc_data_int(
        ttc_array_data->track_id_history,
        ttc_data->track_id,
        ttc_array_data
    );


    append_ttc_data_float(
        ttc_array_data->array_height,
        ttc_data->height,
        ttc_array_data
    );

    if (ttc_array_data->history_count < FCW_TTC_HISTORY_SIZE)
    {
        ttc_array_data->history_count++;
    }

    return true;
}
/*ttcを計算する。返り値はbool main内で宣言した*tccにttcが代入される*/
bool calculate_ttc(
    vx_int32 fps,
    const fcw_ttc_array_data_t *ttc_array_data,
    vx_float32 *ttc
)
{   
    /*dataやmain内にttcがない、dataが10フレーム分ない、fpsが0以下false*/
    if(ttc_array_data == NILL || ttc == NULL){
        retrun false;
    }

    if (ttc_array_data->history_count < FCW_TTC_HISTORY_SIZE)
    {
        return false;
    }

    if (fps <= 0)
    {
        return false;
    }

    /*それぞれの時間と高さ、高さは1/hで計算*/
    vx_float32 times[FCW_TTC_HISTORY_SIZE];
    vx_float32 reverse_array_h[FCW_TTC_HISTORY_SIZE];

    vx_float32 average_t = 0.0f;
    vx_float32 average_h = 0.0f;

    vx_float32 S_tt = 0.0f;
    vx_float32 S_th = 0.0f;

    vx_float32 d_inv_h_dt = 0.0f;
    vx_float32 intercept = 0.0f;

    vx_float32 r_squared = 0.0f;
    vx_float32 SS_res = 0.0f;
    vx_float32 SS_tot = 0.0f;


    for (int i = 0; i < FCW_TTC_HISTORY_SIZE; i++)
    {
        if (ttc_array_data->array_height[i] <= 0.0f)
        {
            return false;
        }

        reverse_array_h[i] =
            1.0f / ttc_array_data->array_height[i];
    }

    /*フレームを時間に変換*/
    for (int i = 0; i < FCW_TTC_HISTORY_SIZE; i++)
    {
        times[i] =
            (ttc_array_data->frame_id_history[i]
             - ttc_array_data->frame_id_history[
                 FCW_TTC_HISTORY_SIZE - 1
             ])
            / (vx_float32)fps;
    }
/*平均時間・高さを計算*/
    for (int i = 0; i < FCW_TTC_HISTORY_SIZE; i++)
    {
        average_t += times[i];
        average_h += reverse_array_h[i];
    }

    average_t /= FCW_TTC_HISTORY_SIZE;
    average_h /= FCW_TTC_HISTORY_SIZE;


    /*線形回帰計算*/
    for (int i = 0; i < FCW_TTC_HISTORY_SIZE; i++)
    {
        S_tt +=
            (times[i] - average_t)
            * (times[i] - average_t);

        S_th +=
            (times[i] - average_t)
            * (reverse_array_h[i] - average_h);
    }

    if (S_tt == 0.0f)
    {
        return false;
    }

    d_inv_h_dt = S_th / S_tt;

    intercept =
        average_h - d_inv_h_dt * average_t;


    /*決定係数用にS_resとS_totを計算*/
    for (int i = 0; i < FCW_TTC_HISTORY_SIZE; i++)
    {
        vx_float32 predicted_h =
            d_inv_h_dt * times[i] + intercept;

        vx_float32 residual =
            reverse_array_h[i] - predicted_h;

        vx_float32 diff_h =
            reverse_array_h[i] - average_h;

        SS_res += residual * residual;
        SS_tot += diff_h * diff_h;
    }

    if (SS_tot == 0.0f)
    {
        return false;
    }

    r_squared =
        1.0f - (SS_res / SS_tot);

    /*決定係数が0.8未満のときfalse*/
    if (r_squared < 0.8f)
    {
        return false;
    }

    if (d_inv_h_dt >= 0.0f)
    {
        return false;
    }

    *ttc =
        -reverse_array_h[0] / d_inv_h_dt;

    return true;
}
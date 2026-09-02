#include "avp_fcw_roi.h"
#include <stddef.h>


/* ROIの4頂点を保存する構造体 */
typedef struct
{
    vx_float32 top_left_x;
    vx_float32 top_left_y;

    vx_float32 top_right_x;
    vx_float32 top_right_y;

    vx_float32 bottom_left_x;
    vx_float32 bottom_left_y;

    vx_float32 bottom_right_x;
    vx_float32 bottom_right_y;

} FcwRoi;


/* 正規化座標で固定ROIを定義する */
/* 上辺より下辺を広くする */
static const FcwRoi roi =
{
    0.40f, 0.50f,
    0.60f, 0.50f,

    0.20f, 1.00f,
    0.80f, 1.00f
};


/* bboxの下辺中央を計算する */
static void calculate_bottom_center(
    const FcwCar *car,
    vx_float32 *center_x,
    vx_float32 *center_y
)
{
    *center_x =
        (car->xmin + car->xmax) / 2.0f;

    *center_y =
        car->ymax;
}


/* 下辺中央がROI内か判定する */
static vx_bool check_point_in_roi(
    vx_float32 center_x,
    vx_float32 center_y
)
{
    vx_float32 ratio;
    vx_float32 left_x;
    vx_float32 right_x;

    /* y座標のROI判定 */
    if (center_y < roi.top_left_y ||
        center_y > roi.bottom_left_y)
    {
        return vx_false_e;
    }

    /* ROI上端から下端までの割合を計算する */
    ratio =
        (center_y - roi.top_left_y)
        / (roi.bottom_left_y - roi.top_left_y);

    /* 現在のy座標におけるROI左端を計算 */
    left_x =
        roi.top_left_x
        + ratio
        * (roi.bottom_left_x - roi.top_left_x);

    /* 現在のy座標におけるROI右端を計算 */
    right_x =
        roi.top_right_x
        + ratio
        * (roi.bottom_right_x - roi.top_right_x);

    /* x座標のROI判定 */
    if (center_x >= left_x &&
        center_x <= right_x)
    {
        return vx_true_e;
    }

    return vx_false_e;
}


/* 車両BBoxの下辺中央がROI内か判定し、
   結果をcar->roi_validへ保存する */
vx_bool fcw_roi_check_car(
    FcwCar *car
)
{
    vx_float32 center_x;
    vx_float32 center_y;

    if (car == NULL)
    {
        return vx_false_e;
    }

    calculate_bottom_center(
        car,
        &center_x,
        &center_y
    );

    car->roi_valid =
        check_point_in_roi(
            center_x,
            center_y
        );

    return car->roi_valid;
}
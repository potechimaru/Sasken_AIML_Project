#ifndef FCW_ROI_H
#define FCW_ROI_H

typedef struct
{
    vx_f;loat32 x;
    vx_float32 y
}fcw_roi_point_t;

typedef struct
{
fcw_roi_point_t top_left;
fcw_roi_point_t top_right;
fcw_roi_point_t bottom_left;
fcw_roi_point_t bottom_right;
}fcw_roi_t;


void fcw_roi_init(
    vx_float32 width,
    vx_int32 height,
    fcw_roi_t *roi
);





#endif
#ifndef AVP_FCW_OVERLAY_MODULE_H_
#define AVP_FCW_OVERLAY_MODULE_H_

#include "avp_common.h"
#include "../../codeC/main_pre.h"

/* OpenVXノードの引数番号 */
enum
{
    FCW_OVERLAY_CONFIG = 0,
    FCW_OVERLAY_FRAME_ID,
    FCW_OVERLAY_TENSOR,
    FCW_OVERLAY_INPUT_IMAGE,
    FCW_OVERLAY_OUTPUT_IMAGE,
    FCW_OVERLAY_NUM_PARAMS
};

typedef struct
{
    sTIDL_IOBufDesc_t io_buf_desc;
    FcwMainPreConfig fcw_config;

    vx_uint32 width;
    vx_uint32 height;
    vx_uint32 line_width;

    /* NV12用の色。RGBではない */
    vx_uint8 red_y;
    vx_uint8 red_u;
    vx_uint8 red_v;
} FcwOverlayConfig;

typedef struct
{
    vx_kernel kernel;
    vx_node node;

    vx_user_data_object config;
    vx_scalar frame_id[APP_MAX_BUFQ_DEPTH];

    vx_object_array output_image_arr;

    vx_uint32 buffer_depth;
    vx_int32 frame_graph_parameter_index;
} FcwOverlayObj;

vx_status app_init_fcw_overlay(
    vx_context context,
    FcwOverlayObj *obj,
    const FcwOverlayConfig *config,
    vx_uint32 buffer_depth);

vx_status app_create_graph_fcw_overlay(
    vx_graph graph,
    FcwOverlayObj *obj,
    vx_object_array tensor_arr,
    vx_object_array input_image_arr);

void app_delete_fcw_overlay(FcwOverlayObj *obj);
void app_deinit_fcw_overlay(FcwOverlayObj *obj);



#endif

#include "avp_fcw_overlay_module.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* 1にするとフレームごとの警報状態を出力する */
#define FCW_OVERLAY_DEBUG (1)

#define FCW_RETURN_IF_ERROR(expr)                   \
    do                                             \
    {                                              \
        vx_status fcw_status_ = (expr);             \
        if (fcw_status_ != VX_SUCCESS)              \
        {                                          \
            return fcw_status_;                    \
        }                                          \
    } while (0)

typedef struct
{
    FcwOverlayConfig config;
    FcwMainPreContext fcw;

    vx_bool initialized;
    vx_bool has_previous_frame;
    vx_int32 previous_frame_id;
} FcwOverlayState;


static void fcw_overlay_alarm_output(
    vx_bool active,
    void *user_data)
{
    (void)user_data;

    printf("[FCW] ALARM_%s\n",
           (active == vx_true_e) ? "ON" : "OFF");
}


static vx_status fcw_overlay_get_state(
    vx_node node,
    FcwOverlayState **state)
{
    FCW_RETURN_IF_ERROR(
        vxQueryNode(
            node,
            VX_NODE_LOCAL_DATA_PTR,
            state,
            sizeof(*state)));

    return (*state != NULL)
         ? VX_SUCCESS
         : VX_ERROR_NO_RESOURCES;
}


/*
 * 座標は画像全体の座標。
 * NV12のUV面では2x2画素が同じ色差成分を共有する。
 */
static void fcw_overlay_fill_rect(
    void *base,
    const vx_imagepatch_addressing_t *addr,
    vx_uint32 plane,
    vx_uint32 x0,
    vx_uint32 y0,
    vx_uint32 x1,
    vx_uint32 y1,
    const FcwOverlayConfig *config)
{
    vx_uint32 x;
    vx_uint32 y;
    vx_uint32 step = (plane == 0u) ? 1u : 2u;

    if ((x0 >= x1) || (y0 >= y1))
    {
        return;
    }

    if (plane != 0u)
    {
        /* UVの2x2ブロックに合わせる */
        x0 &= ~1u;
        y0 &= ~1u;
        x1 = (x1 + 1u) & ~1u;
        y1 = (y1 + 1u) & ~1u;
    }

    if (x1 > config->width)
    {
        x1 = config->width;
    }

    if (y1 > config->height)
    {
        y1 = config->height;
    }

    for (y = y0; y < y1; y += step)
    {
        for (x = x0; x < x1; x += step)
        {
            vx_uint8 *pixel =
                (vx_uint8 *)vxFormatImagePatchAddress2d(
                    base, x, y, addr);

            if (plane == 0u)
            {
                pixel[0] = config->red_y;
            }
            else
            {
                /* NV12はU,Vの順 */
                pixel[0] = config->red_u;
                pixel[1] = config->red_v;
            }
        }
    }
}


static vx_float32 fcw_overlay_clamp01(vx_float32 value)
{
    if (value < 0.0F)
    {
        return 0.0F;
    }

    if (value > 1.0F)
    {
        return 1.0F;
    }

    return value;
}


static void fcw_overlay_draw_box(
    void *base,
    const vx_imagepatch_addressing_t *addr,
    vx_uint32 plane,
    const FcwBox *box,
    const FcwOverlayConfig *config)
{
    vx_uint32 x0;
    vx_uint32 y0;
    vx_uint32 x1;
    vx_uint32 y1;
    vx_uint32 thickness;

    if (!isfinite(box->xmin) ||
        !isfinite(box->ymin) ||
        !isfinite(box->xmax) ||
        !isfinite(box->ymax))
    {
        return;
    }

    /*
     * 正規化座標を描画画像の座標へ変換する。
     * x1,y1は矩形の外側の境界として扱う。
     */
    x0 = (vx_uint32)floorf(
        fcw_overlay_clamp01(box->xmin) * config->width);

    y0 = (vx_uint32)floorf(
        fcw_overlay_clamp01(box->ymin) * config->height);

    x1 = (vx_uint32)ceilf(
        fcw_overlay_clamp01(box->xmax) * config->width);

    y1 = (vx_uint32)ceilf(
        fcw_overlay_clamp01(box->ymax) * config->height);

    if ((x0 >= x1) || (y0 >= y1))
    {
        return;
    }

    thickness = config->line_width;

    if (thickness > x1 - x0)
    {
        thickness = x1 - x0;
    }

    if (thickness > y1 - y0)
    {
        thickness = y1 - y0;
    }

    /* 上辺 */
    fcw_overlay_fill_rect(
        base, addr, plane,
        x0, y0, x1, y0 + thickness, config);

    /* 下辺 */
    fcw_overlay_fill_rect(
        base, addr, plane,
        x0, y1 - thickness, x1, y1, config);

    /* 左辺 */
    fcw_overlay_fill_rect(
        base, addr, plane,
        x0, y0, x0 + thickness, y1, config);

    /* 右辺 */
    fcw_overlay_fill_rect(
        base, addr, plane,
        x1 - thickness, y0, x1, y1, config);
}


/*
 * 既存の描画結果を出力へコピーし、
 * alert中の車両だけ赤い矩形を重ねる。
 */
static vx_status fcw_overlay_copy_and_draw(
    vx_image input,
    vx_image output,
    const FcwFrameResult *result,
    const FcwOverlayConfig *config)
{
    vx_rectangle_t rect;
    vx_uint32 plane;

    rect.start_x = 0u;
    rect.start_y = 0u;
    rect.end_x = config->width;
    rect.end_y = config->height;

    for (plane = 0u; plane < 2u; plane++)
    {
        vx_imagepatch_addressing_t input_addr;
        vx_imagepatch_addressing_t output_addr;

        vx_map_id input_map;
        vx_map_id output_map;

        void *input_base = NULL;
        void *output_base = NULL;

        vx_uint32 x;
        vx_uint32 y;
        vx_uint32 i;

        vx_uint32 step = (plane == 0u) ? 1u : 2u;
        vx_size pixel_bytes = (plane == 0u) ? 1u : 2u;

        vx_status status;
        vx_status unmap_status;

        /*
         * UV面も、画像全体の座標で矩形を指定する。
         * planeによる間引きはaddressing構造体に反映される。
         */
        status = vxMapImagePatch(
            input,
            &rect,
            plane,
            &input_map,
            &input_addr,
            &input_base,
            VX_READ_ONLY,
            VX_MEMORY_TYPE_HOST,
            VX_NOGAP_X);

        if (status != VX_SUCCESS)
        {
            return status;
        }

        status = vxMapImagePatch(
            output,
            &rect,
            plane,
            &output_map,
            &output_addr,
            &output_base,
            VX_WRITE_ONLY,
            VX_MEMORY_TYPE_HOST,
            VX_NOGAP_X);

        if (status != VX_SUCCESS)
        {
            (void)vxUnmapImagePatch(input, input_map);
            return status;
        }

        /*
         * 画像のpaddingやstrideを決め打ちせず、
         * OpenVXのアドレス変換関数でアクセスする。
         */
        for (y = 0u; y < config->height; y += step)
        {
            for (x = 0u; x < config->width; x += step)
            {
                void *src = vxFormatImagePatchAddress2d(
                    input_base, x, y, &input_addr);

                void *dst = vxFormatImagePatchAddress2d(
                    output_base, x, y, &output_addr);

                memcpy(dst, src, pixel_bytes);
            }
        }

        for (i = 0u; i < result->num_cars; i++)
        {
            const FcwCar *car = &result->cars[i];

            /*
             * any_alertではなく、車両ごとのalertを使用する。
             */
            if ((car->alert == vx_true_e) &&
                (car->roi_valid == vx_true_e) &&
                (car->channel == config->fcw_config.ttc_channel) &&
                (car->class_id == config->fcw_config.car_class_id))
            {
                fcw_overlay_draw_box(
                    output_base,
                    &output_addr,
                    plane,
                    &car->box,
                    config);
            }
        }

        unmap_status = vxUnmapImagePatch(output, output_map);
        if (status == VX_SUCCESS)
        {
            status = unmap_status;
        }

        unmap_status = vxUnmapImagePatch(input, input_map);
        if (status == VX_SUCCESS)
        {
            status = unmap_status;
        }

        if (status != VX_SUCCESS)
        {
            return status;
        }
    }

    return VX_SUCCESS;
}


static vx_status VX_CALLBACK fcw_overlay_validate(
    vx_node node,
    const vx_reference parameters[],
    vx_uint32 num,
    vx_meta_format metas[])
{
    FcwOverlayConfig config;
    vx_size config_size = 0u;
    vx_enum scalar_type;
    vx_enum tensor_type;
    vx_uint32 index;

    (void)node;

    if (num != FCW_OVERLAY_NUM_PARAMS)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    for (index = 0u; index < num; index++)
    {
        if (vxGetStatus(parameters[index]) != VX_SUCCESS)
        {
            return VX_ERROR_INVALID_REFERENCE;
        }
    }

    FCW_RETURN_IF_ERROR(
        vxQueryUserDataObject(
            (vx_user_data_object)parameters[FCW_OVERLAY_CONFIG],
            VX_USER_DATA_OBJECT_SIZE,
            &config_size,
            sizeof(config_size)));

    if (config_size != sizeof(config))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    FCW_RETURN_IF_ERROR(
        vxCopyUserDataObject(
            (vx_user_data_object)parameters[FCW_OVERLAY_CONFIG],
            0u,
            sizeof(config),
            &config,
            VX_READ_ONLY,
            VX_MEMORY_TYPE_HOST));

    if ((config.width == 0u) ||
        (config.height == 0u) ||
        ((config.width & 1u) != 0u) ||
        ((config.height & 1u) != 0u) ||
        (config.line_width == 0u) ||
        (config.fcw_config.ttc_channel != 0u))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    FCW_RETURN_IF_ERROR(
        vxQueryScalar(
            (vx_scalar)parameters[FCW_OVERLAY_FRAME_ID],
            VX_SCALAR_TYPE,
            &scalar_type,
            sizeof(scalar_type)));

    if (scalar_type != VX_TYPE_INT32)
    {
        return VX_ERROR_INVALID_TYPE;
    }

    /*
     * 既存adapterはOD出力をfloat32として解釈している。
     */
    FCW_RETURN_IF_ERROR(
        vxQueryTensor(
            (vx_tensor)parameters[FCW_OVERLAY_TENSOR],
            VX_TENSOR_DATA_TYPE,
            &tensor_type,
            sizeof(tensor_type)));

    if (tensor_type != VX_TYPE_FLOAT32)
    {
        return VX_ERROR_INVALID_TYPE;
    }

    for (index = FCW_OVERLAY_INPUT_IMAGE;
         index <= FCW_OVERLAY_OUTPUT_IMAGE;
         index++)
    {
        vx_image image = (vx_image)parameters[index];
        vx_uint32 width;
        vx_uint32 height;
        vx_df_image format;

        FCW_RETURN_IF_ERROR(
            vxQueryImage(
                image, VX_IMAGE_WIDTH, &width, sizeof(width)));

        FCW_RETURN_IF_ERROR(
            vxQueryImage(
                image, VX_IMAGE_HEIGHT, &height, sizeof(height)));

        FCW_RETURN_IF_ERROR(
            vxQueryImage(
                image, VX_IMAGE_FORMAT, &format, sizeof(format)));

        if ((format != VX_DF_IMAGE_NV12) ||
            (width != config.width) ||
            (height != config.height))
        {
            return VX_ERROR_INVALID_PARAMETERS;
        }
    }

    return vxSetMetaFormatFromReference(
        metas[FCW_OVERLAY_OUTPUT_IMAGE],
        parameters[FCW_OVERLAY_INPUT_IMAGE]);
}


static vx_status VX_CALLBACK fcw_overlay_initialize(
    vx_node node,
    const vx_reference parameters[],
    vx_uint32 num)
{
    FcwOverlayState *state = NULL;

    if (num != FCW_OVERLAY_NUM_PARAMS)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    FCW_RETURN_IF_ERROR(fcw_overlay_get_state(node, &state));

    memset(state, 0, sizeof(*state));

    FCW_RETURN_IF_ERROR(
        vxCopyUserDataObject(
            (vx_user_data_object)parameters[FCW_OVERLAY_CONFIG],
            0u,
            sizeof(state->config),
            &state->config,
            VX_READ_ONLY,
            VX_MEMORY_TYPE_HOST));

    FCW_RETURN_IF_ERROR(
        fcw_main_pre_init(
            &state->fcw,
            &state->config.fcw_config,
            fcw_overlay_alarm_output,
            NULL));

    state->initialized = vx_true_e;

    return VX_SUCCESS;
}


static vx_status VX_CALLBACK fcw_overlay_deinitialize(
    vx_node node,
    const vx_reference parameters[],
    vx_uint32 num)
{
    FcwOverlayState *state = NULL;
    vx_status status;

    (void)parameters;
    (void)num;

    status = fcw_overlay_get_state(node, &state);

    if ((status == VX_SUCCESS) &&
        (state->initialized == vx_true_e))
    {
        fcw_main_pre_deinit(&state->fcw);
        state->initialized = vx_false_e;
    }

    /*
     * stateのメモリ自体はOpenVXが確保・解放する。
     */
    return VX_SUCCESS;
}


static vx_status VX_CALLBACK fcw_overlay_process(
    vx_node node,
    const vx_reference parameters[],
    vx_uint32 num)
{
    FcwOverlayState *state = NULL;
    FcwDetection detections[FCW_MAX_DETECTIONS];

    const FcwFrameResult *result;
    vx_uint32 num_detections = 0u;
    vx_uint32 i;
    vx_int32 frame_id;

    if (num != FCW_OVERLAY_NUM_PARAMS)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    FCW_RETURN_IF_ERROR(fcw_overlay_get_state(node, &state));

    if (state->initialized != vx_true_e)
    {
        return VX_FAILURE;
    }

    FCW_RETURN_IF_ERROR(
        vxCopyScalar(
            (vx_scalar)parameters[FCW_OVERLAY_FRAME_ID],
            &frame_id,
            VX_READ_ONLY,
            VX_MEMORY_TYPE_HOST));

    /*
     * 履歴を持つFCWへ、逆順・重複フレームを渡さない。
     * 現在の設定どおり、動画を1回再生する使い方を対象とする。
     */
    if ((state->has_previous_frame == vx_true_e) &&
        (frame_id <= state->previous_frame_id))
    {
        printf("[FCW] Invalid frame order: previous=%d current=%d\n",
               state->previous_frame_id, frame_id);

        return VX_FAILURE;
    }

    FCW_RETURN_IF_ERROR(
        fcw_tidl_adapter_extract_tensor(
            &state->config.io_buf_desc,
            (vx_tensor)parameters[FCW_OVERLAY_TENSOR],
            state->config.fcw_config.ttc_channel,
            detections,
            FCW_MAX_DETECTIONS,
            &num_detections));

    FCW_RETURN_IF_ERROR(
        fcw_main_pre_begin_frame(&state->fcw, frame_id));

    for (i = 0u; i < num_detections; i++)
    {
        FCW_RETURN_IF_ERROR(
            fcw_main_pre_add_detection(
                &state->fcw, &detections[i]));
    }

    FCW_RETURN_IF_ERROR(
        fcw_main_pre_finalize_frame(&state->fcw));

    state->previous_frame_id = frame_id;
    state->has_previous_frame = vx_true_e;

    result = fcw_main_pre_get_frame_result(&state->fcw);
    if (result == NULL)
    {
        return VX_FAILURE;
    }

#if FCW_OVERLAY_DEBUG
    printf("[FCW] frame=%d cars=%u alarm=%d\n",
           frame_id, result->num_cars, (int)result->any_alert);

    for (i = 0u; i < result->num_cars; i++)
    {
        const FcwCar *car = &result->cars[i];

        printf("[FCW] track=%d alert=%d ttc_valid=%d ttc=%.3f\n",
               car->track_id,
               (int)car->alert,
               (int)car->ttc_valid,
               car->ttc_sec);
    }
#endif

    return fcw_overlay_copy_and_draw(
        (vx_image)parameters[FCW_OVERLAY_INPUT_IMAGE],
        (vx_image)parameters[FCW_OVERLAY_OUTPUT_IMAGE],
        result,
        &state->config);
}


vx_status app_init_fcw_overlay(
    vx_context context,
    FcwOverlayObj *obj,
    const FcwOverlayConfig *config,
    vx_uint32 buffer_depth)
{
    vx_status status = VX_SUCCESS;
    vx_enum kernel_id;
    vx_size local_size = sizeof(FcwOverlayState);
    vx_image exemplar = NULL;
    vx_int32 initial_frame = -1;
    vx_uint32 i;

    const vx_enum types[FCW_OVERLAY_NUM_PARAMS] =
    {
        VX_TYPE_USER_DATA_OBJECT,
        VX_TYPE_SCALAR,
        VX_TYPE_TENSOR,
        VX_TYPE_IMAGE,
        VX_TYPE_IMAGE
    };

    if ((obj == NULL) ||
        (config == NULL) ||
        (buffer_depth == 0u) ||
        (buffer_depth > APP_MAX_BUFQ_DEPTH) ||
        (NUM_CH != 1))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memset(obj, 0, sizeof(*obj));

    obj->buffer_depth = buffer_depth;
    obj->frame_graph_parameter_index = -1;

    status = vxAllocateUserKernelId(context, &kernel_id);
    if (status != VX_SUCCESS)
    {
        goto fail;
    }

    obj->kernel = vxAddUserKernel(
        context,
        "app.fcw.overlay",
        kernel_id,
        fcw_overlay_process,
        FCW_OVERLAY_NUM_PARAMS,
        fcw_overlay_validate,
        fcw_overlay_initialize,
        fcw_overlay_deinitialize);

    status = vxGetStatus((vx_reference)obj->kernel);
    if (status != VX_SUCCESS)
    {
        obj->kernel = NULL;
        goto fail;
    }

    for (i = 0u; i < FCW_OVERLAY_NUM_PARAMS; i++)
    {
        vx_enum direction =
            (i == FCW_OVERLAY_OUTPUT_IMAGE)
            ? VX_OUTPUT
            : VX_INPUT;

        status = vxAddParameterToKernel(
            obj->kernel,
            i,
            direction,
            types[i],
            VX_PARAMETER_STATE_REQUIRED);

        if (status != VX_SUCCESS)
        {
            goto fail;
        }
    }

    /*
     * ノードごとのFCW状態をOpenVXに確保してもらう。
     */
    status = vxSetKernelAttribute(
        obj->kernel,
        VX_KERNEL_LOCAL_DATA_SIZE,
        &local_size,
        sizeof(local_size));

    if (status != VX_SUCCESS)
    {
        goto fail;
    }

    status = vxFinalizeKernel(obj->kernel);
    if (status != VX_SUCCESS)
    {
        goto fail;
    }

    obj->config = vxCreateUserDataObject(
        context,
        "FcwOverlayConfig",
        sizeof(*config),
        config);

    status = vxGetStatus((vx_reference)obj->config);
    if (status != VX_SUCCESS)
    {
        goto fail;
    }

    for (i = 0u; i < buffer_depth; i++)
    {
        obj->frame_id[i] = vxCreateScalar(
            context,
            VX_TYPE_INT32,
            &initial_frame);

        status = vxGetStatus((vx_reference)obj->frame_id[i]);
        if (status != VX_SUCCESS)
        {
            goto fail;
        }
    }

    exemplar = vxCreateImage(
        context,
        config->width,
        config->height,
        VX_DF_IMAGE_NV12);

    status = vxGetStatus((vx_reference)exemplar);
    if (status != VX_SUCCESS)
    {
        goto fail;
    }

    obj->output_image_arr = vxCreateObjectArray(
        context,
        (vx_reference)exemplar,
        NUM_CH);

    status = vxGetStatus((vx_reference)obj->output_image_arr);

    (void)vxReleaseImage(&exemplar);

    if (status != VX_SUCCESS)
    {
        goto fail;
    }

    return VX_SUCCESS;

fail:
    if (exemplar != NULL)
    {
        (void)vxReleaseImage(&exemplar);
    }

    app_deinit_fcw_overlay(obj);
    return status;
}


vx_status app_create_graph_fcw_overlay(
    vx_graph graph,
    FcwOverlayObj *obj,
    vx_object_array tensor_arr,
    vx_object_array input_image_arr)
{
    vx_reference refs[FCW_OVERLAY_NUM_PARAMS] = {NULL};
    vx_status status = VX_SUCCESS;
    vx_uint32 i;

    vx_bool replicate[FCW_OVERLAY_NUM_PARAMS] =
    {
        vx_false_e, /* config */
        vx_false_e, /* frame_id */
        vx_true_e,  /* tensor */
        vx_true_e,  /* input image */
        vx_true_e   /* output image */
    };

    refs[FCW_OVERLAY_CONFIG] = (vx_reference)obj->config;
    refs[FCW_OVERLAY_FRAME_ID] = (vx_reference)obj->frame_id[0];

    refs[FCW_OVERLAY_TENSOR] =
        vxGetObjectArrayItem(tensor_arr, 0u);

    refs[FCW_OVERLAY_INPUT_IMAGE] =
        vxGetObjectArrayItem(input_image_arr, 0u);

    refs[FCW_OVERLAY_OUTPUT_IMAGE] =
        vxGetObjectArrayItem(obj->output_image_arr, 0u);

    for (i = 0u; i < FCW_OVERLAY_NUM_PARAMS; i++)
    {
        status = vxGetStatus(refs[i]);
        if (status != VX_SUCCESS)
        {
            goto cleanup;
        }
    }

    obj->node = vxCreateGenericNode(graph, obj->kernel);

    status = vxGetStatus((vx_reference)obj->node);
    if (status != VX_SUCCESS)
    {
        obj->node = NULL;
        goto cleanup;
    }

    for (i = 0u; i < FCW_OVERLAY_NUM_PARAMS; i++)
    {
        status = vxSetParameterByIndex(obj->node, i, refs[i]);

        if (status != VX_SUCCESS)
        {
            goto cleanup;
        }
    }

    /*
     * FCWはA72/Linux側で実行する。
     */
    status = vxSetNodeTarget(
        obj->node, VX_TARGET_STRING, TIVX_TARGET_HOST);

    if (status != VX_SUCCESS)
    {
        goto cleanup;
    }

    /*
     * 1チャネルでも、既存のobject array経路に合わせて
     * tensorと画像のパラメータをreplicateする。
     */
    status = vxReplicateNode(
        graph,
        obj->node,
        replicate,
        FCW_OVERLAY_NUM_PARAMS);

    if (status == VX_SUCCESS)
    {
        status = vxSetReferenceName(
            (vx_reference)obj->node,
            "FcwOverlayNode");
    }

cleanup:
    for (i = FCW_OVERLAY_TENSOR;
         i < FCW_OVERLAY_NUM_PARAMS;
         i++)
    {
        if (refs[i] != NULL)
        {
            (void)vxReleaseReference(&refs[i]);
        }
    }

    if (status != VX_SUCCESS)
    {
        app_delete_fcw_overlay(obj);
    }

    return status;
}


void app_delete_fcw_overlay(FcwOverlayObj *obj)
{
    if ((obj != NULL) && (obj->node != NULL))
    {
        (void)vxReleaseNode(&obj->node);
    }
}


void app_deinit_fcw_overlay(FcwOverlayObj *obj)
{
    vx_uint32 i;

    if (obj == NULL)
    {
        return;
    }

    /*
     * app_delete_graph()でグラフを解放した後に呼ぶ。
     */
    if (obj->output_image_arr != NULL)
    {
        (void)vxReleaseObjectArray(&obj->output_image_arr);
    }

    for (i = 0u; i < obj->buffer_depth; i++)
    {
        if (obj->frame_id[i] != NULL)
        {
            (void)vxReleaseScalar(&obj->frame_id[i]);
        }
    }

    if (obj->config != NULL)
    {
        (void)vxReleaseUserDataObject(&obj->config);
    }

    if (obj->kernel != NULL)
    {
        (void)vxRemoveKernel(obj->kernel);
        obj->kernel = NULL;
    }

    obj->buffer_depth = 0u;
    obj->frame_graph_parameter_index = -1;
}

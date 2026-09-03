#include "fcw_tidl_adapter.h"


/*
 * Read one TIDL OD output object and convert it to the FCW boundary type.
 *
 * TIDL_ODLayerObjInfo is intentionally handled only in this adapter.  The
 * ROI, tracker, TTC, and alert modules do not depend on TIDL structures.
 */
static void fcw_tidl_adapter_copy_object(
    const TIDL_ODLayerObjInfo *object,
    FcwDetection *detection
)
{
    detection->object_id = (vx_int32)object->ObjId;
    detection->class_id = (vx_int32)object->label;
    detection->score = (vx_float32)object->score;

    /* TIDL OD 2D coordinates are normalized coordinates. */
    detection->box.xmin = (vx_float32)object->xmin;
    detection->box.ymin = (vx_float32)object->ymin;
    detection->box.xmax = (vx_float32)object->xmax;
    detection->box.ymax = (vx_float32)object->ymax;
}


vx_status fcw_tidl_adapter_extract(
    const sTIDL_IOBufDesc_t *io_buf_desc,
    vx_object_array output_tensor_arr,
    vx_uint32 channel,
    FcwDetection *detections,
    vx_uint32 detection_capacity,
    vx_uint32 *num_detections
)
{
    vx_status status;
    vx_status unmap_status;

    vx_tensor output_tensor;
    vx_reference output_ref;

    vx_map_id map_id;
    vx_bool mapped;

    vx_size start[APP_MAX_TENSOR_DIMS];
    vx_size output_sizes[APP_MAX_TENSOR_DIMS];
    vx_size output_strides[APP_MAX_TENSOR_DIMS];

    void *output_buffer;
    vx_float32 *p_out;

    TIDL_ODLayerHeaderInfo *header;
    TIDL_ODLayerObjInfo *obj_info;
    TIDL_ODLayerObjInfo *object;

    vx_uint32 num_objects;
    vx_uint32 obj_info_size;
    vx_uint32 obj_info_offset;
    vx_uint32 i;

    if ((io_buf_desc == NULL) ||
        (detections == NULL) ||
        (num_detections == NULL))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (output_tensor_arr == NULL)
    {
        return VX_ERROR_INVALID_REFERENCE;
    }

    if (detection_capacity == 0u)
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    if ((io_buf_desc->outWidth[0] <= 0) ||
        (io_buf_desc->outHeight[0] <= 0) ||
        (io_buf_desc->outNumChannels[0] <= 0))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    *num_detections = 0u;

    output_tensor = NULL;
    output_ref = NULL;
    output_buffer = NULL;
    p_out = NULL;
    header = NULL;
    obj_info = NULL;
    object = NULL;
    mapped = vx_false_e;
    map_id = 0u;
    status = VX_SUCCESS;

    /* The OD output is an object array with one tensor per camera channel. */
    output_ref =
        vxGetObjectArrayItem(
            output_tensor_arr,
            channel
        );

    status = vxGetStatus(output_ref);

    if (status != VX_SUCCESS)
    {
        return status;
    }

    output_tensor = (vx_tensor)output_ref;

    /*
     * This layout is the same layout already used by drawDetections():
     *   dimension 0: tensor width including padding
     *   dimension 1: tensor height including padding
     *   dimension 2: number of output channels
     */
    start[0] = 0u;
    start[1] = 0u;
    start[2] = 0u;
    start[3] = 0u;

    output_sizes[0] =
        (vx_size)io_buf_desc->outWidth[0] +
        (vx_size)io_buf_desc->outPadL[0] +
        (vx_size)io_buf_desc->outPadR[0];

    output_sizes[1] =
        (vx_size)io_buf_desc->outHeight[0] +
        (vx_size)io_buf_desc->outPadT[0] +
        (vx_size)io_buf_desc->outPadB[0];

    output_sizes[2] =
        (vx_size)io_buf_desc->outNumChannels[0];

    output_strides[0] = 1u;
    output_strides[1] = output_sizes[0];
    output_strides[2] = output_sizes[0] * output_sizes[1];

    status =
        tivxMapTensorPatch(
            output_tensor,
            3u,
            start,
            output_sizes,
            &map_id,
            output_strides,
            &output_buffer,
            VX_READ_ONLY,
            VX_MEMORY_TYPE_HOST
        );

    if (status != VX_SUCCESS)
    {
        goto cleanup;
    }

    mapped = vx_true_e;

    /* Skip the tensor padding before interpreting the TIDL header. */
    p_out =
        (vx_float32 *)output_buffer +
        ((vx_uint32)io_buf_desc->outPadT[0] *
         (vx_uint32)output_sizes[0]) +
        (vx_uint32)io_buf_desc->outPadL[0];

    header = (TIDL_ODLayerHeaderInfo *)p_out;

    /* FCW currently consumes only 2D detection output. */
    if ((header->odObjectType &
         (1 << TIDL_Detect2DBox)) == 0)
    {
        status = VX_ERROR_NOT_SUPPORTED;
        goto cleanup;
    }

    if (!(header->numDetObjects >= 0.0F))
    {
        status = VX_ERROR_INVALID_PARAMETERS;
        goto cleanup;
    }

    if (!(header->objInfoSize > 0.0F))
    {
        status = VX_ERROR_INVALID_PARAMETERS;
        goto cleanup;
    }

    num_objects = (vx_uint32)header->numDetObjects;
    obj_info_size = (vx_uint32)header->objInfoSize;
    obj_info_offset = (vx_uint32)header->objInfoOffset;

    /*
     * objInfoOffset points from the beginning of the output payload to the
     * first TIDL_ODLayerObjInfo record.  objInfoSize is the byte stride of
     * one record and must be used instead of sizeof(TIDL_ODLayerObjInfo),
     * because optional keypoint/pose payloads may change the record stride.
     */
    obj_info =
        (TIDL_ODLayerObjInfo *)(
            (vx_uint8 *)p_out + obj_info_offset
        );

    for (i = 0u; i < num_objects; i++)
    {
        object =
            (TIDL_ODLayerObjInfo *)(
                (vx_uint8 *)obj_info +
                (i * obj_info_size)
            );

        /* TIDL uses ObjId == -1 for an unused object slot. */
        if (object->ObjId < 0.0F)
        {
            continue;
        }

        if (*num_detections >= detection_capacity)
        {
            status = VX_ERROR_NOT_SUFFICIENT;
            break;
        }

        fcw_tidl_adapter_copy_object(
            object,
            &detections[*num_detections]
        );

        (*num_detections)++;
    }


cleanup:
    if (mapped == vx_true_e)
    {
        unmap_status =
            tivxUnmapTensorPatch(
                output_tensor,
                map_id
            );

        if (status == VX_SUCCESS)
        {
            status = unmap_status;
        }
    }

    if (output_tensor != NULL)
    {
        vxReleaseTensor(&output_tensor);
    }

    return status;
}

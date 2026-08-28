// 追加

#include "avp_fcw_module.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static vx_float32 avp_fcw_clamp(vx_float32 value, vx_float32 low, vx_float32 high)
{
    if(value < low)
    {
        return low;
    }
    if(value > high)
    {
        return high;
    }
    return value;
}

static vx_float32 avp_fcw_iou(const vx_float32 box1[4], const vx_float32 box2[4])
{
    vx_float32 intersection_ymin = fmaxf(box1[0], box2[0]);
    vx_float32 intersection_xmin = fmaxf(box1[1], box2[1]);
    vx_float32 intersection_ymax = fminf(box1[2], box2[2]);
    vx_float32 intersection_xmax = fminf(box1[3], box2[3]);
    vx_float32 intersection_width = fmaxf(0.0f, intersection_xmax - intersection_xmin);
    vx_float32 intersection_height = fmaxf(0.0f, intersection_ymax - intersection_ymin);
    vx_float32 intersection_area = intersection_width * intersection_height;
    vx_float32 box1_area = fmaxf(0.0f, box1[3] - box1[1]) * fmaxf(0.0f, box1[2] - box1[0]);
    vx_float32 box2_area = fmaxf(0.0f, box2[3] - box2[1]) * fmaxf(0.0f, box2[2] - box2[0]);
    vx_float32 union_area = box1_area + box2_area - intersection_area;

    if(union_area <= 0.0f)
    {
        return 0.0f;
    }
    return intersection_area / union_area;
}

static vx_bool avp_fcw_point_in_roi(const AvpFwcConfig *config,
                                    vx_float32 x,
                                    vx_float32 y)
{
    vx_uint32 i;
    vx_bool inside = vx_false_e;

    /* Standard ray-casting test. The ROI is a convex quadrilateral here,
     * but this also works if its shape is changed later. */
    for(i = 0; i < 4u; i++)
    {
        vx_uint32 j = (i + 3u) % 4u;
        vx_float32 xi = config->roi[i][0];
        vx_float32 yi = config->roi[i][1];
        vx_float32 xj = config->roi[j][0];
        vx_float32 yj = config->roi[j][1];
        vx_bool crosses = ((yi > y) != (yj > y));

        if(crosses && (x < (xj - xi) * (y - yi) / (yj - yi) + xi))
        {
            inside = inside ? vx_false_e : vx_true_e;
        }
    }
    return inside;
}

static void avp_fcw_track_clear(AvpFwcTrack *track)
{
    memset(track, 0, sizeof(*track));
}

static void avp_fcw_drop_track(AvpFwcTrack *track)
{
    avp_fcw_track_clear(track);
}

static AvpFwcTrack *avp_fcw_find_track(AvpFwcContext *context, vx_uint32 track_id)
{
    vx_uint32 i;

    for(i = 0; i < AVP_FCW_MAX_TRACKS; i++)
    {
        if(context->tracks[i].active && context->tracks[i].track_id == track_id)
        {
            return &context->tracks[i];
        }
    }
    return NULL;
}

static AvpFwcTrack *avp_fcw_alloc_track(AvpFwcContext *context)
{
    vx_uint32 i;

    for(i = 0; i < AVP_FCW_MAX_TRACKS; i++)
    {
        if(!context->tracks[i].active)
        {
            avp_fcw_track_clear(&context->tracks[i]);
            context->tracks[i].active = vx_true_e;
            context->tracks[i].track_id = context->next_track_id++;
            if(context->next_track_id == 0u)
            {
                context->next_track_id = 1u;
            }
            return &context->tracks[i];
        }
    }
    return NULL;
}

static void avp_fcw_update_tracks(AvpFwcContext *context,
                                  AvpFwcResult *results,
                                  vx_uint32 num_results)
{
    vx_uint32 i;
    vx_uint32 j;
    vx_bool matched[AVP_FCW_MAX_TRACKS] = {vx_false_e};

    for(i = 0; i < AVP_FCW_MAX_TRACKS; i++)
    {
        if(context->tracks[i].active)
        {
            context->tracks[i].missed_frames++;
        }
    }

    for(i = 0; i < num_results; i++)
    {
        vx_float32 current_box[4] = {
            results[i].ymin, results[i].xmin,
            results[i].ymax, results[i].xmax
        };
        vx_float32 best_iou = 0.0f;
        vx_int32 best_track_index = -1;

        for(j = 0; j < AVP_FCW_MAX_TRACKS; j++)
        {
            vx_float32 iou;

            if(!context->tracks[j].active || matched[j])
            {
                continue;
            }

            iou = avp_fcw_iou(context->tracks[j].box, current_box);
            if(iou > best_iou)
            {
                best_iou = iou;
                best_track_index = (vx_int32)j;
            }
        }

        if((best_track_index >= 0) &&
           (best_iou >= context->config.iou_threshold))
        {
            AvpFwcTrack *track = &context->tracks[best_track_index];
            track->box[0] = current_box[0];
            track->box[1] = current_box[1];
            track->box[2] = current_box[2];
            track->box[3] = current_box[3];
            track->missed_frames = 0u;
            matched[best_track_index] = vx_true_e;
            results[i].track_id = track->track_id;
            results[i].iou = best_iou;
        }
        else
        {
            AvpFwcTrack *track = avp_fcw_alloc_track(context);
            if(track == NULL)
            {
                results[i].track_id = 0u;
                results[i].iou = 0.0f;
                continue;
            }
            memcpy(track->box, current_box, sizeof(current_box));
            results[i].track_id = track->track_id;
            results[i].iou = 0.0f;
        }
    }
}

static void avp_fcw_append_height(AvpFwcContext *context,
                                  AvpFwcTrack *track,
                                  vx_uint32 frame_index,
                                  vx_float32 height_px)
{
    vx_uint32 history_size = context->config.history_size;

    if(history_size == 0u)
    {
        return;
    }
    if(history_size > AVP_FCW_MAX_HISTORY)
    {
        history_size = AVP_FCW_MAX_HISTORY;
    }

    if(track->history_count >= history_size)
    {
        memmove(&track->history_frame[0],
                &track->history_frame[1],
                (history_size - 1u) * sizeof(track->history_frame[0]));
        memmove(&track->history_height[0],
                &track->history_height[1],
                (history_size - 1u) * sizeof(track->history_height[0]));
        track->history_count = history_size - 1u;
    }

    track->history_frame[track->history_count] = frame_index;
    track->history_height[track->history_count] = height_px;
    track->history_count++;
}

static void avp_fcw_calculate_ttc(const AvpFwcContext *context,
                                  const AvpFwcTrack *track,
                                  vx_float32 *dh_dt,
                                  vx_float32 *ttc,
                                  vx_float32 *r_squared)
{
    vx_uint32 i;
    vx_uint32 count = track->history_count;
    double mean_time = 0.0;
    double mean_height = 0.0;
    double covariance = 0.0;
    double time_variance = 0.0;
    double residual_sum = 0.0;
    double total_sum = 0.0;
    double slope;
    double intercept;

    *dh_dt = AVP_FCW_INVALID_VALUE;
    *ttc = AVP_FCW_INVALID_VALUE;
    *r_squared = AVP_FCW_INVALID_VALUE;

    if((count < context->config.min_history_size) || (context->config.fps <= 0.0f))
    {
        return;
    }

    for(i = 0; i < count; i++)
    {
        double time_s = ((double)track->history_frame[i] -
                         (double)track->history_frame[0]) /
                        (double)context->config.fps;
        mean_time += time_s;
        mean_height += track->history_height[i];
    }
    mean_time /= count;
    mean_height /= count;

    for(i = 0; i < count; i++)
    {
        double time_s = ((double)track->history_frame[i] -
                         (double)track->history_frame[0]) /
                        (double)context->config.fps;
        covariance += (time_s - mean_time) *
                      ((double)track->history_height[i] - mean_height);
        time_variance += (time_s - mean_time) * (time_s - mean_time);
    }

    if(time_variance <= DBL_EPSILON)
    {
        return;
    }

    slope = covariance / time_variance;
    intercept = mean_height - slope * mean_time;
    *dh_dt = (vx_float32)slope;

    for(i = 0; i < count; i++)
    {
        double time_s = ((double)track->history_frame[i] -
                         (double)track->history_frame[0]) /
                        (double)context->config.fps;
        double predicted = slope * time_s + intercept;
        double error = (double)track->history_height[i] - predicted;
        double deviation = (double)track->history_height[i] - mean_height;
        residual_sum += error * error;
        total_sum += deviation * deviation;
    }

    *r_squared = (total_sum <= DBL_EPSILON) ? 1.0f :
                 (vx_float32)(1.0 - residual_sum / total_sum);

    /* The Python implementation only reports TTC for an approaching object
     * and only when the regression quality is sufficient. */
    if((slope <= 0.0) || (*r_squared < context->config.r_squared_threshold))
    {
        return;
    }

    if(track->history_height[count - 1u] > 0.0f)
    {
        *ttc = (vx_float32)(track->history_height[count - 1u] / slope);
    }
}

static void avp_fcw_update_alert(const AvpFwcContext *context,
                                 AvpFwcTrack *track,
                                 vx_float32 ttc)
{
    if(track->alert)
    {
        if(!isfinite(ttc) || (ttc >= context->config.off_ttc_threshold))
        {
            track->alert = vx_false_e;
            track->alert_count = 0u;
        }
    }
    else if(isfinite(ttc) && (ttc <= context->config.on_ttc_threshold))
    {
        track->alert_count++;
        if(track->alert_count >= context->config.required_count)
        {
            track->alert = vx_true_e;
        }
    }
    else
    {
        track->alert_count = 0u;
    }
}

void avp_fcw_config_set_defaults(AvpFwcConfig *config)
{
    if(config == NULL)
    {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->fps = 25.0f;
    config->score_threshold = 0.4f;
    config->target_class_id = 3; /* COCO car, matching code/fcw/config.py */
    config->iou_threshold = 0.3f;
    config->max_missed_frames = 150u;
    config->history_size = 5u;
    config->min_history_size = 5u;
    config->r_squared_threshold = 0.8f;
    config->on_ttc_threshold = 4.0f;
    config->off_ttc_threshold = 6.5f;
    config->required_count = 3u;

    config->roi[0][0] = 0.42f;
    config->roi[0][1] = 0.45f;
    config->roi[1][0] = 0.58f;
    config->roi[1][1] = 0.45f;
    config->roi[2][0] = 0.70f;
    config->roi[2][1] = 1.00f;
    config->roi[3][0] = 0.30f;
    config->roi[3][1] = 1.00f;
}

vx_status avp_fcw_init(AvpFwcContext *context, const AvpFwcConfig *config)
{
    if((context == NULL) || (config == NULL) ||
       (config->fps <= 0.0f) ||
       (config->history_size == 0u) ||
       (config->history_size > AVP_FCW_MAX_HISTORY) ||
       (config->min_history_size == 0u) ||
       (config->min_history_size > config->history_size) ||
       (config->required_count == 0u) ||
       (config->on_ttc_threshold >= config->off_ttc_threshold))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memcpy(&context->config, config, sizeof(*config));
    avp_fcw_reset(context);
    return VX_SUCCESS;
}

void avp_fcw_reset(AvpFwcContext *context)
{
    if(context == NULL)
    {
        return;
    }

    memset(context->tracks, 0, sizeof(context->tracks));
    context->next_track_id = 1u;
}

vx_status avp_fcw_process(AvpFwcContext *context,
                          const AvpFwcDetection *detections,
                          vx_uint32 num_detections,
                          vx_uint32 frame_index,
                          vx_uint32 image_width,
                          vx_uint32 image_height,
                          AvpFwcFrameResult *result)
{
    vx_uint32 i;

    if((context == NULL) || (result == NULL) ||
       (image_width == 0u) || (image_height == 0u) ||
       ((num_detections > 0u) && (detections == NULL)))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    memset(result, 0, sizeof(*result));
    for(i = 0; (i < num_detections) &&
               (result->num_detections < AVP_FCW_MAX_DETECTIONS); i++)
    {
        const AvpFwcDetection *input = &detections[i];
        AvpFwcResult *output;
        vx_float32 bottom_center_x;
        vx_float32 bottom_center_y;

        if((input->class_id != context->config.target_class_id) ||
           (input->score < context->config.score_threshold))
        {
            continue;
        }

        bottom_center_x = (input->xmin + input->xmax) * 0.5f;
        bottom_center_y = input->ymax;
        if(!avp_fcw_point_in_roi(&context->config,
                                 bottom_center_x,
                                 bottom_center_y))
        {
            continue;
        }

        output = &result->detections[result->num_detections++];
        output->score = input->score;
        output->xmin = avp_fcw_clamp(input->xmin, 0.0f, 1.0f);
        output->ymin = avp_fcw_clamp(input->ymin, 0.0f, 1.0f);
        output->xmax = avp_fcw_clamp(input->xmax, 0.0f, 1.0f);
        output->ymax = avp_fcw_clamp(input->ymax, 0.0f, 1.0f);
        output->height_px = fmaxf(0.0f, output->ymax - output->ymin) * image_height;
        output->bottom_center_x = (vx_int32)(bottom_center_x * image_width);
        output->bottom_center_y = (vx_int32)(bottom_center_y * image_height);
        output->dh_dt = AVP_FCW_INVALID_VALUE;
        output->ttc = AVP_FCW_INVALID_VALUE;
        output->r_squared = AVP_FCW_INVALID_VALUE;
        output->history_length = 0u;
        output->alert = vx_false_e;
    }

    avp_fcw_update_tracks(context, result->detections, result->num_detections);

    for(i = 0; i < result->num_detections; i++)
    {
        AvpFwcTrack *track = avp_fcw_find_track(context, result->detections[i].track_id);
        if(track != NULL)
        {
            avp_fcw_append_height(context, track, frame_index, result->detections[i].height_px);
            avp_fcw_calculate_ttc(context, track,
                                  &result->detections[i].dh_dt,
                                  &result->detections[i].ttc,
                                  &result->detections[i].r_squared);
            avp_fcw_update_alert(context, track, result->detections[i].ttc);
            result->detections[i].history_length = track->history_count;
            result->detections[i].alert = track->alert;
            if(track->alert)
            {
                result->alert_active = vx_true_e;
            }
        }
    }

    for(i = 0; i < AVP_FCW_MAX_TRACKS; i++)
    {
        if(context->tracks[i].active &&
           (context->tracks[i].missed_frames > context->config.max_missed_frames))
        {
            avp_fcw_drop_track(&context->tracks[i]);
        }
    }

    return VX_SUCCESS;
}

vx_status avp_fcw_process_tidl(AvpFwcContext *context,
                                vx_tensor output_tensor,
                                const sTIDL_IOBufDesc_t *io_buf_desc,
                                vx_uint32 frame_index,
                                vx_uint32 image_width,
                                vx_uint32 image_height,
                                AvpFwcFrameResult *result)
{
    vx_status status;
    vx_size start[3] = {0, 0, 0};
    vx_size sizes[3];
    vx_size strides[3];
    vx_map_id map_id = 0;
    void *output_buffer = NULL;
    AvpFwcDetection detections[AVP_FCW_MAX_DETECTIONS];
    vx_uint32 num_detections = 0u;
    TIDL_ODLayerHeaderInfo *header;
    vx_uint32 num_objects;
    vx_uint32 i;

    if((context == NULL) || (output_tensor == NULL) ||
       (io_buf_desc == NULL) || (result == NULL))
    {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    sizes[0] = io_buf_desc->outWidth[0] + io_buf_desc->outPadL[0] + io_buf_desc->outPadR[0];
    sizes[1] = io_buf_desc->outHeight[0] + io_buf_desc->outPadT[0] + io_buf_desc->outPadB[0];
    sizes[2] = io_buf_desc->outNumChannels[0];
    strides[0] = 1;
    strides[1] = sizes[0];
    strides[2] = sizes[0] * sizes[1];

    status = tivxMapTensorPatch(output_tensor, 3, start, sizes, &map_id,
                                strides, &output_buffer,
                                VX_READ_ONLY, VX_MEMORY_TYPE_HOST);
    if(status != VX_SUCCESS)
    {
        return status;
    }

    header = (TIDL_ODLayerHeaderInfo *)((vx_uint8 *)output_buffer +
             (io_buf_desc->outPadT[0] * sizes[0]) + io_buf_desc->outPadL[0]);
    num_objects = (vx_uint32)fmaxf(0.0f, header->numDetObjects);
    num_objects = (num_objects > AVP_FCW_MAX_DETECTIONS) ?
                  AVP_FCW_MAX_DETECTIONS : num_objects;

    for(i = 0; i < num_objects; i++)
    {
        TIDL_ODLayerObjInfo *object = (TIDL_ODLayerObjInfo *)
            ((vx_uint8 *)header + (vx_uint32)header->objInfoOffset +
             i * (vx_uint32)header->objInfoSize);

        detections[num_detections].ymin = object->ymin;
        detections[num_detections].xmin = object->xmin;
        detections[num_detections].ymax = object->ymax;
        detections[num_detections].xmax = object->xmax;
        detections[num_detections].score = object->score;
        detections[num_detections].class_id = (vx_int32)object->label;
        num_detections++;
    }

    tivxUnmapTensorPatch(output_tensor, map_id);
    return avp_fcw_process(context, detections, num_detections,
                           frame_index, image_width, image_height, result);
}

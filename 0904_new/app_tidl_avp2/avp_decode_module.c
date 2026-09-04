#include "avp_decode_module.h"

/*
 * 外部ファイル（main.c）から呼び出す公開関数。
 * 詳細な機能、引数、戻り値についてはavp_decode_module.hにも記載している。
 */
 // ここから
vx_status avp_decode_h264_next_frame(
    AvpDecodeContext **context,
    const char *h264_path,
    const uint8_t **nv12_data,
    size_t *nv12_size,
    uint32_t *width,
    uint32_t *height);

void avp_decode_release(AvpDecodeContext **context);
// ここまでの2つの関数がmain.cから呼び出す公開関数
// avp_decode_h264_next_frameは1回の呼び出しにつき1フレームのデコードを行う

#if defined(LINUX)

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

/*
 * appsinkからのpullを打ち切るまでの時間。無限待ちを避けるために使用する。
 */
#define AVP_DECODE_PULL_TIMEOUT_NS  ((GstClockTime)(5 * GST_SECOND))

struct AvpDecodeContext
{
    GstElement *pipeline;
    GstAppSink *appsink;
    char h264_path[APP_MAX_FILE_PATH];

    uint8_t *working_frame;
    size_t frame_size;

    uint32_t width;
    uint32_t height;
};

/*
 * GStreamerのバスからエラーまたは警告を取り出して表示する。
 *
 * context : エラーを確認するGStreamerパイプラインを保持したデコーダーコンテキスト。
 *
 * 戻り値 : メッセージを取得して表示した場合はvx_true_e、無い場合はvx_false_e。
 */
static vx_bool avp_decode_print_bus_error(AvpDecodeContext *context)
{
    GstBus *bus;
    GstMessage *message;
    vx_bool message_found = vx_false_e;

    bus = gst_element_get_bus(context->pipeline);
    message = gst_bus_pop_filtered(bus, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING));

    if(message != NULL)
    {
        if(GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR)
        {
            GError *error = NULL;
            gchar *debug = NULL;
            gst_message_parse_error(message, &error, &debug);
            printf("avp_decode: GStreamer ERROR: %s\n",
                   (error != NULL) ? error->message : "unknown error");
            if(debug != NULL)
            {
                printf("avp_decode: GStreamer DEBUG: %s\n", debug);
            }
            if(error != NULL)
            {
                g_error_free(error);
            }
            g_free(debug);
        }
        else
        {
            GError *error = NULL;
            gchar *debug = NULL;
            gst_message_parse_warning(message, &error, &debug);
            printf("avp_decode: GStreamer WARNING: %s\n",
                   (error != NULL) ? error->message : "unknown warning");
            if(debug != NULL)
            {
                printf("avp_decode: GStreamer DEBUG: %s\n", debug);
            }
            if(error != NULL)
            {
                g_error_free(error);
            }
            g_free(debug);
        }
        gst_message_unref(message);
        message_found = vx_true_e;
    }
    gst_object_unref(bus);

    return message_found;
}

/*
 * デコーダーコンテキストが保持する作業用・履歴用NV12バッファを解放する。
 *
 * context : バッファを解放するデコーダーコンテキスト。
 */
static void avp_decode_free_buffers(AvpDecodeContext *context)
{
    free(context->working_frame);

    context->working_frame = NULL;
    context->frame_size = 0;
}

/*
 * デコード画像の幅・高さから、NV12フレーム用の内部バッファを確保する。
 *
 * context : バッファを保持するデコーダーコンテキスト。
 * width   : デコード画像の幅（ピクセル単位）。
 * height  : デコード画像の高さ（ピクセル単位）。偶数である必要がある。
 */
static vx_status avp_decode_allocate_buffers(AvpDecodeContext *context,
                                             uint32_t width,
                                             uint32_t height)
{
    size_t frame_size;

    if((width == 0u) || (height == 0u) || ((height & 1u) != 0u))
    {
        printf("avp_decode: invalid NV12 dimensions %ux%u\n", width, height);
        return VX_FAILURE;
    }

    frame_size = (size_t)width * (size_t)height * 3u / 2u;
    if(context->frame_size == frame_size)
    {
        return VX_SUCCESS;
    }

    avp_decode_free_buffers(context);

    context->working_frame = (uint8_t *)malloc(frame_size);

    if(context->working_frame == NULL)
    {
        printf("avp_decode: unable to allocate %zu bytes per frame buffer\n", frame_size);
        avp_decode_free_buffers(context);
        return VX_FAILURE;
    }

    context->frame_size = frame_size;
    context->width = width;
    context->height = height;

    return VX_SUCCESS;
}

/*
 * GStreamerサンプルのcapsからNV12形式と画像サイズを取得し、バッファサイズを確認する。
 *
 * context : 取得した画像レイアウトと内部バッファを保持するコンテキスト。
 * sample  : デコードされたGStreamerサンプル。capsの取得に使用する。
 * buffer  : sampleに含まれるデコード済みバッファ。サイズ確認に使用する。
 */
static vx_status avp_decode_update_layout(AvpDecodeContext *context,
                                          GstSample *sample,
                                          GstBuffer *buffer)
{
    GstCaps *caps;
    GstStructure *structure;
    const gchar *format;
    gint width = 0;
    gint height = 0;

    caps = gst_sample_get_caps(sample);
    if((caps == NULL) || (gst_caps_get_size(caps) == 0u))
    {
        printf("avp_decode: decoded sample has no caps\n");
        return VX_FAILURE;
    }

    structure = gst_caps_get_structure(caps, 0);
    format = gst_structure_get_string(structure, "format");
    if((format == NULL) || (strcmp(format, "NV12") != 0))
    {
        printf("avp_decode: expected NV12 output, received %s\n",
               (format != NULL) ? format : "unknown format");
        return VX_FAILURE;
    }

    // 入力したh264動画のwidthとheightを取得する（hennkou1の内容）
    // 同じwidthとheightを使用して、内部バッファを確保する（hennkou2の内容）
    if(!gst_structure_get_int(structure, "width", &width) ||
       !gst_structure_get_int(structure, "height", &height))
    {
        printf("avp_decode: decoded sample has no width/height caps\n");
        return VX_FAILURE;
    }

    if(avp_decode_allocate_buffers(context, (uint32_t)width, (uint32_t)height) != VX_SUCCESS)
    {
        return VX_FAILURE;
    }

    if(gst_buffer_get_size(buffer) < context->frame_size)
    {
        printf("avp_decode: buffer is too small (%zu, expected at least %zu)\n",
               gst_buffer_get_size(buffer), context->frame_size);
        return VX_FAILURE;
    }

    return VX_SUCCESS;
}

/*
 * GStreamerバッファをマッピングし、planeのstrideとoffsetを考慮して
 * パック形式のNV12バッファへコピーする。
 *
 * context     : 出力幅・高さ・フレームサイズを保持するコンテキスト。
 * buffer      : GStreamerから取得したデコード済みNV12バッファ。
 * destination : パック形式NV12を書き込む、確保済みの出力バッファ。
 */
static vx_status avp_decode_copy_buffer_to_nv12(AvpDecodeContext *context,
                                                GstBuffer *buffer,
                                                uint8_t *destination)
{
    GstMapInfo map;
    GstVideoMeta *video_meta;
    uint32_t row;
    uint8_t *source;

    if(!gst_buffer_map(buffer, &map, GST_MAP_READ))
    {
        printf("avp_decode: unable to map decoded buffer\n");
        return VX_FAILURE;
    }

    video_meta = gst_buffer_get_video_meta(buffer);

    if((video_meta != NULL) && (video_meta->n_planes >= 2u))
    {
        for(row = 0u; row < context->height; row++)
        {
            source = map.data + video_meta->offset[0] +
                     ((size_t)row * (size_t)video_meta->stride[0]);
            memcpy(destination + ((size_t)row * context->width), source, context->width);
        }

        for(row = 0u; row < (context->height / 2u); row++)
        {
            source = map.data + video_meta->offset[1] +
                     ((size_t)row * (size_t)video_meta->stride[1]);
            memcpy(destination + ((size_t)context->width * context->height) +
                   ((size_t)row * context->width), source, context->width);
        }
    }
    else if(map.size >= context->frame_size)
    {
        /*
         * GstVideoMetaが無い場合。
         *
         * 実機のv4l2h264dec（J721E）は、バッファの確保サイズだけを高さ64行
         * アラインで計算する一方（1280x720の出力に対して
         * map.size = 1280 * 768 * 3 / 2 = 1474560）、実データは
         * stride = width、UVプレーンは width * height の直後に置く
         * パック形式NV12として書き込む。実機で確認済み。
         * 末尾の余剰バイトは未使用のアロケーション余白である。
         *
         * したがって、先頭からframe_size分をそのままコピーする。
         * サイズが一致しないことを縦padding付きレイアウトと解釈してはならない。
         */
        memcpy(destination, map.data, context->frame_size);
    }
    else
    {
        /* パック形式NV12に必要なサイズに満たないバッファは扱えない */
        printf("avp_decode: buffer has no GstVideoMeta and size %zu is smaller than "
               "packed NV12 size %zu for %ux%u\n",
               map.size, context->frame_size, context->width, context->height);
        gst_buffer_unmap(buffer, &map);
        return VX_FAILURE;
    }

    gst_buffer_unmap(buffer, &map);
    return VX_SUCCESS;
}

/*
 * H.264ファイルからNV12を取得するGStreamerパイプラインを作成して起動する。
 *
 * context   : 作成したパイプラインとappsinkを保持するコンテキスト。
 * h264_path : 入力するH.264ファイルのパス。
 */
static vx_status avp_decode_create_pipeline(AvpDecodeContext *context,
                                            const char *h264_path)
{
    GstElement *source;
    GstElement *parser;
    GstElement *decoder;
    GstElement *caps_filter;
    GstCaps *raw_caps;
    GstStateChangeReturn state_result;

    // multifilesrcではなくfilesrcを使用する（hennkou3の内容）
    source = gst_element_factory_make("filesrc", "avp_h264_source");
    parser = gst_element_factory_make("h264parse", "avp_h264_parser");

    // v4l2h264decを使用する（hennkou5の内容）(sinkType ==3)
    decoder = gst_element_factory_make("v4l2h264dec", "avp_h264_decoder");
    caps_filter = gst_element_factory_make("capsfilter", "avp_nv12_filter");
    context->appsink = GST_APP_SINK(gst_element_factory_make("appsink", "avp_nv12_sink"));
    context->pipeline = gst_pipeline_new("avp_h264_pipeline");

    if((source == NULL) || (parser == NULL) || (decoder == NULL) ||
       (caps_filter == NULL) || (context->appsink == NULL) ||
       (context->pipeline == NULL))
    {
        printf("avp_decode: required GStreamer element is unavailable\n");
        if(context->pipeline != NULL)
        {
            gst_object_unref(context->pipeline);
            context->pipeline = NULL;
        }
        return VX_FAILURE;
    }

    g_object_set(source, "location", h264_path, NULL);
    g_object_set(context->appsink,
                 "drop", FALSE,
                 "wait-on-eos", TRUE,
                 "max-buffers", 4u,
                 "sync", FALSE,
                 NULL);

    raw_caps = gst_caps_new_simple("video/x-raw",
                                   "format", G_TYPE_STRING, "NV12",
                                   NULL);
    g_object_set(caps_filter, "caps", raw_caps, NULL);
    gst_caps_unref(raw_caps);

    gst_bin_add_many(GST_BIN(context->pipeline),
                     source, parser, decoder, caps_filter,
                     GST_ELEMENT(context->appsink), NULL);

    if(!gst_element_link_many(source, parser, decoder, caps_filter,
                              GST_ELEMENT(context->appsink), NULL))
    {
        printf("avp_decode: failed to link H.264 decode pipeline\n");
        gst_element_set_state(context->pipeline, GST_STATE_NULL);
        gst_object_unref(context->pipeline);
        context->pipeline = NULL;
        context->appsink = NULL;
        return VX_FAILURE;
    }

    state_result = gst_element_set_state(context->pipeline, GST_STATE_PLAYING);
    if(state_result == GST_STATE_CHANGE_FAILURE)
    {
        printf("avp_decode: failed to start H.264 decode pipeline\n");
        gst_element_set_state(context->pipeline, GST_STATE_NULL);
        gst_object_unref(context->pipeline);
        context->pipeline = NULL;
        context->appsink = NULL;
        return VX_FAILURE;
    }

    return VX_SUCCESS;
}

/*
 * appsinkから次のGStreamerサンプルを取得する。
 *
 * gst_app_sink_pull_sample()は無限に待つため使用しない。
 * AVP_DECODE_PULL_TIMEOUT_NSで打ち切るbounded pullを使用し、
 * EOS・timeout・GStreamerエラーを区別して表示する。
 *
 * context : サンプルを取得するappsinkを保持するデコーダーコンテキスト。
 * sample  : 取得したGstSampleのポインタを書き込む出力引数。
 *
 * 戻り値 : VX_SUCCESSならサンプル取得成功、AVP_DECODE_EOSなら終端、
 *          VX_FAILUREならtimeoutまたはデコードエラー。
 */
static vx_status avp_decode_pull_sample(AvpDecodeContext *context,
                                        GstSample **sample)
{
    *sample = gst_app_sink_try_pull_sample(context->appsink,
                                           AVP_DECODE_PULL_TIMEOUT_NS);
    if(*sample != NULL)
    {
        return VX_SUCCESS;
    }

    if(gst_app_sink_is_eos(context->appsink))
    {
        printf("avp_decode: end of H.264 stream\n");
        return AVP_DECODE_EOS;
    }

    /*
     * EOSでもサンプルでもない場合は、GStreamerのエラーとtimeoutを区別する。
     * バスにエラー・警告があればそれを表示し、無ければtimeoutとして扱う。
     */
    if(avp_decode_print_bus_error(context) == vx_true_e)
    {
        printf("avp_decode: decode pipeline reported an error\n");
    }
    else
    {
        printf("avp_decode: timed out after %u ms waiting for a decoded frame\n",
               (unsigned int)(AVP_DECODE_PULL_TIMEOUT_NS / GST_MSECOND));
    }

    return VX_FAILURE;
}

/*
 * 公開関数：H.264ファイルから次の1フレームをデコードし、NV12データを返す。
 * context_ptrは状態保持用、h264_pathは入力ファイル、nv12_data/nv12_size/
 * width/heightは出力値。
 */
vx_status avp_decode_h264_next_frame(AvpDecodeContext **context_ptr,
                                     const char *h264_path,
                                     const uint8_t **nv12_data,
                                     size_t *nv12_size,
                                     uint32_t *width,
                                     uint32_t *height)
{
    AvpDecodeContext *context;
    GstSample *sample = NULL;
    GstBuffer *buffer;
    vx_status status;

    // どのポインタが無くても弾く（NULLチェック）
    if((context_ptr == NULL) || (h264_path == NULL) ||
       (nv12_data == NULL) || (nv12_size == NULL) ||
       (width == NULL) || (height == NULL))
    {
        return VX_FAILURE;
    }

    if(*context_ptr == NULL)
    {
        context = (AvpDecodeContext *)calloc(1u, sizeof(AvpDecodeContext));
        if(context == NULL)
        {
            printf("avp_decode: unable to allocate decoder context\n");
            return VX_FAILURE;
        }

        if(!gst_is_initialized())
        {
            gst_init(NULL, NULL);
        }

        strncpy(context->h264_path, h264_path, APP_MAX_FILE_PATH - 1u);
        context->h264_path[APP_MAX_FILE_PATH - 1u] = '\0';
        status = avp_decode_create_pipeline(context, context->h264_path);
        if(status != VX_SUCCESS)
        {
            free(context);
            return status;
        }
        *context_ptr = context;
    }
    else
    {
        context = *context_ptr;
        if(strncmp(context->h264_path, h264_path, APP_MAX_FILE_PATH) != 0)
        {
            printf("avp_decode: H.264 path changed while decoder is active\n");
            return VX_FAILURE;
        }
    }

    status = avp_decode_pull_sample(context, &sample);
    if(status != VX_SUCCESS)
    {
        return status;
    }

    buffer = gst_sample_get_buffer(sample);
    if(buffer == NULL)
    {
        printf("avp_decode: sample has no buffer\n");
        gst_sample_unref(sample);
        return VX_FAILURE;
    }

    status = avp_decode_update_layout(context, sample, buffer);
    if(status == VX_SUCCESS)
    {
        status = avp_decode_copy_buffer_to_nv12(context, buffer,
                                                 context->working_frame);
    }
    gst_sample_unref(sample);
    if(status != VX_SUCCESS)
    {
        return status;
    }

    *nv12_data = context->working_frame;
    *nv12_size = context->frame_size;
    *width = context->width;
    *height = context->height;
    return VX_SUCCESS;
}

/*
 * 公開関数：GStreamerパイプラインと内部NV12バッファを解放する。
 * context_ptrは解放対象コンテキストのアドレスで、解放後はNULLになる。
 */
void avp_decode_release(AvpDecodeContext **context_ptr)
{
    AvpDecodeContext *context;

    if((context_ptr == NULL) || (*context_ptr == NULL))
    {
        return;
    }

    context = *context_ptr;
    if(context->pipeline != NULL)
    {
        gst_element_set_state(context->pipeline, GST_STATE_NULL);
        gst_object_unref(context->pipeline);
    }
    avp_decode_free_buffers(context);
    free(context);
    *context_ptr = NULL;
}

#else

struct AvpDecodeContext
{
    int unused;
};

vx_status avp_decode_h264_next_frame(AvpDecodeContext **context,
                                     const char *h264_path,
                                     const uint8_t **nv12_data,
                                     size_t *nv12_size,
                                     uint32_t *width,
                                     uint32_t *height)
{
    (void)context;
    (void)h264_path;
    (void)nv12_data;
    (void)nv12_size;
    (void)width;
    (void)height;
    printf("avp_decode: H.264 GStreamer decoder is supported only on Linux\n");
    return VX_FAILURE;
}

void avp_decode_release(AvpDecodeContext **context)
{
    (void)context;
}

#endif /* defined(LINUX) */

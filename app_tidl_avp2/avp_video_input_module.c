#include "avp_video_input_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The WSL/PC backend uses the ffmpeg executable rather than linking the
 * application against libav*.  This keeps the Concerto link step simple and
 * allows the same code to accept an elementary .h264 stream or a container
 * such as MP4.  ffmpeg must be installed in the WSL environment.
 */
#if defined(x86_64)

#if defined(_WIN32)
#define AVP_POPEN  _popen
#define AVP_PCLOSE _pclose
#else
#define AVP_POPEN  popen
#define AVP_PCLOSE pclose
#endif

#define AVP_VIDEO_COMMAND_SIZE (APP_MAX_FILE_PATH * 4)

typedef struct {
    FILE *pipe;
    char command[AVP_VIDEO_COMMAND_SIZE];
} AvpVideoPipe;

static vx_status avp_read_exact(FILE *pipe, void *buffer, size_t size)
{
    size_t total = 0U;
    unsigned char *dst = (unsigned char *)buffer;

    while(total < size)
    {
        size_t bytes = fread(dst + total, 1U, size - total, pipe);
        if(bytes == 0U)
        {
            return VX_FAILURE;
        }
        total += bytes;
    }

    return VX_SUCCESS;
}

static vx_status avp_append_shell_quoted(char *command,
                                         size_t command_size,
                                         size_t *used,
                                         const char *value)
{
    size_t i;

    if((*used + 1U) >= command_size)
    {
        return VX_FAILURE;
    }
    command[(*used)++] = '\'';

    for(i = 0U; value[i] != '\0'; i++)
    {
        /* Close, escape, and reopen a single-quoted shell string. */
        if(value[i] == '\'')
        {
            const char escaped[] = "'\\''";
            size_t escaped_len = sizeof(escaped) - 1U;
            if((*used + escaped_len) >= command_size)
            {
                return VX_FAILURE;
            }
            memcpy(&command[*used], escaped, escaped_len);
            *used += escaped_len;
        }
        else
        {
            if((*used + 1U) >= command_size)
            {
                return VX_FAILURE;
            }
            command[(*used)++] = value[i];
        }
    }

    if((*used + 1U) >= command_size)
    {
        return VX_FAILURE;
    }
    command[(*used)++] = '\'';
    command[*used] = '\0';

    return VX_SUCCESS;
}

static vx_status avp_copy_nv12_frame(FILE *pipe,
                                     vx_image output_image,
                                     vx_uint32 width,
                                     vx_uint32 height)
{
    vx_status status = VX_SUCCESS;
    vx_rectangle_t rect;
    vx_imagepatch_addressing_t image_addr;
    vx_map_id map_id;
    void *data_ptr;
    vx_uint32 image_width = 0U;
    vx_uint32 image_height = 0U;
    vx_uint32 row;

    vxQueryImage(output_image, VX_IMAGE_WIDTH, &image_width, sizeof(image_width));
    vxQueryImage(output_image, VX_IMAGE_HEIGHT, &image_height, sizeof(image_height));

    if((image_width != width) || (image_height != height))
    {
        printf("H.264 output size %ux%u does not match OpenVX input image %ux%u\n",
               width, height, image_width, image_height);
        return VX_FAILURE;
    }

    rect.start_x = 0U;
    rect.start_y = 0U;
    rect.end_x = width;
    rect.end_y = height;

    status = vxMapImagePatch(output_image,
                             &rect,
                             0U,
                             &map_id,
                             &image_addr,
                             &data_ptr,
                             VX_WRITE_ONLY,
                             VX_MEMORY_TYPE_HOST,
                             VX_NOGAP_X);
    if(status != VX_SUCCESS)
    {
        return status;
    }

    for(row = 0U; row < height; row++)
    {
        status = avp_read_exact(pipe,
                                data_ptr,
                                (size_t)width);
        if(status != VX_SUCCESS)
        {
            break;
        }
        data_ptr = (void *)((unsigned char *)data_ptr + image_addr.stride_y);
    }

    vxUnmapImagePatch(output_image, map_id);

    if(status != VX_SUCCESS)
    {
        return status;
    }

    rect.end_y = height / 2U;
    status = vxMapImagePatch(output_image,
                             &rect,
                             1U,
                             &map_id,
                             &image_addr,
                             &data_ptr,
                             VX_WRITE_ONLY,
                             VX_MEMORY_TYPE_HOST,
                             VX_NOGAP_X);
    if(status != VX_SUCCESS)
    {
        return status;
    }

    for(row = 0U; row < (height / 2U); row++)
    {
        status = avp_read_exact(pipe,
                                data_ptr,
                                (size_t)width);
        if(status != VX_SUCCESS)
        {
            break;
        }
        data_ptr = (void *)((unsigned char *)data_ptr + image_addr.stride_y);
    }

    vxUnmapImagePatch(output_image, map_id);
    return status;
}

#endif /* x86_64 */

vx_status avp_video_input_init(AvpVideoInputContext *ctx,
                               const char *file_name,
                               vx_uint32 width,
                               vx_uint32 height)
{
    if((ctx == NULL) || (file_name == NULL) || (width == 0U) || (height == 0U))
    {
        return VX_FAILURE;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->width = width;
    ctx->height = height;

#if defined(x86_64)
    {
        AvpVideoPipe *video_pipe;
        size_t used = 0U;
        int written;

        video_pipe = (AvpVideoPipe *)calloc(1U, sizeof(*video_pipe));
        if(video_pipe == NULL)
        {
            return VX_FAILURE;
        }

        written = snprintf(video_pipe->command,
                           sizeof(video_pipe->command),
                           "ffmpeg -hide_banner -loglevel error -nostdin "
                           "-i ");
        if((written < 0) || ((size_t)written >= sizeof(video_pipe->command)))
        {
            free(video_pipe);
            return VX_FAILURE;
        }
        used = (size_t)written;

        if(avp_append_shell_quoted(video_pipe->command,
                                   sizeof(video_pipe->command),
                                   &used,
                                   file_name) != VX_SUCCESS)
        {
            free(video_pipe);
            return VX_FAILURE;
        }

        written = snprintf(&video_pipe->command[used],
                           sizeof(video_pipe->command) - used,
                           " -map 0:v:0 -vf scale=%ux%u:flags=bilinear "
                           "-pix_fmt nv12 -f rawvideo -",
                           width,
                           height);
        if((written < 0) || ((size_t)written >= (sizeof(video_pipe->command) - used)))
        {
            free(video_pipe);
            return VX_FAILURE;
        }

        printf("Opening H.264 input through ffmpeg: %s\n", file_name);
        video_pipe->pipe = AVP_POPEN(video_pipe->command, "rb");
        if(video_pipe->pipe == NULL)
        {
            printf("Unable to start ffmpeg. Is ffmpeg installed and on PATH?\n");
            free(video_pipe);
            return VX_FAILURE;
        }

        ctx->decoder_handle = (void *)video_pipe;
        ctx->initialized = vx_true_e;
        return VX_SUCCESS;
    }
#else
    (void)file_name;
    printf("H.264 file input is currently implemented for x86_64/WSL only.\n");
    return VX_ERROR_NOT_SUPPORTED;
#endif
}

vx_status avp_video_input_read_frame(AvpVideoInputContext *ctx,
                                     vx_image output_image)
{
    if((ctx == NULL) || (output_image == NULL) || (ctx->initialized == vx_false_e))
    {
        return VX_FAILURE;
    }

#if defined(x86_64)
    {
        AvpVideoPipe *video_pipe = (AvpVideoPipe *)ctx->decoder_handle;
        vx_status status;

        if((video_pipe == NULL) || (video_pipe->pipe == NULL))
        {
            return VX_FAILURE;
        }

        status = avp_copy_nv12_frame(video_pipe->pipe,
                                     output_image,
                                     ctx->width,
                                     ctx->height);
        if(status == VX_SUCCESS)
        {
            ctx->frame_count++;
        }
        else
        {
            printf("H.264 decoder reached end of stream or returned an incomplete frame.\n");
        }
        return status;
    }
#else
    return VX_ERROR_NOT_SUPPORTED;
#endif
}

void avp_video_input_deinit(AvpVideoInputContext *ctx)
{
    if(ctx == NULL)
    {
        return;
    }

#if defined(x86_64)
    if(ctx->decoder_handle != NULL)
    {
        AvpVideoPipe *video_pipe = (AvpVideoPipe *)ctx->decoder_handle;
        if(video_pipe->pipe != NULL)
        {
            AVP_PCLOSE(video_pipe->pipe);
        }
        free(video_pipe);
    }
#endif

    memset(ctx, 0, sizeof(*ctx));
}

#ifndef AVP_VIDEO_INPUT_MODULE_H
#define AVP_VIDEO_INPUT_MODULE_H

#include "avp_common.h"

/*
 * H.264 input backend used by the Linux x86_64/WSL build.
 *
 * The backend keeps the encoded file on disk and decodes one NV12 frame at a
 * time.  The implementation intentionally hides the decoder-specific state
 * from the application so that a TI hardware-decoder backend can be added
 * later without changing the application-level interface.
 */
typedef struct {
    vx_uint32 width;
    vx_uint32 height;
    vx_uint64 frame_count;
    vx_bool initialized;
    void *decoder_handle;
} AvpVideoInputContext;

vx_status avp_video_input_init(AvpVideoInputContext *ctx,
                               const char *file_name,
                               vx_uint32 width,
                               vx_uint32 height);

vx_status avp_video_input_read_frame(AvpVideoInputContext *ctx,
                                     vx_image output_image);

void avp_video_input_deinit(AvpVideoInputContext *ctx);

#endif /* AVP_VIDEO_INPUT_MODULE_H */

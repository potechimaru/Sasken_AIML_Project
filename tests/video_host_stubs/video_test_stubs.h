#ifndef VIDEO_TEST_STUBS_H
#define VIDEO_TEST_STUBS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef _AVP_COMMON
#define _AVP_COMMON
#endif

typedef uint8_t  vx_uint8;
typedef uint32_t vx_uint32;
typedef uint64_t vx_uint64;
typedef int      vx_bool;
typedef size_t   vx_size;
typedef int      vx_status;
typedef int      vx_map_id;
typedef struct TestImage *vx_image;

typedef struct {
    vx_uint32 start_x;
    vx_uint32 start_y;
    vx_uint32 end_x;
    vx_uint32 end_y;
} vx_rectangle_t;

typedef struct {
    vx_uint32 stride_x;
    vx_uint32 stride_y;
} vx_imagepatch_addressing_t;

#define APP_MAX_FILE_PATH (256U)
#define vx_true_e (1)
#define vx_false_e (0)
#define VX_SUCCESS (0)
#define VX_FAILURE (-1)
#define VX_IMAGE_WIDTH (1)
#define VX_IMAGE_HEIGHT (2)
#define VX_WRITE_ONLY (1)
#define VX_MEMORY_TYPE_HOST (0)
#define VX_NOGAP_X (0)

FILE *_popen(const char *command, const char *mode);
int _pclose(FILE *pipe);

vx_status vxQueryImage(vx_image image, int attribute, void *value, vx_size size);
vx_status vxMapImagePatch(vx_image image, const vx_rectangle_t *rect,
                          vx_uint32 plane_index, vx_map_id *map_id,
                          vx_imagepatch_addressing_t *addr, void **ptr,
                          int usage, int mem_type, int flags);
vx_status vxUnmapImagePatch(vx_image image, vx_map_id map_id);

#endif

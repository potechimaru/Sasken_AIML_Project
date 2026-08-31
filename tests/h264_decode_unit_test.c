#include "video_host_stubs/video_test_stubs.h"
#include "../app_tidl_avp2/avp_video_input_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_WIDTH (1280U)
#define TEST_HEIGHT (720U)

/* Test-only compatibility wrappers for the Windows _popen branch on WSL. */
FILE *_popen(const char *command, const char *mode)
{
    (void)mode;
    return popen(command, "r");
}

int _pclose(FILE *pipe)
{
    return pclose(pipe);
}

struct TestImage {
    vx_uint32 width;
    vx_uint32 height;
    vx_uint8 *y;
    vx_uint8 *uv;
};

static void require_condition(int condition, const char *message)
{
    if(!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

vx_status vxQueryImage(vx_image image, int attribute, void *value, vx_size size)
{
    if((image == NULL) || (value == NULL) || (size != sizeof(vx_uint32)))
    {
        return VX_FAILURE;
    }
    if(attribute == VX_IMAGE_WIDTH)
    {
        *(vx_uint32 *)value = image->width;
        return VX_SUCCESS;
    }
    if(attribute == VX_IMAGE_HEIGHT)
    {
        *(vx_uint32 *)value = image->height;
        return VX_SUCCESS;
    }
    return VX_FAILURE;
}

vx_status vxMapImagePatch(vx_image image, const vx_rectangle_t *rect,
                          vx_uint32 plane_index, vx_map_id *map_id,
                          vx_imagepatch_addressing_t *addr, void **ptr,
                          int usage, int mem_type, int flags)
{
    (void)usage; (void)mem_type; (void)flags;
    if((image == NULL) || (rect == NULL) || (map_id == NULL) ||
       (addr == NULL) || (ptr == NULL) || (rect->start_x != 0U) ||
       (rect->end_x != image->width))
    {
        return VX_FAILURE;
    }
    addr->stride_x = 1U;
    addr->stride_y = image->width;
    *map_id = (vx_map_id)(plane_index + 1U);
    *ptr = (plane_index == 0U) ? (void *)image->y : (void *)image->uv;
    return (*ptr != NULL) ? VX_SUCCESS : VX_FAILURE;
}

vx_status vxUnmapImagePatch(vx_image image, vx_map_id map_id)
{
    (void)image; (void)map_id;
    return VX_SUCCESS;
}

static vx_image test_image_create(vx_uint32 width, vx_uint32 height)
{
    vx_image image = (vx_image)calloc(1U, sizeof(*image));
    if(image == NULL)
    {
        return NULL;
    }
    image->width = width;
    image->height = height;
    image->y = (vx_uint8 *)calloc((size_t)width * height, 1U);
    image->uv = (vx_uint8 *)calloc((size_t)width * (height / 2U), 1U);
    if((image->y == NULL) || (image->uv == NULL))
    {
        free(image->y); free(image->uv); free(image);
        return NULL;
    }
    return image;
}

static void test_image_destroy(vx_image image)
{
    if(image != NULL)
    {
        free(image->y); free(image->uv); free(image);
    }
}

static unsigned long checksum(const vx_uint8 *data, size_t size)
{
    size_t i;
    unsigned long value = 2166136261UL;
    for(i = 0U; i < size; i++)
    {
        value ^= data[i];
        value *= 16777619UL;
    }
    return value;
}

static void test_invalid_arguments(void)
{
    AvpVideoInputContext context;
    require_condition(avp_video_input_init(NULL, "input.h264", TEST_WIDTH, TEST_HEIGHT) == VX_FAILURE,
                      "invalid arguments: NULL context is rejected");
    require_condition(avp_video_input_init(&context, NULL, TEST_WIDTH, TEST_HEIGHT) == VX_FAILURE,
                      "invalid arguments: NULL path is rejected");
    require_condition(avp_video_input_init(&context, "input.h264", 0U, TEST_HEIGHT) == VX_FAILURE,
                      "invalid arguments: zero width is rejected");
}

static void test_h264_decode(const char *file_name)
{
    AvpVideoInputContext context;
    vx_image first_image = test_image_create(TEST_WIDTH, TEST_HEIGHT);
    vx_image second_image = test_image_create(TEST_WIDTH, TEST_HEIGHT);
    unsigned long first_checksum;
    unsigned long second_checksum;

    require_condition((first_image != NULL) && (second_image != NULL),
                      "decoder: test image allocation");
    require_condition(avp_video_input_init(&context, file_name, TEST_WIDTH, TEST_HEIGHT) == VX_SUCCESS,
                      "decoder: initialization");
    require_condition(context.initialized == vx_true_e,
                      "decoder: context is initialized");
    require_condition(avp_video_input_read_frame(&context, first_image) == VX_SUCCESS,
                      "decoder: first frame is readable");
    require_condition(context.frame_count == 1U,
                      "decoder: frame count after first frame");
    first_checksum = checksum(first_image->y, (size_t)TEST_WIDTH * TEST_HEIGHT);
    require_condition(avp_video_input_read_frame(&context, second_image) == VX_SUCCESS,
                      "decoder: second frame is readable");
    require_condition(context.frame_count == 2U,
                      "decoder: frame count after second frame");
    second_checksum = checksum(second_image->y, (size_t)TEST_WIDTH * TEST_HEIGHT);
    require_condition(first_checksum != second_checksum,
                      "decoder: consecutive frames are not identical");
    require_condition(checksum(first_image->uv, (size_t)TEST_WIDTH * (TEST_HEIGHT / 2U)) != 2166136261UL,
                      "decoder: first frame contains chroma data");

    avp_video_input_deinit(&context);
    require_condition(context.initialized == vx_false_e,
                      "decoder: deinitialization clears context");
    require_condition(context.decoder_handle == NULL,
                      "decoder: deinitialization releases decoder");
    test_image_destroy(first_image);
    test_image_destroy(second_image);
    printf("PASS: H.264 decode: two NV12 frames read, data differs, and cleanup succeeded\n");
}

int main(int argc, char **argv)
{
    const char *file_name = (argc > 1) ? argv[1] : "h264/06_fcw_drive_rec_dump.h264";
    test_invalid_arguments();
    printf("PASS: invalid argument handling\n");
    test_h264_decode(file_name);
    printf("All H.264 decode unit tests passed.\n");
    return EXIT_SUCCESS;
}

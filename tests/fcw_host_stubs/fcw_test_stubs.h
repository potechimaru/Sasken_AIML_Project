#ifndef FCW_TEST_STUBS_H
#define FCW_TEST_STUBS_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifndef _AVP_COMMON
#define _AVP_COMMON
#endif

typedef uint8_t  vx_uint8;
typedef uint32_t vx_uint32;
typedef uint64_t vx_uint64;
typedef int32_t  vx_int32;
typedef int      vx_bool;
typedef float    vx_float32;
typedef size_t   vx_size;
typedef int      vx_status;
typedef int      vx_map_id;
typedef void    *vx_tensor;

#define vx_true_e  (1)
#define vx_false_e (0)
#define VX_SUCCESS  (0)
#define VX_FAILURE  (-1)
#define VX_ERROR_INVALID_PARAMETERS (-2)
#define VX_ERROR_NOT_SUPPORTED (-3)
#define VX_READ_ONLY (0)
#define VX_MEMORY_TYPE_HOST (0)

struct sTIDL_IOBufDesc_t;
static inline vx_status tivxMapTensorPatch(vx_tensor tensor,
                                            vx_uint32 num_dims,
                                            const vx_size *start,
                                            const vx_size *sizes,
                                            vx_map_id *map_id,
                                            const vx_size *strides,
                                            void **ptr,
                                            int usage,
                                            int mem_type)
{
    (void)tensor; (void)num_dims; (void)start; (void)sizes;
    (void)map_id; (void)strides; (void)ptr; (void)usage; (void)mem_type;
    return VX_ERROR_NOT_SUPPORTED;
}

static inline vx_status tivxUnmapTensorPatch(vx_tensor tensor, vx_map_id map_id)
{
    (void)tensor; (void)map_id;
    return VX_SUCCESS;
}

#endif

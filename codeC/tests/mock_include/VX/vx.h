#ifndef TEST_MOCK_VX_H_
#define TEST_MOCK_VX_H_

#include <stdint.h>

typedef int32_t vx_int32;
typedef uint32_t vx_uint32;
typedef float vx_float32;
typedef int32_t vx_bool;
typedef int32_t vx_status;

#define vx_false_e (0)
#define vx_true_e (1)
#define VX_SUCCESS (0)
#define VX_FAILURE (-1)
#define VX_ERROR_INVALID_PARAMETERS (-2)
#define VX_ERROR_NO_RESOURCES (-3)
#define VX_ERROR_NOT_SUPPORTED (-4)

#endif /* TEST_MOCK_VX_H_ */

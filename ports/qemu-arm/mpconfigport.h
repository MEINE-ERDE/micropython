#include <stdint.h>

// options to control how MicroPython is built

#define MICROPY_ALLOC_PATH_MAX      (512)

#if defined(__ARM_ARCH_ISA_ARM)
#define MICROPY_EMIT_ARM            (1)
#define MICROPY_EMIT_INLINE_THUMB   (1)
#elif defined(__ARM_ARCH_ISA_THUMB)
#define MICROPY_EMIT_THUMB          (1)
#define MICROPY_EMIT_INLINE_THUMB   (1)
#define MICROPY_MAKE_POINTER_CALLABLE(p) ((void *)((mp_uint_t)(p) | 1))
#endif

#define MICROPY_MALLOC_USES_ALLOCATED_SIZE (1)
#define MICROPY_MEM_STATS           (1)
#define MICROPY_DEBUG_PRINTERS      (0)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_STACK_CHECK         (1)
#define MICROPY_HELPER_REPL         (0)
#define MICROPY_HELPER_LEXER_UNIX   (0)
#define MICROPY_ENABLE_SOURCE_LINE  (1)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_FLOAT)
#define MICROPY_CAN_OVERRIDE_BUILTINS (1)
#define MICROPY_WARNINGS            (1)
#define MICROPY_PY_ALL_SPECIAL_METHODS (1)
#define MICROPY_PY_REVERSE_SPECIAL_METHODS (1)
#define MICROPY_PY_ARRAY_SLICE_ASSIGN (1)
#define MICROPY_PY_BUILTINS_BYTES_HEX (1)
#define MICROPY_PY_BUILTINS_FROZENSET (1)
#define MICROPY_PY_BUILTINS_MEMORYVIEW (1)
#define MICROPY_PY_BUILTINS_POW3    (1)
#define MICROPY_PY_IO               (1)
#define MICROPY_PY_SYS_EXIT         (1)
#define MICROPY_PY_SYS_MAXSIZE      (1)
#define MICROPY_PY_SYS_PLATFORM     "qemu-arm"
#define MICROPY_PY_ERRNO            (1)
#define MICROPY_PY_BINASCII         (1)
#define MICROPY_PY_RANDOM           (1)
#define MICROPY_PY_UCTYPES          (1)
#define MICROPY_PY_JSON             (1)
#define MICROPY_PY_OS               (1)
#define MICROPY_PY_RE               (1)
#define MICROPY_PY_HEAPQ            (1)
#define MICROPY_PY_HASHLIB          (1)
#define MICROPY_PY_MACHINE          (1)
#define MICROPY_PY_MICROPYTHON_MEM_INFO (1)
#define MICROPY_USE_INTERNAL_PRINTF (1)
#define MICROPY_VFS                 (1)

// type definitions for the specific machine

#define MP_SSIZE_MAX (0x7fffffff)

#define UINT_FMT "%lu"
#define INT_FMT "%ld"

typedef int32_t mp_int_t; // must be pointer size
typedef uint32_t mp_uint_t; // must be pointer size
typedef long mp_off_t;

// We need to provide a declaration/definition of alloca()
#include <alloca.h>

#ifdef TEST
#include "shared/upytesthelper/upytesthelper.h"
#undef MP_PLAT_PRINT_STRN
#define MP_PLAT_PRINT_STRN(str, len) upytest_output(str, len)
#endif

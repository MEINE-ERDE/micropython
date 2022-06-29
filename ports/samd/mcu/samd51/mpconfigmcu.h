// Deinitions common to all SAMD51 boards
#include "samd51.h"

#define MICROPY_CONFIG_ROM_LEVEL        (MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES)

// MicroPython emitters
#define MICROPY_EMIT_THUMB              (1)
#define MICROPY_EMIT_INLINE_THUMB       (1)

#define MICROPY_FLOAT_IMPL              (MICROPY_FLOAT_IMPL_FLOAT)
#define MICROPY_PY_BUILTINS_COMPLEX     (0)
#define MICROPY_PY_MATH                 (1)
#define MP_NEED_LOG2                    (1)
#define MICROPY_PY_CMATH                (0)

#define MICROPY_PY_UOS_URANDOM          (1)
#define MICROPY_PY_URANDOM_SEED_INIT_FUNC (trng_random_u32())
unsigned long trng_random_u32(void);

// Due to a limitation in the TC counter for us, the ticks period is 2**29
#define MICROPY_PY_UTIME_TICKS_PERIOD   (0x20000000)

#define VFS_BLOCK_SIZE_BYTES            (1536) //

#define MICROPY_HW_UART_TXBUF           (1)

#define CPU_FREQ                        (120000000)
#define DFLL48M_FREQ                    (48000000)
#define MAX_CPU_FREQ                    (200000000)
#define DPLLx_REF_FREQ                  (32768)

#define NVIC_PRIORITYGROUP_4            ((uint32_t)0x00000003)
#define IRQ_PRI_PENDSV                  NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 7, 0)

static inline uint32_t raise_irq_pri(uint32_t pri) {
    uint32_t basepri = __get_BASEPRI();
    // If non-zero, the processor does not process any exception with a
    // priority value greater than or equal to BASEPRI.
    // When writing to BASEPRI_MAX the write goes to BASEPRI only if either:
    //   - Rn is non-zero and the current BASEPRI value is 0
    //   - Rn is non-zero and less than the current BASEPRI value
    pri <<= (8 - __NVIC_PRIO_BITS);
    __ASM volatile ("msr basepri_max, %0" : : "r" (pri) : "memory");
    return basepri;
}

// "basepri" should be the value returned from raise_irq_pri
static inline void restore_irq_pri(uint32_t basepri) {
    __set_BASEPRI(basepri);
}

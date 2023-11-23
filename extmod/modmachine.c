/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"

#if MICROPY_PY_MACHINE

#include "extmod/modmachine.h"
#include "shared/runtime/pyexec.h"

#if MICROPY_PY_MACHINE_DHT_READINTO
#include "drivers/dht/dht.h"
#endif

// The port must provide implementations of these low-level machine functions.

STATIC void mp_machine_idle(void);

// The port can provide additional machine-module implementation in this file.
#ifdef MICROPY_PY_MACHINE_INCLUDEFILE
#include MICROPY_PY_MACHINE_INCLUDEFILE
#endif

STATIC mp_obj_t machine_soft_reset(void) {
    pyexec_system_exit = PYEXEC_FORCED_EXIT;
    mp_raise_type(&mp_type_SystemExit);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_soft_reset_obj, machine_soft_reset);

STATIC mp_obj_t machine_idle(void) {
    mp_machine_idle();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_idle_obj, machine_idle);

STATIC const mp_rom_map_elem_t machine_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_machine) },

    // Memory access objects.
    { MP_ROM_QSTR(MP_QSTR_mem8), MP_ROM_PTR(&machine_mem8_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem16), MP_ROM_PTR(&machine_mem16_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem32), MP_ROM_PTR(&machine_mem32_obj) },

    // Reset related functions.
    { MP_ROM_QSTR(MP_QSTR_soft_reset), MP_ROM_PTR(&machine_soft_reset_obj) },

    // Power related functions.
    { MP_ROM_QSTR(MP_QSTR_idle), MP_ROM_PTR(&machine_idle_obj) },

    // Functions for bit protocols.
    #if MICROPY_PY_MACHINE_BITSTREAM
    { MP_ROM_QSTR(MP_QSTR_bitstream), MP_ROM_PTR(&machine_bitstream_obj) },
    #endif
    #if MICROPY_PY_MACHINE_DHT_READINTO
    { MP_ROM_QSTR(MP_QSTR_dht_readinto), MP_ROM_PTR(&dht_readinto_obj) },
    #endif
    #if MICROPY_PY_MACHINE_PULSE
    { MP_ROM_QSTR(MP_QSTR_time_pulse_us), MP_ROM_PTR(&machine_time_pulse_us_obj) },
    #endif

    // Classes for PinBase and Signal.
    #if MICROPY_PY_MACHINE_PIN_BASE
    { MP_ROM_QSTR(MP_QSTR_PinBase), MP_ROM_PTR(&machine_pinbase_type) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_Signal), MP_ROM_PTR(&machine_signal_type) },

    // Classes for software bus protocols.
    #if MICROPY_PY_MACHINE_SOFTI2C
    { MP_ROM_QSTR(MP_QSTR_SoftI2C), MP_ROM_PTR(&mp_machine_soft_i2c_type) },
    #endif
    #if MICROPY_PY_MACHINE_SOFTSPI
    { MP_ROM_QSTR(MP_QSTR_SoftSPI), MP_ROM_PTR(&mp_machine_soft_spi_type) },
    #endif

    // Classes for hardware peripherals.
    #if MICROPY_PY_MACHINE_ADC
    { MP_ROM_QSTR(MP_QSTR_ADC), MP_ROM_PTR(&machine_adc_type) },
    #endif
    #if MICROPY_PY_MACHINE_ADC_BLOCK
    { MP_ROM_QSTR(MP_QSTR_ADCBlock), MP_ROM_PTR(&machine_adc_block_type) },
    #endif
    #if MICROPY_PY_MACHINE_DAC
    { MP_ROM_QSTR(MP_QSTR_DAC), MP_ROM_PTR(&machine_dac_type) },
    #endif
    #if MICROPY_PY_MACHINE_I2C
    { MP_ROM_QSTR(MP_QSTR_I2C), MP_ROM_PTR(&machine_i2c_type) },
    #endif
    #if MICROPY_PY_MACHINE_I2S
    { MP_ROM_QSTR(MP_QSTR_I2S), MP_ROM_PTR(&machine_i2s_type) },
    #endif
    #if MICROPY_PY_MACHINE_PWM
    { MP_ROM_QSTR(MP_QSTR_PWM), MP_ROM_PTR(&machine_pwm_type) },
    #endif
    #if MICROPY_PY_MACHINE_SPI
    { MP_ROM_QSTR(MP_QSTR_SPI), MP_ROM_PTR(&machine_spi_type) },
    #endif
    #if MICROPY_PY_MACHINE_UART
    { MP_ROM_QSTR(MP_QSTR_UART), MP_ROM_PTR(&machine_uart_type) },
    #endif
    #if MICROPY_PY_MACHINE_WDT
    { MP_ROM_QSTR(MP_QSTR_WDT), MP_ROM_PTR(&machine_wdt_type) },
    #endif

    // A port can add extra entries to the module by defining the following macro.
    #ifdef MICROPY_PY_MACHINE_EXTRA_GLOBALS
    MICROPY_PY_MACHINE_EXTRA_GLOBALS
    #endif
};
STATIC MP_DEFINE_CONST_DICT(machine_module_globals, machine_module_globals_table);

const mp_obj_module_t mp_module_machine = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&machine_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_machine, mp_module_machine);

#endif // MICROPY_PY_MACHINE

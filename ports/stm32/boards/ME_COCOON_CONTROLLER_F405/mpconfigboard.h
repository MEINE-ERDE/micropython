// Basic board and MCU information
#define MICROPY_HW_BOARD_NAME "ME_COCOON_CONTROLLER_F405"
#define MICROPY_HW_MCU_NAME "STM32F405RG"

// Feature configuration
#define MICROPY_HW_HAS_SWITCH (0) // no need for pyb switches
#define MICROPY_HW_HAS_FLASH (1)
#define MICROPY_HW_HAS_MMA7660 (0)
#define MICROPY_HW_HAS_LCD (0)
#define MICROPY_HW_ENABLE_RNG (1)
#define MICROPY_HW_ENABLE_RTC (1)
#define MICROPY_HW_ENABLE_SERVO (0)
#define MICROPY_HW_ENABLE_DAC (1)
#define MICROPY_HW_ENABLE_USB (1)
#define MICROPY_HW_ENABLE_SDCARD (0)

#define MICROPY_PY_ONEWIRE (1)
#define MICROPY_PY_DS18X20 (1)

// HSE in BYPASS mode.
#define MICROPY_HW_CLK_USE_HSE (1)
#define MICROPY_HW_CLK_USE_HSI (0)
#define MICROPY_HW_CLK_USE_BYPASS (1)
// Clock configuration
#define MICROPY_HW_CLK_PLLM (12)
#define MICROPY_HW_CLK_PLLN (336)
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV2)
#define MICROPY_HW_CLK_PLLQ (7)
// #define MICROPY_HW_CLK_LAST_FREQ (1)

// The pyboard has a 32kHz crystal for the RTC
#define MICROPY_HW_RTC_USE_LSE (1)
#define MICROPY_HW_RTC_USE_US (0)
#define MICROPY_HW_RTC_USE_CALOUT (1)

#define MICROPY_HW_UART_REPL PYB_UART_3 // changed for avoiding overlay with 3 -> no wired out though
#define MICROPY_HW_UART_REPL_BAUD 115200

// UART configuration
#define MICROPY_HW_UART1_NAME "UART1"
#define MICROPY_HW_UART1_TX (pin_B6)
#define MICROPY_HW_UART1_RX (pin_B7)

#define MICROPY_HW_UART2_NAME "UART2"
#define MICROPY_HW_UART2_TX (pin_A2)
#define MICROPY_HW_UART2_RX (pin_A3)

#define MICROPY_HW_UART3_NAME "UART3"
#define MICROPY_HW_UART3_TX (pin_B10)
#define MICROPY_HW_UART3_RX (pin_B11)

#define MICROPY_HW_UART4_NAME "UART4"
#define MICROPY_HW_UART4_TX (pin_A0)
#define MICROPY_HW_UART4_RX (pin_A1)

#define MICROPY_HW_UART6_NAME "UART6"
#define MICROPY_HW_UART6_TX (pin_C6)
#define MICROPY_HW_UART6_RX (pin_C7)

// I2C configuration
#define MICROPY_HW_I2C1_NAME "I2C1"
#define MICROPY_HW_I2C1_SCL (pin_B6)
#define MICROPY_HW_I2C1_SDA (pin_B7)

#define MICROPY_HW_I2C2_NAME "I2C2"
#define MICROPY_HW_I2C2_SCL (pin_B10)
#define MICROPY_HW_I2C2_SDA (pin_B11)

// SPI configuration
#define MICROPY_HW_SPI1_NAME "SPI1"
#define MICROPY_HW_SPI1_NSS (pin_A4)
#define MICROPY_HW_SPI1_SCK (pin_A5)
#define MICROPY_HW_SPI1_MISO (pin_A6)
#define MICROPY_HW_SPI1_MOSI (pin_A7)

// USB configuration
#define MICROPY_HW_USB_FS (1)
#define MICROPY_HW_USB_VBUS_DETECT_PIN (pin_A9)
#define MICROPY_HW_USB_OTG_ID_PIN (pin_A10)

// LEDs configuration
#define MICROPY_HW_LED1 (pin_C6)
#define MICROPY_HW_LED_ON(pin) (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin) (mp_hal_pin_low(pin))

// User switch configuration
// #define MICROPY_HW_USRSW_PIN (pin_C3)
// #define MICROPY_HW_USRSW_PULL (GPIO_PULLUP)
// #define MICROPY_HW_USRSW_EXTI_MODE (GPIO_MODE_IT_FALLING)
// #define MICROPY_HW_USRSW_PRESSED (0)

// Bootloader configuration for I2C with Mboot
#define MBOOT_I2C_PERIPH_ID 1
#define MBOOT_I2C_SCL (pin_B8)
#define MBOOT_I2C_SDA (pin_B9)
#define MBOOT_I2C_ALTFUNC (4)

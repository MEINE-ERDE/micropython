import uasyncio as asyncio
import machine
from max31865 import Max31865
from lis2dw12 import LIS2DW12
import ubinascii
import time
import os

from utils import debug_print, DEBUG_ENABLED

# Configuration
SPI_BUS = 1
SPI_FREQ = 2000000
I2C_BUS = 1
I2C_FREQ = 400000
CS_PINS = ["PC0", "PC1", "PC2"]
UART_BUS = 3
UART_BAUDRATE = 9600
DATA_SEND_INTERVAL_SEC = 10  # seconds
ACCEL_THRESHOLD_G = 0.01
MIN_MESSAGE_INTERVAL = 1  # minimum seconds between messages

RESET_INTERVAL_MS = 15 * 60 * 1000  # 15 minutes in milliseconds

DISABLE_REPL = True  # disable the repl in order to activate UART


# Data format specification:
# Index  Field                   Example Value    Description
# [0]    machine_id             2861640a7db617b6 Unique device identifier
# [1]    protocol_version       CC01             Protocol version number
# [2]    humidity              74                Humidity reading
# [3]    weight                450.0             Weight measurement
# [4]    temperature_1         21.8              Temperature sensor 1
# [5]    temperature_2         22.1              Temperature sensor 2
# [6]    temperature_3         21.9              Temperature sensor 3
# [7]    temperature_onboard   23.5              Onboard temperature sensor from LIS2DW12
# [8]    x                     0.000              X-axis acceleration/orientation
# [9]    y                     0.000              Y-axis acceleration/orientation
# [10]   z                     1.000              Z-axis acceleration/orientation
#
# Example complete string:
# 2861640a7db617b6;CC01;74;450.0;21.8;22.1;21.9;23.5;0.000;0.000;1.000

# Initialize SPI
debug_print("Starting program initialization...")
debug_print("Initializing SPI...")
spi = machine.SPI(SPI_BUS, baudrate=SPI_FREQ, polarity=1, phase=1)

# Initialize MAX31865 sensors
debug_print("Initializing temperature sensors...")
sensors = [
    Max31865(spi, machine.Pin(cs_pin, machine.Pin.OUT, value=1)) for cs_pin in CS_PINS
]

# Initialize I2C and Accelerometer
debug_print("Initializing I2C...")
i2c = machine.I2C(I2C_BUS, freq=I2C_FREQ)
try:
    debug_print("Initializing accelerometer...")
    accel = LIS2DW12(i2c)
    # accel.calibrate() only measure, no need for calibration
except Exception as e:
    debug_print("Accelerometer initialization failed:", e)
    accel = None

# Initialize UART
debug_print("Initializing UART...")
uart = machine.UART(UART_BUS, baudrate=UART_BAUDRATE)

# Read Machine ID
debug_print("Reading machine ID...")


def get_machine_id():
    machine_id = ubinascii.hexlify(machine.unique_id()).decode()
    debug_print("Machine ID:", machine_id)
    return machine_id


machine_id = get_machine_id()
debug_print("Initialization complete.")

# Global state
last_sent_acceleration = {"x": 0.0, "y": 0.0, "z": 0.0}
last_message_time = 0  # track the last message time


async def reset_timer():
    debug_print("Starting reset timer (15 minutes)...")
    await asyncio.sleep_ms(RESET_INTERVAL_MS)
    debug_print("Performing scheduled reset...")
    machine.reset()


async def read_temperatures():
    temperatures = []
    for sensor in sensors:
        try:
            temp = sensor.temperature
            sensor.clear_faults()
            time.sleep_ms(2)
            temperatures.append(temp)
        except Exception as e:
            print("Error reading temperature:", e)
            temperatures.append(-100)

        sensor.clear_faults()
        time.sleep_ms(2)
    return temperatures


async def read_accelerometer():
    if accel:
        return accel.get_raw_acceleration()  # Returns x, y, z values
    else:
        return {"x": 0.0, "y": 0.0, "z": 0.0}


def calculate_checksum(message):
    return hex(sum(message.encode("ascii")) % 16)


class MessageOutput:
    def __init__(self, use_uart=True, uart_bus=3, baudrate=9600):
        self.use_uart = use_uart
        if use_uart:
            # Disable REPL if using UART
            try:
                # os.dupterm(None, 1)
                debug_print("REPL disabled")
            except:
                debug_print("Warning: Could not disable REPL")
            self.uart = machine.UART(uart_bus, baudrate=baudrate)
        else:
            self.uart = None

    def send(self, message, debug_info=None):
        if self.use_uart:
            self.uart.write(message.encode())
        else:
            print("MESSAGE:", message.strip())
            if debug_info:
                debug_print("DEBUG:", debug_info)


message_output = MessageOutput(
    use_uart=DISABLE_REPL, uart_bus=UART_BUS, baudrate=UART_BAUDRATE
)


async def send_data_stream():
    global last_message_time
    current_time = time.time()

    # Check if enough time has passed since last message
    if current_time - last_message_time < MIN_MESSAGE_INTERVAL:
        return None

    temperatures = await read_temperatures()
    acceleration = await read_accelerometer()
    onboard_temp = accel.read_temperature() if accel else 0.0

    message = (
        f"{machine_id};CC01;0;0.0;"
        f"{temperatures[0]:.1f};{temperatures[1]:.1f};{temperatures[2]:.1f};"
        f"{onboard_temp};"
        f"{acceleration['x']:.3f};{acceleration['y']:.3f};{acceleration['z']:.3f}"
    )

    checksum = calculate_checksum(message)
    combined = f"{message};{checksum}\n"

    debug_info = {
        "temperatures": temperatures,
        "acceleration": acceleration,
        "onboard_temp": onboard_temp,
        "checksum": checksum,
    }

    message_output.send(combined, debug_info)

    last_message_time = current_time
    return acceleration


async def main_loop():
    global last_sent_acceleration
    debug_print("Starting main data loop...")
    while True:
        debug_print("Sending data stream...")
        new_accel = await send_data_stream()
        if new_accel:  # Only update if message was actually sent
            last_sent_acceleration = new_accel
        await asyncio.sleep(DATA_SEND_INTERVAL_SEC)


async def monitor_angles():
    global last_sent_acceleration
    debug_print("Starting angle monitoring...")
    while True:
        debug_print("monitoring angles...")
        current_accel = await read_accelerometer()

        # Check if any axis changed more than threshold
        significant_change = any(
            abs(current_accel[axis] - last_sent_acceleration[axis]) > ACCEL_THRESHOLD_G
            for axis in ["x", "y", "z"]
        )

        if significant_change:
            new_accel = await send_data_stream()
            if new_accel:  # Only update if message was actually sent
                last_sent_acceleration = new_accel

        await asyncio.sleep(0.5)


async def main():
    debug_print("Starting main application...")
    await asyncio.gather(
        main_loop(),
        monitor_angles(),
        reset_timer(),  # Add the reset timer to the gathered tasks
    )


# Start the event loop
debug_print("Starting asyncio.run...")
try:
    asyncio.run(main())
except Exception as e:
    debug_print("Error:", e)

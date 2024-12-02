import machine
import time
import math
from utils import debug_print, DEBUG_ENABLED


class LIS2DW12:
    # Register addresses
    REG_CTRL1 = 0x20
    REG_CTRL4 = 0x23
    REG_OUT_X_L = 0x28
    REG_OUT_X_H = 0x29
    REG_OUT_Y_L = 0x2A
    REG_OUT_Y_H = 0x2B
    REG_OUT_Z_L = 0x2C
    REG_OUT_Z_H = 0x2D
    REG_STATUS_REG = 0x27
    REG_OUT_T_L = 0x0D
    REG_OUT_T_H = 0x0E
    REG_OUT_T = 0x26

    # Device ID
    WHO_AM_I = 0x44

    def __init__(self, i2c, addr=0x19):
        """
        Initialize the LIS2DW12 sensor.

        :param i2c: An instance of machine.I2C
        :param addr: I2C address of the LIS2DW12 (default: 0x19)
        """
        self.i2c = i2c
        self.addr = addr
        # Add offset variables
        self.offset_pitch = 0
        self.offset_roll = 0
        self._init_sensor()

    def _init_sensor(self):
        """
        Configure the sensor with default settings.
        """
        debug_print("Initializing sensor...")

        # First verify device ID before proceeding
        try:
            device_id = self.read_reg(
                0x0F
            )  # WHO_AM_I register is typically at 0x0F for LIS2DW12
            debug_print(f"Device ID: 0x{device_id:02x}")
            if device_id != 0x44:  # Expected WHO_AM_I value
                raise Exception(
                    f"Unexpected device ID: 0x{device_id:02x}, expected 0x44"
                )
        except Exception as e:
            debug_print(f"Failed to read WHO_AM_I register: {e}")
            raise

        # If we get here, device ID is correct, proceed with configuration
        try:
            # Reset the sensor
            self.write_reg(self.REG_CTRL1, 0x00)  # First disable
            time.sleep(0.1)
            self.write_reg(self.REG_CTRL1, 0x57)  # Enable XYZ, 400Hz

            # Set CTRL4 to enable high-resolution mode
            self.write_reg(self.REG_CTRL4, 0x00)

        except Exception as e:
            debug_print(f"Failed to configure sensor: {e}")
            raise

    def write_reg(self, reg, data):
        """
        Write a byte to a register.

        :param reg: Register address
        :param data: Byte to write
        """
        self.i2c.writeto_mem(self.addr, reg, bytes([data]))

    def read_reg(self, reg):
        """
        Read a byte from a register.

        :param reg: Register address
        :return: Byte read
        """
        return self.i2c.readfrom_mem(self.addr, reg, 1)[0]

    def read_acceleration(self):
        """
        Read acceleration data from X, Y, Z axes.

        :return: Tuple of (acc_x, acc_y, acc_z) in g
        """
        x_l = self.read_reg(self.REG_OUT_X_L)
        x_h = self.read_reg(self.REG_OUT_X_H)
        y_l = self.read_reg(self.REG_OUT_Y_L)
        y_h = self.read_reg(self.REG_OUT_Y_H)
        z_l = self.read_reg(self.REG_OUT_Z_L)
        z_h = self.read_reg(self.REG_OUT_Z_H)

        # Combine high and low bytes
        acc_x = (
            self._convert_to_signed(x_l, x_h) / 16384.0
        )  # Scale factor for ±2g range
        acc_y = self._convert_to_signed(y_l, y_h) / 16384.0
        acc_z = self._convert_to_signed(z_l, z_h) / 16384.0

        return acc_x, acc_y, acc_z

    def _convert_to_signed(self, low, high):
        """
        Convert two bytes to a signed integer.

        :param low: Low byte
        :param high: High byte
        :return: Signed integer
        """
        value = (high << 8) | low
        if value & (1 << 15):
            value = value - (1 << 16)
        return value

    def get_orientation(self):
        """
        Determine the orientation based on acceleration data.

        :return: String representing the orientation
        """
        acc_x, acc_y, acc_z = self.read_acceleration()

        if abs(acc_z) > abs(acc_x) and abs(acc_z) > abs(acc_y):
            if acc_z > 0:
                return "Up"
            else:
                return "Down"
        elif abs(acc_y) > abs(acc_x):
            if acc_y > 0:
                return "Forward"
            else:
                return "Backward"
        else:
            if acc_x > 0:
                return "Right"
            else:
                return "Left"

    def get_rotation_angles(self, apply_offset=True):
        """
        Calculate rotation angles with respect to horizontal surface.

        :param apply_offset: Whether to apply calibration offsets
        :return: Dictionary containing calibrated pitch and roll angles
        """
        acc_x, acc_y, acc_z = self.read_acceleration()

        # Calculate raw angles
        pitch = math.degrees(math.atan2(acc_y, math.sqrt(acc_x**2 + acc_z**2)))
        roll = math.degrees(math.atan2(-acc_x, acc_z))

        if apply_offset:
            # Apply calibration offsets
            pitch -= self.offset_pitch
            roll -= self.offset_roll

        return {"pitch": pitch, "roll": roll}

    def calibrate(self, samples=50):
        """
        Calibrate the sensor by establishing current position as 'zero' reference

        :param samples: Number of samples to average for calibration
        """
        debug_print("Calibrating... Keep the device in rest position...")
        x_sum = 0
        y_sum = 0
        z_sum = 0

        # Take multiple readings and average them
        for _ in range(samples):
            acc_x, acc_y, acc_z = self.read_acceleration()
            x_sum += acc_x
            y_sum += acc_y
            z_sum += acc_z
            time.sleep(0.02)

        # Average readings for reference position
        self.ref_x = x_sum / samples
        self.ref_y = y_sum / samples
        self.ref_z = z_sum / samples

        debug_print("Calibration complete!")
        debug_print(
            f"Reference values - X: {self.ref_x:.3f}g, Y: {self.ref_y:.3f}g, Z: {self.ref_z:.3f}g"
        )

    def get_rotation_angle(self):
        """
        Calculate rotation angle from the rest position.
        Returns angle in degrees (0-360°), where 0° is the calibrated position.
        """
        acc_x, acc_y, acc_z = self.read_acceleration()

        # Calculate angle using atan2 for full 360° range
        current_angle = math.degrees(math.atan2(acc_y, acc_x))
        ref_angle = math.degrees(math.atan2(self.ref_y, self.ref_x))

        # Calculate relative angle
        angle = current_angle - ref_angle

        # Normalize to 0-360° range
        angle = angle % 360

        return angle

    def get_raw_acceleration(self):
        """
        Get raw acceleration values in g.

        :return: Dictionary containing x, y, z acceleration values
        """
        acc_x, acc_y, acc_z = self.read_acceleration()
        return {"x": acc_x, "y": acc_y, "z": acc_z}

    def read_temperature(self, mode="12bit"):
        """
        Read temperature data from the sensor.

        :param mode: '12bit' or '8bit' temperature reading mode
        :return: Temperature in degrees Celsius
        """
        if mode == "12bit":
            # Read from OUT_T_L and OUT_T_H for 12-bit mode
            t_l = self.read_reg(self.REG_OUT_T_L)
            t_h = self.read_reg(self.REG_OUT_T_H)

            # Combine high and low bytes
            temp = self._convert_to_signed(t_l, t_h)
            # Convert to Celsius (assuming 16 LSB/°C as per typical specifications)
            return temp / 16.0
        else:
            # Read from OUT_T for 8-bit mode
            temp = self.read_reg(self.REG_OUT_T)
            # Convert to signed 8-bit value
            if temp & (1 << 7):
                temp = temp - (1 << 8)
            # Convert to Celsius (assuming 1 LSB/°C as per typical specifications)
            return float(temp)

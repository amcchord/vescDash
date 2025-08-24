# vescDash
BLE connected dashboard for VESC based speed controllers

## M5Stack Core2 Development Setup

This project includes development setup and boilerplate code for the M5Stack Core2, an ESP32-based development board with a 2.0" LCD touchscreen, built-in speaker, and various sensors.

### Prerequisites

- **Python 3.7+** with pip
- **PlatformIO** (will be auto-installed by the flash script if not present)
- **M5Stack Core2** device
- **USB-C cable** for connecting the M5Stack Core2

### Quick Start

1. **Clone and navigate to the project:**
   ```bash
   git clone <your-repo-url>
   cd vescDash
   ```

2. **Flash the Hello World example:**
   ```bash
   ./flash_m5stack.sh --monitor
   ```

   This will:
   - Install PlatformIO if not present
   - Build the project
   - Auto-detect your M5Stack Core2
   - Flash the firmware
   - Open the serial monitor

### Project Structure

```
vescDash/
├── src/
│   └── main.cpp              # Main application code
├── platformio.ini            # PlatformIO configuration
├── flash_m5stack.sh          # Automated flashing script
└── README.md                 # This file
```

### Development Environment

The project uses **PlatformIO** with the Arduino framework for ESP32. The configuration is optimized for the M5Stack Core2 with:

- **Platform:** ESP32 (Espressif)
- **Board:** M5Stack Core2
- **Framework:** Arduino
- **Libraries:** M5Core2, M5GFX
- **Upload Speed:** 921600 baud
- **Monitor Speed:** 115200 baud

### Hello World Application

The boilerplate code (`src/main.cpp`) demonstrates:

- **Display initialization** and text rendering
- **Button handling** for the three capacitive buttons
- **Serial communication** for debugging
- **Blinking text effect** to show the device is running

#### Features:
- Displays "Hello World!" on the LCD screen
- Text blinks every second
- Pressing buttons A, B, or C shows colored text
- Serial output for debugging

### Flashing Script Usage

The `flash_m5stack.sh` script provides automated building and flashing:

#### Basic usage:
```bash
./flash_m5stack.sh                    # Build and flash
./flash_m5stack.sh --monitor          # Build, flash, and open monitor
./flash_m5stack.sh --skip-build       # Only flash (skip build)
```

#### Advanced options:
```bash
./flash_m5stack.sh --port /dev/tty.usbserial-1234    # Use specific port
./flash_m5stack.sh --help                            # Show all options
```

#### Script features:
- **Auto-detection** of M5Stack Core2 port
- **Automatic PlatformIO installation** if needed
- **Idempotent operation** - safe to run multiple times
- **Colored output** for better readability
- **Error handling** with informative messages
- **Serial monitoring** option

### Manual PlatformIO Commands

If you prefer using PlatformIO directly:

```bash
# Install dependencies (first time only)
pio lib install

# Build the project
pio run -e m5stack-core2

# Upload to device
pio run -e m5stack-core2 --target upload

# Open serial monitor
pio device monitor --baud 115200

# Clean build files
pio run --target clean
```

### Troubleshooting

#### Port Detection Issues:
- **macOS:** Look for `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- **Linux:** Look for `/dev/ttyUSB*` or `/dev/ttyACM*`
- **Windows:** Look for `COM*` ports

#### Common Solutions:
1. **Device not detected:**
   - Check USB cable connection
   - Try a different USB port
   - Install CP2104 USB driver if needed

2. **Upload fails:**
   - Hold the reset button during upload
   - Try lowering upload speed in `platformio.ini`
   - Check that no serial monitor is open

3. **Build errors:**
   - Run `pio lib install` to ensure libraries are installed
   - Clean build with `pio run --target clean`

#### Getting Help:
- Check the [M5Stack Core2 documentation](https://docs.m5stack.com/en/core/core2)
- Visit the [PlatformIO documentation](https://docs.platformio.org/)
- Review the [M5Core2 library examples](https://github.com/m5stack/M5Core2)

### Next Steps

This setup provides a solid foundation for M5Stack Core2 development. You can now:

1. **Modify** `src/main.cpp` to implement your application
2. **Add libraries** to `platformio.ini` as needed
3. **Use the flash script** for quick development iterations
4. **Expand** the project structure as your application grows

### Hardware Specifications

**M5Stack Core2:**
- **MCU:** ESP32-D0WDQ6-V3
- **Flash:** 16MB
- **PSRAM:** 8MB
- **Display:** 2.0" IPS LCD (320×240)
- **Touch:** Capacitive touch screen
- **Audio:** NS4168 amplifier with speaker
- **Connectivity:** Wi-Fi, Bluetooth
- **Sensors:** IMU (MPU6886), RTC (BM8563)
- **Power:** 390mAh battery with charging

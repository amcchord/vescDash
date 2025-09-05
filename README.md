# VESC Dashboard

A real-time Bluetooth dashboard for VESC (Vedder Electronic Speed Controller) built for the M5Stack Core2. Monitor voltage, temperature, and other telemetry data wirelessly from your electric skateboard, scooter, or other VESC-powered vehicle.

![M5Stack Core2](https://img.shields.io/badge/Hardware-M5Stack%20Core2-blue)
![Platform](https://img.shields.io/badge/Platform-ESP32-green)
![Framework](https://img.shields.io/badge/Framework-Arduino-orange)
![License](https://img.shields.io/badge/License-MIT-yellow)

## Features

### Device Discovery
- **Smart Scanning**: Automatically discovers VESC devices with BLE modules
- **Device Selection**: Visual interface to select from multiple discovered devices
- **Signal Strength**: Displays RSSI for connection quality assessment
- **Auto-filtering**: Only shows devices with "VESC" in the name

### Robust Connectivity
- **Dual Address Support**: Attempts both RANDOM and PUBLIC BLE address types
- **Auto-reconnection**: Automatically reconnects if connection is lost
- **Connection Monitoring**: Real-time connection status with grace periods
- **Stabilization Delays**: Proper timing for reliable VESC BLE module communication

### Real-time Data Display
- **Large Voltage Display**: Prominent real-time battery voltage (V)
- **Temperature Monitoring**: FET temperature in Fahrenheit
- **Data Age Indicator**: Shows how recent the data is
- **M5Stack Battery**: Built-in battery level monitoring
- **No Data Warnings**: Clear indication when data becomes stale

### Intuitive Controls
- **Button A**: Rescan for devices / Disconnect
- **Button B**: Navigate device list / Manual data request
- **Button C**: Connect to selected device / Return to device list

### Configurable Settings
- **Scan Duration**: Adjustable BLE scan time (default: 3 seconds)
- **Data Refresh Rate**: Configurable telemetry update interval (default: 300ms)
- **Update Thresholds**: Prevent display flicker with smart update logic
- **Timeout Settings**: Customizable data staleness detection

## Hardware Requirements

### M5Stack Core2
- ESP32-based development board with built-in display
- Integrated battery and charging
- Three programmable buttons
- Built-in BLE capability

### VESC with BLE Module
- VESC motor controller (any version supporting UART)
- NRF51/NRF52-based BLE module configured for UART bridge mode
- BLE module must advertise Nordic UART Service (NUS)

## VESC Configuration

Before using the dashboard, ensure your VESC is properly configured:

1. **Enable UART Communication**:
   - Open VESC Tool
   - Navigate to **App Settings → UART**
   - Set baud rate to **115200**
   - **Enable UART**
   - Write configuration to VESC

2. **BLE Module Setup**:
   - Ensure BLE module is in UART bridge mode
   - Module should advertise Nordic UART Service
   - Verify connection between BLE module and VESC UART pins

## Installation

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- M5Stack Core2 connected via USB
- CP2104 USB drivers (usually auto-installed)

### Quick Setup
1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd vescDash
   ```

2. **Flash the device** (using included script):
   ```bash
   ./flash_m5stack.sh
   ```
   
   Or manually with PlatformIO:
   ```bash
   platformio run --target upload
   ```

3. **Monitor serial output** (optional):
   ```bash
   ./flash_m5stack.sh --monitor
   ```

### Build Configuration
The project uses PlatformIO with the following key settings:
- **Platform**: ESP32 (espressif32)
- **Board**: M5Stack Core2
- **Framework**: Arduino
- **Libraries**: M5Core2, M5GFX, ESP32 BLE Arduino

## Usage

### First Time Setup
1. **Power on** your M5Stack Core2
2. **Enable VESC**: Ensure your VESC and BLE module are powered and configured
3. **Scan for devices**: The dashboard will automatically scan on startup
4. **Select your VESC**: Use Button B to navigate, Button C to connect

### Normal Operation
1. **Monitor data**: Voltage and temperature update automatically every 300ms
2. **Check connection**: Status indicator shows data age and connection health
3. **Reconnection**: Device automatically reconnects if connection is lost
4. **Battery monitoring**: M5Stack battery level shown in corner

### Button Functions

| Button | Scanning Mode | Connected Mode |
|--------|---------------|----------------|
| **A** | Rescan for devices | Disconnect from VESC |
| **B** | Navigate device list | Manual data request |
| **C** | Connect to selected device | Return to device list |

## Configuration

Modify the constants at the top of `src/main.cpp` to customize behavior:

```cpp
// BLE Scan Settings
const int BLE_SCAN_TIME_SECONDS = 3;        // Scan duration

// VESC Data Refresh Settings  
const int VESC_DATA_REFRESH_MS = 300;       // Update interval
const int VESC_DATA_STALE_TIMEOUT_MS = 5000; // Data timeout

// Display Update Thresholds
const float VOLTAGE_UPDATE_THRESHOLD = 0.05;  // Voltage change threshold
const float TEMP_UPDATE_THRESHOLD = 0.1;      // Temperature change threshold
const int BATTERY_UPDATE_THRESHOLD = 1;       // Battery change threshold
```

## Protocol Details

The dashboard communicates with VESC using the standard VESC UART protocol over BLE:

### Nordic UART Service (NUS)
- **Service UUID**: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- **RX Characteristic**: `6e400002-b5a3-f393-e0a9-e50e24dcca9e` (Write to VESC)
- **TX Characteristic**: `6e400003-b5a3-f393-e0a9-e50e24dcca9e` (Notify from VESC)

### VESC Commands
- **COMM_GET_VALUES** (0x04): Requests telemetry data including voltage, current, temperature, RPM
- **COMM_ALIVE** (0x1E): Connection test command

For detailed protocol information, see `scratchpad/VESC_UART_Protocol.md`.

## Troubleshooting

### Connection Issues
- **"No VESC devices found"**: 
  - Verify VESC BLE module is powered and advertising
  - Check that device name contains "VESC"
  - Try reducing distance between devices

- **"Connection failed"**:
  - Ensure no other device is connected to the VESC
  - Verify UART is enabled in VESC configuration
  - Check BLE module is in UART bridge mode

### Data Issues
- **"No data" warning**:
  - Connection may be unstable - device will attempt reconnection
  - Verify VESC UART configuration
  - Check BLE module UART wiring

- **Incorrect values**:
  - Ensure VESC firmware is compatible (3.x or newer)
  - Verify baud rate is set to 115200
  - Check for packet corruption in serial monitor

### Development Debugging
Enable verbose logging by monitoring serial output:
```bash
platformio device monitor --baud 115200
```

## Development

### Project Structure
```
vescDash/
├── src/
│   └── main.cpp              # Main application code
├── scratchpad/
│   ├── Implementation_Summary.md    # Development notes
│   ├── BLE_Connection_Setup.md      # Connection guide
│   └── VESC_UART_Protocol.md        # Protocol documentation
├── platformio.ini            # Build configuration
├── flash_m5stack.sh          # Automated flash script
└── README.md                 # This file
```

### Key Components
- **BLE Scanner**: Discovers and connects to VESC devices
- **UART Protocol Handler**: Implements VESC communication protocol
- **Display Manager**: Manages UI updates and user interaction
- **Connection Manager**: Handles reconnection and connection monitoring

### Contributing
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly with actual VESC hardware
5. Submit a pull request

## Future Enhancements

- [ ] **Additional Telemetry**: Current, RPM, motor temperature, fault codes
- [ ] **Data Logging**: Store telemetry data to SD card
- [ ] **Graphical Display**: Real-time graphs and gauges
- [ ] **Multiple Device Support**: Connect to multiple VESCs simultaneously
- [ ] **Control Features**: Send commands to VESC (throttle, current limits)
- [ ] **Configuration Interface**: Modify VESC parameters from dashboard
- [ ] **WiFi Integration**: Remote monitoring and data upload

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- **VESC Project**: For the excellent motor controller and open protocol
- **M5Stack**: For the amazing development platform
- **ESP32 Community**: For comprehensive BLE libraries and examples
- **Benjamin Vedder**: Creator of the VESC project and UART protocol

## Support

For issues, questions, or contributions:
1. Check the [troubleshooting section](#troubleshooting)
2. Review the documentation in the `scratchpad/` directory
3. Open an issue on GitHub with detailed information about your setup

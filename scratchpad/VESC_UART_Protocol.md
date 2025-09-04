# VESC UART Protocol Documentation

## Overview
The VESC (Vedder Electronic Speed Controller) communicates over UART using a packet-based protocol. When using BLE, this protocol is tunneled through the Nordic UART Service (NUS).

## BLE Connection Setup

### Nordic UART Service UUIDs
```
Service UUID:        6e400001-b5a3-f393-e0a9-e50e24dcca9e
RX Characteristic:   6e400002-b5a3-f393-e0a9-e50e24dcca9e  (Write to VESC)
TX Characteristic:   6e400003-b5a3-f393-e0a9-e50e24dcca9e  (Notify from VESC)
```

### Connection Process
1. Scan for BLE devices advertising the Nordic UART Service
2. Connect to the device
3. Discover services and characteristics
4. Enable notifications on the TX characteristic
5. Write commands to the RX characteristic

### Important Connection Considerations
- MTU negotiation may be required (typical: 247 bytes)
- Connection interval should be set appropriately (7.5ms - 4s)
- Some VESC BLE modules require specific initialization sequences

## Packet Structure

### Packet Format
```
[Start Byte] [Length] [Payload] [CRC16] [Stop Byte]
```

### Start Byte
- `0x02`: Short packet (payload length ≤ 255 bytes)
- `0x03`: Long packet (payload length > 255 bytes)

### Length Field
- Short packet: 1 byte
- Long packet: 2 bytes (big-endian)

### CRC16
- Algorithm: CRC-16-CCITT
- Polynomial: 0x1021
- Initial value: 0x0000
- Covers only the payload

### Stop Byte
- Always `0x03`

## Command Types

### Common Commands
```c
typedef enum {
    COMM_FW_VERSION = 0,
    COMM_JUMP_TO_BOOTLOADER = 1,
    COMM_ERASE_NEW_APP = 2,
    COMM_WRITE_NEW_APP_DATA = 3,
    COMM_GET_VALUES = 4,
    COMM_SET_DUTY = 5,
    COMM_SET_CURRENT = 6,
    COMM_SET_CURRENT_BRAKE = 7,
    COMM_SET_RPM = 8,
    COMM_SET_POS = 9,
    COMM_SET_HANDBRAKE = 10,
    COMM_SET_DETECT = 11,
    COMM_SET_SERVO_POS = 12,
    COMM_SET_MCCONF = 13,
    COMM_GET_MCCONF = 14,
    COMM_GET_MCCONF_DEFAULT = 15,
    COMM_SET_APPCONF = 16,
    COMM_GET_APPCONF = 17,
    COMM_GET_APPCONF_DEFAULT = 18,
    COMM_SAMPLE_PRINT = 19,
    COMM_TERMINAL_CMD = 20,
    COMM_PRINT = 21,
    COMM_ROTOR_POSITION = 22,
    COMM_EXPERIMENT_SAMPLE = 23,
    COMM_DETECT_MOTOR_PARAM = 24,
    COMM_DETECT_MOTOR_R_L = 25,
    COMM_DETECT_MOTOR_FLUX_LINKAGE = 26,
    COMM_DETECT_ENCODER = 27,
    COMM_DETECT_HALL_FOC = 28,
    COMM_REBOOT = 29,
    COMM_ALIVE = 30,
    COMM_GET_DECODED_PPM = 31,
    COMM_GET_DECODED_ADC = 32,
    COMM_GET_DECODED_CHUK = 33,
    COMM_FORWARD_CAN = 34,
    COMM_SET_CHUCK_DATA = 35,
    COMM_CUSTOM_APP_DATA = 36,
    COMM_NRF_START_PAIRING = 37
} COMM_PACKET_ID;
```

## COMM_GET_VALUES Response

### Request Packet
```
[0x02] [0x01] [0x04] [CRC_H] [CRC_L] [0x03]
```
- Start: 0x02 (short packet)
- Length: 0x01 (1 byte payload)
- Command: 0x04 (COMM_GET_VALUES)
- CRC: 16-bit CRC of payload
- Stop: 0x03

### Response Structure (FW 3.x and newer)
**IMPORTANT**: The response packet structure is:
```
[Start] [Length] [Command] [Payload] [CRC16] [Stop]
```

The PAYLOAD (not the full packet) contains:

| Offset | Size | Type    | Description | Scaling |
|--------|------|---------|-------------|---------|
| 0      | 2    | int16   | temp_fet    | /10 °C  |
| 2      | 2    | int16   | temp_motor  | /10 °C  |
| 4      | 4    | int32   | current_motor | /100 A |
| 8      | 4    | int32   | current_in  | /100 A  |
| 12     | 4    | int32   | id          | /100 A  |
| 16     | 4    | int32   | iq          | /100 A  |
| 20     | 2    | int16   | duty_now    | /1000   |
| 22     | 4    | int32   | rpm         | 1 ERPM  |
| 26     | 2    | int16   | v_in        | /10 V   |
| 28     | 4    | int32   | amp_hours   | /10000 Ah |
| 32     | 4    | int32   | amp_hours_charged | /10000 Ah |
| 36     | 4    | int32   | watt_hours  | /10000 Wh |
| 40     | 4    | int32   | watt_hours_charged | /10000 Wh |
| 44     | 4    | int32   | tachometer  | 1 count |
| 48     | 4    | int32   | tachometer_abs | 1 count |
| 52     | 1    | uint8   | fault_code  | enum    |

**NOTE**: When parsing, the voltage (v_in) is at offset 26 in the PAYLOAD, which means:
- In the full packet: offset = 3 + 26 = 29 (after Start, Length, Command bytes)

### Important Notes
- All multi-byte values are big-endian
- The actual response will be wrapped in the packet structure
- Total payload size for COMM_GET_VALUES response: 53+ bytes

## CRC16 Implementation

```c
uint16_t crc16(uint8_t *data, uint32_t len) {
    uint16_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}
```

## Packet Assembly Example

### Sending COMM_GET_VALUES
```c
uint8_t packet[6];
packet[0] = 0x02;  // Start byte (short packet)
packet[1] = 0x01;  // Length (1 byte)
packet[2] = 0x04;  // COMM_GET_VALUES

uint16_t crc = crc16(&packet[2], 1);
packet[3] = (crc >> 8) & 0xFF;  // CRC high byte
packet[4] = crc & 0xFF;         // CRC low byte
packet[5] = 0x03;  // Stop byte
```

## Parsing Response

### Steps to Parse COMM_GET_VALUES Response
1. Wait for start byte (0x02 or 0x03)
2. Read length field
3. Read payload (length bytes)
4. Verify CRC
5. Check stop byte (0x03)
6. Extract values from payload based on offsets

### Example: Extract Voltage
```c
// Full packet structure for COMM_GET_VALUES response:
// [0x02] [Length] [0x04] [53+ bytes payload] [CRC_H] [CRC_L] [0x03]
//
// To extract voltage from the full packet:
// Voltage is at payload offset 26, so in full packet:
// Position = 3 (header) + 26 = index 29

uint8_t* packet = receivedData;
if (packet[0] == 0x02) {  // Start byte
    uint8_t length = packet[1];
    if (packet[2] == 0x04) {  // COMM_GET_VALUES response
        // Voltage at indices 29-30 (big-endian)
        int16_t v_in_raw = (packet[29] << 8) | packet[30];
        float voltage = v_in_raw / 10.0f;
    }
}
```

### Important Parsing Notes:
1. **The response does NOT echo the command** - it just contains the data
2. **Check packet length** - COMM_GET_VALUES response should have 50+ byte payload
3. **Verify CRC** before trusting data
4. **Handle fragmented packets** - BLE may split large packets

## Common Issues and Solutions

### Connection Issues
- **Problem**: Connection fails immediately
- **Solution**: Ensure proper service discovery, check if device requires pairing

### No Data Received
- **Problem**: Commands sent but no response
- **Solution**: Verify notification registration, check packet format

### Corrupted Data
- **Problem**: Received data doesn't match expected format
- **Solution**: Verify CRC, check for packet fragmentation over BLE

### MTU Issues
- **Problem**: Large packets get truncated
- **Solution**: Request MTU increase or handle packet fragmentation

## Testing Strategy

1. **Basic Connection Test**
   - Connect to device
   - Enable notifications
   - Send COMM_ALIVE (0x1E) - simplest command

2. **Data Request Test**
   - Send COMM_GET_VALUES
   - Verify packet structure
   - Parse and display voltage

3. **Continuous Communication**
   - Request data periodically
   - Monitor for disconnections
   - Implement reconnection logic

# VESC BLE Connection Setup Guide

## Prerequisites

### Hardware Requirements
- VESC with BLE module (NRF51/NRF52 based)
- BLE module must be configured for UART bridge mode
- UART on VESC must be enabled (App Settings -> UART)

### VESC Configuration
1. In VESC Tool, go to App Settings -> UART
2. Set baud rate to 115200
3. Enable UART
4. Write configuration

## BLE Connection Process

### Step 1: Scanning
```c
// Look for devices with these characteristics:
// - Device name contains "VESC"
// - Advertises Nordic UART Service UUID
// - Has strong RSSI (> -80 dBm for reliable connection)
```

### Step 2: Connection Setup
```c
// Critical connection parameters:
BLEClient* pClient = BLEDevice::createClient();

// IMPORTANT: Set callbacks before connecting
pClient->setClientCallbacks(new ClientCallbacks());

// Connect with proper address type
// Most VESC BLE modules use random address
bool connected = pClient->connect(bleAddress, BLE_ADDR_TYPE_RANDOM);
```

### Step 3: Service Discovery
```c
// Wait after connection before service discovery
delay(1000);  // Critical delay!

// Get Nordic UART Service
BLERemoteService* pService = pClient->getService(UART_SERVICE_UUID);
if (!pService) {
    // Try discovering services first
    pClient->discoverServices();
    delay(500);
    pService = pClient->getService(UART_SERVICE_UUID);
}
```

### Step 4: Characteristic Setup
```c
// Get characteristics with error checking
BLERemoteCharacteristic* pTX = pService->getCharacteristic(TX_CHAR_UUID);
BLERemoteCharacteristic* pRX = pService->getCharacteristic(RX_CHAR_UUID);

// CRITICAL: Register for notifications BEFORE any communication
if (pTX->canNotify()) {
    pTX->registerForNotify(notifyCallback);
}

// Some modules require descriptor write
BLERemoteDescriptor* pDescriptor = pTX->getDescriptor(BLEUUID((uint16_t)0x2902));
if (pDescriptor) {
    uint8_t notifyValue[] = {0x01, 0x00};
    pDescriptor->writeValue(notifyValue, 2, true);
}
```

### Step 5: MTU Negotiation (Optional but Recommended)
```c
// Request larger MTU for VESC packets
pClient->setMTU(517);  // Maximum MTU
```

## Common Connection Problems and Solutions

### Problem 1: Connection Fails Immediately
**Symptoms**: `connect()` returns false or times out

**Solutions**:
1. Try both address types:
   ```c
   pClient->connect(bleAddress, BLE_ADDR_TYPE_PUBLIC);
   // or
   pClient->connect(bleAddress, BLE_ADDR_TYPE_RANDOM);
   ```

2. Add connection callbacks:
   ```c
   class MyClientCallbacks : public BLEClientCallbacks {
       void onConnect(BLEClient* pclient) {
           Serial.println("Connected callback");
       }
       void onDisconnect(BLEClient* pclient) {
           Serial.println("Disconnected callback");
       }
   };
   ```

3. Ensure no other device is connected to VESC

### Problem 2: Services Not Found
**Symptoms**: `getService()` returns nullptr

**Solutions**:
1. Force service discovery:
   ```c
   std::map<std::string, BLERemoteService*>* services = pClient->getServices();
   for (auto& service : *services) {
       Serial.printf("Found service: %s\n", service.first.c_str());
   }
   ```

2. Add delays between operations:
   ```c
   pClient->connect(address);
   delay(2000);  // Wait for connection to stabilize
   pClient->discoverServices();
   delay(1000);  // Wait for discovery
   ```

### Problem 3: Notifications Not Working
**Symptoms**: No data received from VESC

**Solutions**:
1. Write to CCCD descriptor manually:
   ```c
   BLERemoteDescriptor* pDesc = pTX->getDescriptor(BLEUUID((uint16_t)0x2902));
   uint8_t notify[] = {0x01, 0x00};
   pDesc->writeValue(notify, 2);
   ```

2. Verify characteristic properties:
   ```c
   if (pTX->canNotify()) {
       Serial.println("TX can notify");
   }
   if (pTX->canIndicate()) {
       Serial.println("TX can indicate");
   }
   ```

## Working Connection Sequence

Based on successful implementations, this sequence works reliably:

```c
bool connectToVESC(String address) {
    Serial.println("Starting VESC connection...");
    
    // 1. Create client with callbacks
    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallbacks());
    
    // 2. Connect (try random address first)
    BLEAddress bleAddress(address);
    if (!pClient->connect(bleAddress, BLE_ADDR_TYPE_RANDOM)) {
        Serial.println("Failed with random address, trying public...");
        if (!pClient->connect(bleAddress, BLE_ADDR_TYPE_PUBLIC)) {
            Serial.println("Connection failed");
            return false;
        }
    }
    
    Serial.println("Connected to VESC");
    delay(2000);  // Critical stabilization delay
    
    // 3. Discover services
    Serial.println("Discovering services...");
    pClient->discoverServices();
    delay(1000);
    
    // 4. Get UART service
    BLERemoteService* pService = pClient->getService(
        BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    );
    
    if (!pService) {
        Serial.println("UART service not found");
        pClient->disconnect();
        return false;
    }
    
    // 5. Get characteristics
    BLERemoteCharacteristic* pTX = pService->getCharacteristic(
        BLEUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
    );
    BLERemoteCharacteristic* pRX = pService->getCharacteristic(
        BLEUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    );
    
    if (!pTX || !pRX) {
        Serial.println("Characteristics not found");
        pClient->disconnect();
        return false;
    }
    
    // 6. Enable notifications
    if (pTX->canNotify()) {
        pTX->registerForNotify(notifyCallback);
        
        // Also write descriptor
        BLERemoteDescriptor* pDesc = pTX->getDescriptor(
            BLEUUID((uint16_t)0x2902)
        );
        if (pDesc) {
            uint8_t notify[] = {0x01, 0x00};
            pDesc->writeValue(notify, 2);
        }
    }
    
    delay(500);  // Let notifications stabilize
    
    // 7. Test communication
    sendTestCommand();
    
    return true;
}
```

## Debugging Tips

### Enable Verbose BLE Logging
```c
// In setup()
esp_log_level_set("*", ESP_LOG_VERBOSE);
```

### Monitor BLE Events
```c
class MyClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        Serial.println("onConnect");
    }
    
    void onDisconnect(BLEClient* pclient) {
        Serial.println("onDisconnect");
        // Set flag to attempt reconnection
    }
    
    void onMTUChanged(BLEClient* pclient, uint16_t mtu) {
        Serial.printf("MTU changed to %d\n", mtu);
    }
};
```

### Test with Simple Commands
Start with COMM_ALIVE (0x1E) before COMM_GET_VALUES:
```c
uint8_t alivePacket[] = {0x02, 0x01, 0x1E, 0x40, 0x84, 0x03};
pRX->writeValue(alivePacket, 6);
```

## References
- VESC Tool source code
- Nordic UART Service specification
- ESP32 BLE Arduino examples
- Working implementations (m365 dash, etc.)

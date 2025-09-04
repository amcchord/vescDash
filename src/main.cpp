#include <M5Core2.h>
#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEScan.h"
#include "BLEAdvertisedDevice.h"
#include "BLEClient.h"
#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include <vector>
#include <string>

// Structure to store BLE device information
struct BLEDeviceInfo {
    String name;
    String address;
    int rssi;
};

std::vector<BLEDeviceInfo> discoveredDevices;
bool scanComplete = false;
int selectedDeviceIndex = 0;
bool isConnected = false;
float vescVoltage = 0.0;
float vescFetTemp = 0.0;
unsigned long lastVoltageUpdate = 0;

// Display update tracking to prevent flicker
float lastDisplayedVoltage = -1.0;
float lastDisplayedFetTemp = -1.0;
int lastBatteryLevel = -1;
bool needsFullRedraw = true;
String lastStatusText = "";

// VESC communication now handled by VescUart library

// Nordic UART Service UUIDs
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID charUUID_RX("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID charUUID_TX("6e400003-b5a3-f393-e0a9-e50e24dcca9e");

// VESC UART Communication Constants
#define COMM_GET_VALUES 4
#define COMM_ALIVE 30
#define VESC_PACKET_START 2
#define VESC_PACKET_STOP 3

// BLE Connection variables
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristicTX = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristicRX = nullptr;

// Buffer for assembling fragmented packets
std::vector<uint8_t> rxBuffer;
bool waitingForPacket = false;

// Callback class for BLE scan results
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        // Only add devices with "VESC" in the name
        if (advertisedDevice.haveName()) {
            String deviceName = advertisedDevice.getName().c_str();
            deviceName.toUpperCase();
            if (deviceName.indexOf("VESC") != -1) {
                BLEDeviceInfo device;
                device.name = advertisedDevice.getName().c_str();
                device.address = advertisedDevice.getAddress().toString().c_str();
                device.rssi = advertisedDevice.getRSSI();
                discoveredDevices.push_back(device);
                Serial.printf("Found VESC device: %s (%s) RSSI: %d\n", 
                             device.name.c_str(), device.address.c_str(), device.rssi);
            }
        }
    }
};

// CRC16 calculation for VESC packets
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

// Send VESC packet
void sendVESCPacket(uint8_t command) {
    if (!pRemoteCharacteristicRX || !isConnected) return;
    
    uint8_t packet[6];
    packet[0] = VESC_PACKET_START;
    packet[1] = 1; // Length
    packet[2] = command;
    
    uint16_t crc = crc16(&packet[2], 1);
    packet[3] = (crc >> 8) & 0xFF;
    packet[4] = crc & 0xFF;
    packet[5] = VESC_PACKET_STOP;
    
    pRemoteCharacteristicRX->writeValue(packet, 6);
    Serial.printf("Sent VESC packet: command %d\n", command);
}

// Parse VESC response and extract voltage
void parseVESCResponse(uint8_t* data, size_t length) {
    if (length < 6) return; // Too short to be valid
    
    Serial.print("Raw data: ");
    for (int i = 0; i < length && i < 64; i++) { // Limit output for readability
        Serial.printf("%02X ", data[i]);
    }
    if (length > 64) Serial.print("...");
    Serial.printf(" (len=%d)\n", length);
    
    // Look for VESC packet structure
    if (data[0] == VESC_PACKET_START) {
        Serial.println("Found VESC packet start byte (0x02)");
        
        // Get payload length
        uint8_t payloadLength = data[1];
        Serial.printf("Payload length: %d bytes\n", payloadLength);
        
        // COMM_GET_VALUES response doesn't echo the command byte
        // The payload starts directly after the length byte
        // Expected minimum payload for COMM_GET_VALUES is around 50-60 bytes
        
        if (payloadLength >= 50 && length >= payloadLength + 5) {
            Serial.println("Packet size looks like COMM_GET_VALUES response");
            
            // The packet has a COMM_GET_VALUES byte at position 2
            // The actual data payload starts at position 3
            // Voltage is at offset 26 in the data payload
            // So in full packet: position = 3 + 26 = 29
            
            if (length >= 31) { // Need at least 31 bytes to read voltage
                // Let's debug the packet structure more carefully
                Serial.printf("Packet structure: Start=0x%02X Len=%d Cmd=0x%02X\n", 
                             data[0], data[1], data[2]);
                
                // Print key data positions for debugging
                Serial.printf("Data at key positions:\n");
                for (int i = 3; i < 35 && i < length; i += 2) {
                    Serial.printf("  [%d-%d]: 0x%02X%02X = %d\n", 
                                 i, i+1, data[i], data[i+1], 
                                 (int16_t)((data[i] << 8) | data[i+1]));
                }
                
                // Voltage is at position 29-30 (confirmed from testing)
                int voltageIndex = 29;
                int16_t voltageRaw = (data[voltageIndex] << 8) | data[voltageIndex + 1];
                vescVoltage = voltageRaw / 10.0;
                
                // FET temp is at position 3-4
                int16_t tempFetRaw = (data[3] << 8) | data[4];
                vescFetTemp = tempFetRaw / 10.0;
                
                Serial.printf("Voltage: %.1fV (raw: 0x%04X = %d)\n", 
                             vescVoltage, voltageRaw & 0xFFFF, voltageRaw);
                Serial.printf("FET Temp: %.1f°C (%.1f°F)\n", 
                             vescFetTemp, (vescFetTemp * 9.0 / 5.0) + 32.0);
                lastVoltageUpdate = millis();
                
                // Also check motor temp
                int16_t tempMotor = (data[5] << 8) | data[6];
                Serial.printf("Motor Temp: %.1f°C\n", tempMotor/10.0);
            } else {
                Serial.printf("Packet too short for voltage data (need 31, got %d)\n", length);
            }
        } else if (data[2] == COMM_ALIVE) {
            Serial.println("Received COMM_ALIVE response");
        } else {
            Serial.printf("Unknown or incomplete packet (payload len=%d, total len=%d)\n", 
                         payloadLength, length);
        }
    } else {
        Serial.printf("No start byte found (got 0x%02X, expected 0x02)\n", data[0]);
    }
}

// BLE notification callback
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.printf("BLE notification: %d bytes\n", length);
    
    // Add data to buffer
    for (size_t i = 0; i < length; i++) {
        rxBuffer.push_back(pData[i]);
    }
    
    // Try to find and parse complete packets
    while (rxBuffer.size() >= 6) { // Minimum packet size
        // Look for start byte
        if (rxBuffer[0] != VESC_PACKET_START) {
            // Remove bytes until we find a start byte or buffer is empty
            while (!rxBuffer.empty() && rxBuffer[0] != VESC_PACKET_START) {
                rxBuffer.erase(rxBuffer.begin());
            }
            if (rxBuffer.empty()) break;
        }
        
        // Check if we have enough data for the length byte
        if (rxBuffer.size() < 2) break;
        
        uint8_t payloadLength = rxBuffer[1];
        size_t totalPacketLength = 2 + payloadLength + 3; // Start + Length + Payload + CRC(2) + Stop
        
        // Wait for complete packet
        if (rxBuffer.size() < totalPacketLength) {
            Serial.printf("Waiting for complete packet (have %d, need %d)\n", 
                         rxBuffer.size(), totalPacketLength);
            break;
        }
        
        // Check for stop byte
        if (rxBuffer[totalPacketLength - 1] == VESC_PACKET_STOP) {
            // We have a complete packet!
            std::vector<uint8_t> packet(rxBuffer.begin(), rxBuffer.begin() + totalPacketLength);
            parseVESCResponse(packet.data(), packet.size());
            
            // Remove processed packet from buffer
            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + totalPacketLength);
        } else {
            // Invalid packet, remove start byte and try again
            Serial.println("Invalid packet (no stop byte), searching for next...");
            rxBuffer.erase(rxBuffer.begin());
        }
    }
}

// BLE Client callbacks
class MyClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        Serial.println("BLE Client Connected");
    }
    
    void onDisconnect(BLEClient* pclient) {
        Serial.println("BLE Client Disconnected");
        isConnected = false;
    }
};

// Connect to selected VESC device
bool connectToVESC(int deviceIndex) {
    if (deviceIndex >= discoveredDevices.size()) return false;
    
    BLEDeviceInfo& device = discoveredDevices[deviceIndex];
    Serial.printf("Connecting to VESC: %s (%s)\n", device.name.c_str(), device.address.c_str());
    
    // Clean up any existing connection
    if (pClient) {
        delete pClient;
        pClient = nullptr;
    }
    
    // Create BLE client with callbacks
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallbacks());
    Serial.println("BLE client created with callbacks");
    
    // Connect to the BLE server - try random address first (most common for VESC)
    BLEAddress bleAddress(device.address.c_str());
    Serial.printf("Attempting connection to %s with RANDOM address type...\n", device.address.c_str());
    
    if (!pClient->connect(bleAddress, BLE_ADDR_TYPE_RANDOM)) {
        Serial.println("Failed with RANDOM address, trying PUBLIC...");
        if (!pClient->connect(bleAddress, BLE_ADDR_TYPE_PUBLIC)) {
            Serial.println("Failed to connect to VESC BLE device");
            delete pClient;
            pClient = nullptr;
            return false;
        }
    }
    
    Serial.println("Connected to VESC BLE device");
    delay(2000); // Critical stabilization delay for VESC BLE modules
    
    // Get the Nordic UART service
    Serial.println("Getting UART service...");
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find Nordic UART service");
        Serial.println("Listing all available services:");
        std::map<std::string, BLERemoteService*>* serviceMap = pClient->getServices();
        for (auto const& service : *serviceMap) {
            Serial.printf("  Found service: %s\n", service.first.c_str());
        }
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        return false;
    }
    
    Serial.println("Found Nordic UART service");
    
    // Get the TX characteristic (for receiving data from VESC)
    Serial.println("Getting TX characteristic...");
    pRemoteCharacteristicTX = pRemoteService->getCharacteristic(charUUID_TX);
    if (pRemoteCharacteristicTX == nullptr) {
        Serial.println("Failed to find TX characteristic");
        pClient->disconnect();
        return false;
    }
    
    // Get the RX characteristic (for sending data to VESC)
    Serial.println("Getting RX characteristic...");
    pRemoteCharacteristicRX = pRemoteService->getCharacteristic(charUUID_RX);
    if (pRemoteCharacteristicRX == nullptr) {
        Serial.println("Failed to find RX characteristic");
        pClient->disconnect();
        return false;
    }
    
    Serial.println("Found both characteristics");
    
    // Register for notifications from TX characteristic
    if (pRemoteCharacteristicTX->canNotify()) {
        Serial.println("Registering for notifications...");
        pRemoteCharacteristicTX->registerForNotify(notifyCallback);
        
        // Also write to the CCCD descriptor to ensure notifications are enabled
        Serial.println("Writing to CCCD descriptor...");
        BLERemoteDescriptor* pDescriptor = pRemoteCharacteristicTX->getDescriptor(BLEUUID((uint16_t)0x2902));
        if (pDescriptor) {
            uint8_t notifyValue[] = {0x01, 0x00}; // Enable notifications
            pDescriptor->writeValue(notifyValue, 2, true);
            Serial.println("CCCD descriptor written");
        } else {
            Serial.println("Warning: CCCD descriptor not found");
        }
        
        Serial.println("Notifications enabled");
    } else {
        Serial.println("TX characteristic cannot notify");
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        return false;
    }
    
    delay(1000); // Allow notification setup to complete
    
    isConnected = true;
    Serial.println("VESC connection fully established");
    
    // Test connection with COMM_ALIVE first (simpler command)
    Serial.println("Testing connection with COMM_ALIVE...");
    sendVESCPacket(COMM_ALIVE);
    delay(500);
    
    // Then request voltage data
    Serial.println("Requesting initial voltage data...");
    sendVESCPacket(COMM_GET_VALUES);
    
    return true;
}

void displayDeviceList() {
    static int lastSelectedIndex = -1;
    
    // Only do full redraw when needed
    if (needsFullRedraw) {
        M5.Lcd.fillScreen(BLACK);
        needsFullRedraw = false;
        lastSelectedIndex = -1; // Force redraw of all items
    }
    
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(10, 10);
    
    if (discoveredDevices.empty()) {
        M5.Lcd.println("No VESC devices found");
        M5.Lcd.setCursor(10, 40);
        M5.Lcd.println("Press A to rescan");
    } else {
        M5.Lcd.printf("Found %d VESC devices:\n", discoveredDevices.size());
        
        int yPos = 40;
        for (int i = 0; i < discoveredDevices.size() && i < 6; i++) {
            // Only redraw if this item or the selection changed
            if (i == selectedDeviceIndex || i == lastSelectedIndex || lastSelectedIndex == -1) {
                // Clear the area for this item
                M5.Lcd.fillRect(10, yPos, 300, 35, BLACK);
                
                M5.Lcd.setCursor(10, yPos);
                M5.Lcd.setTextSize(1);
                
                // Highlight selected device
                if (i == selectedDeviceIndex) {
                    M5.Lcd.setTextColor(BLACK, WHITE);
                    M5.Lcd.printf("> %d. %s\n", i + 1, discoveredDevices[i].name.c_str());
                    M5.Lcd.setTextColor(WHITE, BLACK);
                } else {
                    M5.Lcd.printf("  %d. %s\n", i + 1, discoveredDevices[i].name.c_str());
                }
                
                M5.Lcd.setCursor(20, yPos + 15);
                M5.Lcd.printf("   %s (RSSI: %d)\n", discoveredDevices[i].address.c_str(), discoveredDevices[i].rssi);
            }
            yPos += 35;
        }
        
        lastSelectedIndex = selectedDeviceIndex;
        
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(10, 200);
        M5.Lcd.println("A:Rescan B:Up/Down C:Connect");
    }
}

void displayVoltage() {
    // Only do full redraw when needed
    if (needsFullRedraw) {
        M5.Lcd.fillScreen(BLACK);
        
        // Draw static elements
        // Small "VESC Connected" at top
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(10, 10);
        M5.Lcd.println("VESC Connected");
        
        // Button labels at bottom
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.setCursor(10, 220);
        M5.Lcd.println("A:Disconnect  B:Request  C:Back");
        
        needsFullRedraw = false;
        lastDisplayedVoltage = -1.0; // Force voltage update
        lastDisplayedFetTemp = -1.0; // Force temp update
        lastBatteryLevel = -1; // Force battery update
        lastStatusText = ""; // Force status update
    }
    
    // Update voltage - large and centered
    if (abs(vescVoltage - lastDisplayedVoltage) > 0.05) { // Only update if change > 0.05V
        // Clear voltage area
        M5.Lcd.fillRect(0, 70, 320, 60, BLACK);
        
        // Calculate center position for voltage text
        M5.Lcd.setTextSize(6);
        M5.Lcd.setTextColor(GREEN, BLACK);
        String voltageStr = String(vescVoltage, 1) + "V";
        int textWidth = voltageStr.length() * 36; // Approximate width per char at size 6
        int xPos = (320 - textWidth) / 2;
        M5.Lcd.setCursor(xPos, 80);
        M5.Lcd.print(voltageStr);
        
        lastDisplayedVoltage = vescVoltage;
    }
    
    // Update FET temperature in Fahrenheit
    float fetTempF = (vescFetTemp * 9.0 / 5.0) + 32.0;
    if (abs(vescFetTemp - lastDisplayedFetTemp) > 0.5) { // Only update if change > 0.5°C
        // Clear temp area
        M5.Lcd.fillRect(0, 140, 320, 30, BLACK);
        
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(YELLOW, BLACK);
        String tempStr = "FET: " + String(fetTempF, 1) + "°F";
        int textWidth = tempStr.length() * 12; // Approximate width per char at size 2
        int xPos = (320 - textWidth) / 2;
        M5.Lcd.setCursor(xPos, 145);
        M5.Lcd.print(tempStr);
        
        lastDisplayedFetTemp = vescFetTemp;
    }
    
    // Update M5Stack battery level in lower right
    int batteryLevel = M5.Axp.GetBatteryLevel();
    if (abs(batteryLevel - lastBatteryLevel) > 2 || lastBatteryLevel == -1) { // Update if change > 2%
        // Clear battery area (lower right)
        M5.Lcd.fillRect(220, 195, 100, 20, BLACK);
        
        M5.Lcd.setTextSize(1);
        // Color based on battery level
        if (batteryLevel > 60) {
            M5.Lcd.setTextColor(GREEN, BLACK);
        } else if (batteryLevel > 20) {
            M5.Lcd.setTextColor(YELLOW, BLACK);
        } else {
            M5.Lcd.setTextColor(RED, BLACK);
        }
        
        M5.Lcd.setCursor(240, 200);
        M5.Lcd.printf("M5: %d%%", batteryLevel);
        
        lastBatteryLevel = batteryLevel;
    }
    
    // Update status text (data age) in lower left
    String statusText;
    unsigned long timeSinceUpdate = millis() - lastVoltageUpdate;
    if (timeSinceUpdate > 5000) {
        statusText = "No data";
    } else {
        statusText = String(timeSinceUpdate / 1000) + "s ago";
    }
    
    // Only update status if changed
    if (statusText != lastStatusText) {
        // Clear status area (lower left)
        M5.Lcd.fillRect(10, 195, 100, 20, BLACK);
        
        M5.Lcd.setTextSize(1);
        if (timeSinceUpdate > 5000) {
            M5.Lcd.setTextColor(RED, BLACK);
        } else {
            M5.Lcd.setTextColor(CYAN, BLACK);
        }
        M5.Lcd.setCursor(10, 200);
        M5.Lcd.print(statusText);
        
        lastStatusText = statusText;
    }
}

void performBLEScan() {
    // Clear previous results
    discoveredDevices.clear();
    
    // Show scanning message
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.println("Scanning for devices...");
    
    Serial.println("Starting BLE scan...");
    
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->clearResults();
    
    // Scan for 10 seconds
    BLEScanResults foundDevices = pBLEScan->start(10, false);
    
    Serial.printf("Scan complete. Found %d total devices, %d UART devices.\n", 
                  foundDevices.getCount(), discoveredDevices.size());
    
    // Clear screen and display results
    needsFullRedraw = true;
    displayDeviceList();
}

void setup() {
    // Initialize M5Stack Core2
    M5.begin();
    
    // Initialize the display
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(2);
    
    // Display Loading message
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.println("Loading...");
    
    // Initialize serial communication
    Serial.begin(115200);
    Serial.println("M5Stack Core2 BLE Scanner");
    Serial.println("System initialized successfully");
    
    // Initialize BLE
    M5.Lcd.setCursor(10, 80);
    M5.Lcd.println("Initializing BLE...");
    Serial.println("Initializing BLE...");
    
    BLEDevice::init("");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    // Perform initial scan
    performBLEScan();
}

void loop() {
    // Update M5Stack Core2 system
    M5.update();
    
    if (isConnected) {
        // Handle connected state
        if (M5.BtnA.wasPressed()) {
            Serial.println("Button A pressed - Disconnect");
            if (pClient) {
                pClient->disconnect();
            }
            isConnected = false;
            selectedDeviceIndex = 0;
            needsFullRedraw = true;
            displayDeviceList();
        }
        
        if (M5.BtnB.wasPressed()) {
            Serial.println("Button B pressed - Request voltage");
            sendVESCPacket(COMM_GET_VALUES);
        }
        
        if (M5.BtnC.wasPressed()) {
            Serial.println("Button C pressed - Back to device list");
            if (pClient) {
                pClient->disconnect();
            }
            isConnected = false;
            selectedDeviceIndex = 0;
            needsFullRedraw = true;
            displayDeviceList();
        }
        
        // Auto-request voltage data every 3 seconds
        static unsigned long lastRequest = 0;
        if (millis() - lastRequest > 3000) {
            sendVESCPacket(COMM_GET_VALUES);
            lastRequest = millis();
        }
        
        // Update voltage display
        displayVoltage();
        
    } else {
        // Handle scanning/selection state
        if (M5.BtnA.wasPressed()) {
            Serial.println("Button A pressed - Rescanning");
            selectedDeviceIndex = 0;
            performBLEScan();
        }
        
        if (M5.BtnB.wasPressed()) {
            Serial.println("Button B pressed - Navigate devices");
            if (!discoveredDevices.empty()) {
                selectedDeviceIndex = (selectedDeviceIndex + 1) % discoveredDevices.size();
                displayDeviceList();
            }
        }
        
        if (M5.BtnC.wasPressed()) {
            Serial.println("Button C pressed - Connect to selected device");
            if (!discoveredDevices.empty() && selectedDeviceIndex < discoveredDevices.size()) {
                M5.Lcd.fillScreen(BLACK);
                M5.Lcd.setTextSize(2);
                M5.Lcd.setTextColor(YELLOW, BLACK);
                M5.Lcd.setCursor(10, 100);
                M5.Lcd.println("Connecting...");
                
                if (connectToVESC(selectedDeviceIndex)) {
                    needsFullRedraw = true;
                    displayVoltage();
                } else {
                    needsFullRedraw = true;
                    M5.Lcd.setTextColor(RED, BLACK);
                    M5.Lcd.setCursor(10, 100);
                    M5.Lcd.println("Connection failed");
                    delay(2000);
                    M5.Lcd.fillScreen(BLACK);
                    displayDeviceList();
                }
            }
        }
    }
    
    // Small delay to prevent overwhelming the system
    delay(50);
}

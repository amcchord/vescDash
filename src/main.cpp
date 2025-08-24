#include <M5Core2.h>

void setup() {
    // Initialize M5Stack Core2
    M5.begin();
    
    // Initialize the display
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(3);
    
    // Display Hello World
    M5.Lcd.setCursor(50, 100);
    M5.Lcd.println("Hello World!");
    
    // Initialize serial communication
    Serial.begin(115200);
    Serial.println("M5Stack Core2 Hello World");
    Serial.println("System initialized successfully");
}

void loop() {
    // Update M5Stack Core2 system
    M5.update();
    
    // Simple blink effect for the Hello World text
    static unsigned long lastBlink = 0;
    static bool textVisible = true;
    
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        textVisible = !textVisible;
        
        if (textVisible) {
            M5.Lcd.setTextColor(WHITE, BLACK);
            M5.Lcd.setCursor(50, 100);
            M5.Lcd.println("Hello World!");
        } else {
            M5.Lcd.setTextColor(BLACK, BLACK);
            M5.Lcd.setCursor(50, 100);
            M5.Lcd.println("Hello World!");
        }
    }
    
    // Handle button presses
    if (M5.BtnA.wasPressed()) {
        Serial.println("Button A pressed");
        M5.Lcd.setCursor(50, 150);
        M5.Lcd.setTextColor(GREEN, BLACK);
        M5.Lcd.println("Button A!");
    }
    
    if (M5.BtnB.wasPressed()) {
        Serial.println("Button B pressed");
        M5.Lcd.setCursor(50, 150);
        M5.Lcd.setTextColor(BLUE, BLACK);
        M5.Lcd.println("Button B!");
    }
    
    if (M5.BtnC.wasPressed()) {
        Serial.println("Button C pressed");
        M5.Lcd.setCursor(50, 150);
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.println("Button C!");
    }
    
    // Small delay to prevent overwhelming the system
    delay(50);
}

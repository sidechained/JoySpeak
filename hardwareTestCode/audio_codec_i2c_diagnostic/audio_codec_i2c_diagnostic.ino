#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN 16
#define CODEC_RESET_PIN 12 

Adafruit_NeoPixel onboardLED(1, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial.begin(115200);
    
    // Wait up to 3 seconds for the Serial Monitor window to be opened by the user
    unsigned long start = millis();
    while (!Serial && (millis() - start < 3000)); 

    Serial.println("\n=================================");
    Serial.println("TLV320AIC3100 I2C Diagnostic Tool");
    Serial.println("=================================");

    onboardLED.begin();
    onboardLED.setPixelColor(0, onboardLED.Color(0, 0, 100)); // Solid Blue during test
    onboardLED.show();

    // Perform Hardware Reset on GP12
    Serial.println("-> Toggling RESET pin on GP12...");
    pinMode(CODEC_RESET_PIN, OUTPUT);
    digitalWrite(CODEC_RESET_PIN, LOW);
    delay(100); 
    digitalWrite(CODEC_RESET_PIN, HIGH);
    delay(200); // Give the chip plenty of time to wake up its internal digital rails

    // Initialize I2C Pins
    Serial.println("-> Opening I2C bus on GP4 (SDA) and GP5 (SCL)...");
    Wire.setSDA(4);
    Wire.setSCL(5);
    Wire.begin();
    delay(100);
}

void loop() {
    Serial.println("\nScanning address 0x18 (Default Codec Address)...");
    
    Wire.beginTransmission(0x18);
    uint8_t error = Wire.endTransmission();
    
    Serial.print("Result Code: ");
    Serial.print(error);
    
    switch(error) {
        case 0:
            Serial.println(" -> SUCCESS! Codec acknowledged communication!");
            onboardLED.setPixelColor(0, onboardLED.Color(0, 255, 0)); // Green
            break;
        case 1:
            Serial.println(" -> ERROR: Data too long to fit in buffer.");
            onboardLED.setPixelColor(0, onboardLED.Color(255, 0, 0)); // Red
            break;
        case 2:
            Serial.println(" -> ERROR: NACK (No Device Acknowledged). The lines are pulled up, but the chip is acting dead or sleeping.");
            onboardLED.setPixelColor(0, onboardLED.Color(255, 255, 0)); // Yellow
            break;
        case 3:
            Serial.println(" -> ERROR: NACK on data transmit.");
            onboardLED.setPixelColor(0, onboardLED.Color(255, 0, 255)); // Purple
            break;
        case 4:
            Serial.println(" -> ERROR: Bus Error (Lines might be shorted, swapped, or missing common GND).");
            onboardLED.setPixelColor(0, onboardLED.Color(255, 128, 0)); // Orange
            break;
        case 5:
            Serial.println(" -> ERROR: Timeout (I2C bus is locked/stuck low).");
            onboardLED.setPixelColor(0, onboardLED.Color(100, 0, 0)); // Dim Red
            break;
        default:
            Serial.println(" -> Unknown error code.");
    }
    onboardLED.show();
    
    delay(3000); // Scan again every 3 seconds
}
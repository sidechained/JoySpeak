#include <Arduino.h>
#include <Wire.h>
#include <I2S.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>

#define CODEC_I2C_ADDR 0x18
#define LED_PIN 16

// UPDATED: Corrected to your physical wiring layout
#define CODEC_RESET_PIN 12 

Adafruit_NeoPixel onboardLED(1, LED_PIN, NEO_GRB + NEO_KHZ800);
I2S i2sOutput(OUTPUT);

const int SAMPLE_RATE = 44100;
const float TONE_FREQUENCY = 440.0; 
float samplePhase = 0.0;
float phaseIncrement = (2.0 * PI * TONE_FREQUENCY) / SAMPLE_RATE;

unsigned long lastLedUpdate = 0;
bool i2cBusError = false; 

void codecWrite(uint8_t page, uint8_t reg, uint8_t val) {
    static uint8_t currentPage = 0xFF;
    uint8_t response;
    
    if (currentPage != page) {
        Wire.beginTransmission(CODEC_I2C_ADDR);
        Wire.write(0x00); 
        Wire.write(page);
        response = Wire.endTransmission();
        if (response != 0) i2cBusError = true; 
        currentPage = page;
    }
    
    Wire.beginTransmission(CODEC_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    response = Wire.endTransmission();
    if (response != 0) i2cBusError = true; 
}

void initTLV320AIC3100() {
    codecWrite(0, 1, 0x01); // Software Reset
    delay(10); 

    // Clock Config (MCLK = 256 * Fs)
    codecWrite(0, 4, 0x00);  
    codecWrite(0, 11, 0x81); 
    codecWrite(0, 12, 0x82); 
    codecWrite(0, 13, 0x00); 
    codecWrite(0, 14, 0x80); 

    codecWrite(0, 27, 0x00); // I2S Slave mode

    // Routing & Drivers
    codecWrite(1, 9, 0x3C);  
    codecWrite(1, 35, 0x44); 
    codecWrite(1, 38, 0x00); // HPL 0dB
    codecWrite(1, 39, 0x00); // HPR 0dB
    codecWrite(1, 41, 0x04); // Left DAC to Speaker
    codecWrite(1, 42, 0x00); // Speaker 0dB
    codecWrite(1, 16, 0x00); 
    codecWrite(1, 17, 0x00); 
    codecWrite(1, 45, 0x00); 

    // Power up DACs
    codecWrite(0, 63, 0xD4); 
    codecWrite(0, 64, 0x00); 
}

void setup() {
    // 1. Fire up the diagnostic LED
    onboardLED.begin();
    onboardLED.setPixelColor(0, onboardLED.Color(0, 0, 50)); // Solid blue during setup
    onboardLED.show();

    // 2. Perform Hardware Reset Sequence on GP12
    pinMode(CODEC_RESET_PIN, OUTPUT);
    digitalWrite(CODEC_RESET_PIN, LOW);
    delay(20);                          // Hold chip in reset
    digitalWrite(CODEC_RESET_PIN, HIGH);
    delay(50);                          // Wait for internal chip wake-up

    // 3. Initialize I2C Bus
    Wire.setSDA(4);
    Wire.setSCL(5);
    Wire.begin();
    delay(50); 

    // 4. Configure Codec
    initTLV320AIC3100();

    // 5. Initialize I2S Clocks and Engine
    i2sOutput.setMCLK(7);   
    i2sOutput.setBCLK(8);   
    i2sOutput.setDATA(10);  // DOUT from RP2040 -> Connects to DIN on Codec
    i2sOutput.begin(SAMPLE_RATE);
}

void loop() {
    // -----------------------------------------------------------------
    // AUDIO ENGINE (Runs uninterrupted)
    // -----------------------------------------------------------------
    int16_t sample = (int16_t)(sin(samplePhase) * 8000.0);
    samplePhase += phaseIncrement;
    if (samplePhase >= 2.0 * PI) samplePhase -= 2.0 * PI;

    i2sOutput.write16(sample, sample);

    // -----------------------------------------------------------------
    // DIAGNOSTIC VISUALIZER
    // -----------------------------------------------------------------
    unsigned long currentMillis = millis();
    if (currentMillis - lastLedUpdate >= 15) {
        lastLedUpdate = currentMillis;

        if (i2cBusError) {
            // ERROR MODE: Codec is missing, unpowered, or missing I2C pullups. Solid Amber.
            onboardLED.setPixelColor(0, onboardLED.Color(200, 80, 0));
            onboardLED.show();
        } else {
            // HEALTHY MODE: Anatomical Heartbeat
            unsigned long epoch = currentMillis % 1300;
            uint8_t pulseIntensity = 0;

            if (epoch < 120)       pulseIntensity = map(epoch, 0, 120, 0, 120);
            else if (epoch < 240)  pulseIntensity = map(epoch, 120, 240, 120, 0);
            else if (epoch < 360)  pulseIntensity = map(epoch, 240, 360, 0, 255);
            else if (epoch < 550)  pulseIntensity = map(epoch, 360, 550, 255, 0);
            else                   pulseIntensity = 0;

            onboardLED.setPixelColor(0, onboardLED.Color(pulseIntensity, 0, 0));
            onboardLED.show();
        }
    }
}
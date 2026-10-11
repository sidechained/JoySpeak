#include <Arduino.h>
#include <Wire.h>
#include <I2S.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>

#define CODEC_I2C_ADDR 0x18
#define LED_PIN 16
#define CODEC_RESET_PIN 12 

// Button Pin Definitions
#define BUTTON_SPK_PIN 0  // GP0 handles Speaker Mute Toggle
#define BUTTON_HP_PIN 1   // GP1 handles Headphone Mute Toggle

// NEW: External SK6812MINI-EA Chain Definitions
#define EXT_LED_PIN 13    // Data line hooked up to GP13
#define EXT_LED_COUNT 2   // Two addressable LEDs in series

#define CODEC_EXPECTS_64_BCLK 

Adafruit_NeoPixel onboardLED(1, LED_PIN, NEO_GRB + NEO_KHZ800);
// NEW: Instantiated external LED string handler
Adafruit_NeoPixel externalLEDs(EXT_LED_COUNT, EXT_LED_PIN, NEO_GRB + NEO_KHZ800);

I2S i2sOutput(OUTPUT);

const int SAMPLE_RATE = 44100;
unsigned long lastLedUpdate = 0;
bool i2cBusError = false; 

// Button State Variables (for non-blocking edge detection)
bool lastSpkButtonState = HIGH;
bool lastHpButtonState = HIGH;
bool spkMuted = false;
bool hpMuted = false;
unsigned long lastSpkDebounce = 0;
unsigned long lastHpDebounce = 0;

#define LUT_SIZE 256
int16_t sineLUT[LUT_SIZE];
uint16_t lutIndex = 0;
uint32_t phaseAccumulatorSine = 0;
uint32_t phaseAccumulatorSaw = 0;

static uint32_t localPhaseIncrementSine = 0;
static uint32_t localPhaseIncrementSaw = 0;
static uint8_t currentAudioState = 0;
static unsigned long lastParamUpdate = 0;

void generateSineLUT() {
    for (int i = 0; i < LUT_SIZE; i++) {
        sineLUT[i] = (int16_t)(sin((2.0 * PI * i) / LUT_SIZE) * 3000.0); 
    }
}

void codecWrite(uint8_t page, uint8_t reg, uint8_t val) {
    static uint8_t currentPage = 0xFF;
    if (currentPage != page) {
        Wire.beginTransmission(CODEC_I2C_ADDR);
        Wire.write(0x00); 
        Wire.write(page);
        if (Wire.endTransmission() != 0) i2cBusError = true; 
        currentPage = page;
    }
    Wire.beginTransmission(CODEC_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    if (Wire.endTransmission() != 0) i2cBusError = true; 
}

void initTLV320AIC3100() {
    codecWrite(0, 1, 0x01); // Soft Reset
    delay(50); 

    #ifdef CODEC_EXPECTS_64_BCLK
        codecWrite(0, 4, 0x07);  codecWrite(0, 5, 0x91);  codecWrite(0, 6, 0x20);  
        codecWrite(0, 7, 0x00);  codecWrite(0, 8, 0x00);  
        codecWrite(0, 11, 0x84); codecWrite(0, 12, 0x84); 
    #else
        codecWrite(0, 4, 0x07);  codecWrite(0, 5, 0x92);  codecWrite(0, 6, 0x20);  
        codecWrite(0, 7, 0x00);  codecWrite(0, 8, 0x00);  
        codecWrite(0, 11, 0x88); codecWrite(0, 12, 0x82); 
    #endif

    codecWrite(0, 13, 0x00); codecWrite(0, 14, 0x80); 
    codecWrite(0, 27, 0x00); 

    codecWrite(1, 33, 0x4E); codecWrite(1, 31, 0xC2); codecWrite(1, 32, 0x86); 
    codecWrite(1, 35, 0x44); codecWrite(1, 36, 0x00); codecWrite(1, 37, 0x00); 
    codecWrite(1, 38, 0x80); 
    
    codecWrite(1, 40, 0x04); // HPL 0dB Gain
    codecWrite(1, 41, 0x04); // HPR 0dB Gain
    codecWrite(1, 42, 0x04); // Class-D Speaker +6dB Gain

    codecWrite(0, 63, 0xD4); codecWrite(0, 64, 0x00); codecWrite(0, 65, 0x00); codecWrite(0, 66, 0x00); 
}

void setup() {
    Serial.begin(115200);
    generateSineLUT();

    pinMode(BUTTON_SPK_PIN, INPUT_PULLUP);
    pinMode(BUTTON_HP_PIN, INPUT_PULLUP);

    // Initialize Onboard LED
    onboardLED.begin();
    onboardLED.setPixelColor(0, onboardLED.Color(0, 0, 50)); 
    onboardLED.show();

    // NEW: Initialize External SK6812 MINI-EA string
    externalLEDs.begin();
    externalLEDs.setPixelColor(0, externalLEDs.Color(0, 0, 0));
    externalLEDs.setPixelColor(1, externalLEDs.Color(0, 0, 0));
    externalLEDs.show();

    pinMode(CODEC_RESET_PIN, OUTPUT);
    digitalWrite(CODEC_RESET_PIN, LOW);
    delay(50);                          
    digitalWrite(CODEC_RESET_PIN, HIGH);
    delay(100);                         

    Wire.setSDA(4); Wire.setSCL(5); Wire.begin();
    delay(50); 

    initTLV320AIC3100();

    i2sOutput.setBCLK(8);   
    i2sOutput.setDATA(11);  
    i2sOutput.begin(SAMPLE_RATE);
}

void loop() {
    unsigned long currentMillis = millis();

    // -----------------------------------------------------------------
    // 1. NON-BLOCKING BUTTON CONTROLLER LOGIC
    // -----------------------------------------------------------------
    bool currentSpkState = digitalRead(BUTTON_SPK_PIN);
    if (currentSpkState == LOW && lastSpkButtonState == HIGH && (currentMillis - lastSpkDebounce > 250)) {
        spkMuted = !spkMuted;
        lastSpkDebounce = currentMillis;
        if (spkMuted) { codecWrite(1, 42, 0x00); Serial.println("-> SPEAKER: MUTED"); } 
        else          { codecWrite(1, 42, 0x04); Serial.println("-> SPEAKER: UNMUTED"); }
    }
    lastSpkButtonState = currentSpkState;

    bool currentHpState = digitalRead(BUTTON_HP_PIN);
    if (currentHpState == LOW && lastHpButtonState == HIGH && (currentMillis - lastHpDebounce > 250)) {
        hpMuted = !hpMuted;
        lastHpDebounce = currentMillis;
        if (hpMuted) { codecWrite(1, 40, 0x00); codecWrite(1, 41, 0x00); Serial.println("-> HEADPHONES: MUTED"); } 
        else         { codecWrite(1, 40, 0x04); codecWrite(1, 41, 0x04); Serial.println("-> HEADPHONES: UNMUTED"); }
    }
    lastHpButtonState = currentHpState;

    // -----------------------------------------------------------------
    // 2. PARAMETER TIMELINE SEQUENCER (Updates parameters every 2ms)
    // -----------------------------------------------------------------
    if (currentMillis - lastParamUpdate >= 2) {
        lastParamUpdate = currentMillis;
        unsigned long sequenceTime = currentMillis % 9000; 
        
        if (sequenceTime < 3000) {
            currentAudioState = 0; 
            float progress = (float)sequenceTime / 3000.0;
            float freq = 40.0 + (progress * progress * progress * 12000.0); 
            localPhaseIncrementSine = (uint32_t)((freq * LUT_SIZE * 65536.0) / SAMPLE_RATE);
        } 
        else if (sequenceTime < 6000) {
            currentAudioState = 1; 
        } 
        else {
            currentAudioState = 2; 
            float progress = sin(((float)(sequenceTime - 6000) / 3000.0) * PI); 
            float freq = 150.0 + (progress * 800.0);
            localPhaseIncrementSaw = (uint32_t)((freq * 4294967296.0) / SAMPLE_RATE);
        }
    }

    // -----------------------------------------------------------------
    // 3. HIGH-SPEED SAMPLE GENERATION
    // -----------------------------------------------------------------
    int16_t sample = 0;

    if (currentAudioState == 0) {
        lutIndex = (phaseAccumulatorSine >> 16) % LUT_SIZE;
        sample = sineLUT[lutIndex];
        phaseAccumulatorSine += localPhaseIncrementSine;
    } 
    else if (currentAudioState == 1) {
        static uint32_t xorState = 0x12345678;
        xorState ^= xorState << 13;
        xorState ^= xorState >> 17;
        xorState ^= xorState << 5;
        sample = (int16_t)((xorState % 4001) - 2000);
    } 
    else {
        sample = (int16_t)(((phaseAccumulatorSaw >> 16) * 8000 / 65535) - 4000);
        phaseAccumulatorSaw += localPhaseIncrementSaw;
    }

    i2sOutput.write16(sample, sample);

    // -----------------------------------------------------------------
    // 4. SYNCHRONIZED STATUS HEARTBEAT & LED ROUTING ENGINE
    // -----------------------------------------------------------------
    if (currentMillis - lastLedUpdate >= 15) {
        lastLedUpdate = currentMillis;
        
        uint32_t pulseColor = onboardLED.Color(0, 0, 0);

        if (i2cBusError) {
            pulseColor = onboardLED.Color(200, 80, 0); // Amber warning color
        } else {
            unsigned long epoch = currentMillis % 1300;
            uint8_t pulseIntensity = 0;
            if (epoch < 120)       pulseIntensity = map(epoch, 0, 120, 0, 120);
            else if (epoch < 240)  pulseIntensity = map(epoch, 120, 240, 120, 0);
            else if (epoch < 360)  pulseIntensity = map(epoch, 240, 360, 0, 255);
            else if (epoch < 550)  pulseIntensity = map(epoch, 360, 550, 255, 0);
            else                   pulseIntensity = 0;

            // Generate precise phase color matches
            if (currentAudioState == 0)      pulseColor = onboardLED.Color(pulseIntensity, 0, 0);                 // Red (Sine)
            else if (currentAudioState == 1) pulseColor = onboardLED.Color(pulseIntensity, pulseIntensity, 0); // Yellow (Noise)
            else                             pulseColor = onboardLED.Color(0, 0, pulseIntensity);                 // Blue (Sawtooth)
        }

        // Set comparison Onboard LED
        onboardLED.setPixelColor(0, pulseColor);
        onboardLED.show();

        // NEW: Matrix Routing for External Series Chain on GP13
        if (currentAudioState == 0) {
            // Sequence 1: LED 1 active, LED 2 dark
            externalLEDs.setPixelColor(0, pulseColor);
            externalLEDs.setPixelColor(1, externalLEDs.Color(0, 0, 0));
        } 
        else if (currentAudioState == 1) {
            // Sequence 2: LED 1 dark, LED 2 active
            externalLEDs.setPixelColor(0, externalLEDs.Color(0, 0, 0));
            externalLEDs.setPixelColor(1, pulseColor);
        } 
        else {
            // Sequence 3: Both LEDs active simultaneously
            externalLEDs.setPixelColor(0, pulseColor);
            externalLEDs.setPixelColor(1, pulseColor);
        }
        externalLEDs.show();
    }
}
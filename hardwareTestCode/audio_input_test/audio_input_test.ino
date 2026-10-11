#include <Arduino.h>
#include <Wire.h>
#include <I2S.h>
#include <Adafruit_NeoPixel.h>

#define CODEC_I2C_ADDR 0x18
#define LED_PIN 16
#define CODEC_RESET_PIN 12 

// Hardware Pin Layout
#define BUTTON_SPK_PIN 0  
#define BUTTON_HP_PIN 1   
#define EXT_LED_PIN 13    
#define EXT_LED_COUNT 2   
#define JACK_IN_PIN 15    
#define JACK_OUT_PIN 14   

// Joystick Pins
#define JOY_VERT_PIN 26   
#define JOY_HORZ_PIN 27   
#define JOY_BTN_PIN 28    

Adafruit_NeoPixel onboardLED(1, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel externalLEDs(EXT_LED_COUNT, EXT_LED_PIN, NEO_GRB + NEO_KHZ800);

I2S i2sFullDuplex(INPUT_PULLUP);

const int SAMPLE_RATE = 44100;
unsigned long lastLedUpdate = 0;
bool i2cBusError = false; 

// Button Debounce Registers
bool lastSpkButtonState = HIGH;
bool lastHpButtonState = HIGH;
bool spkMuted = false;
bool hpMuted = false;
unsigned long lastSpkDebounce = 0;
unsigned long lastHpDebounce = 0;

bool lastJoyBtnState = HIGH;
unsigned long lastJoyBtnDebounce = 0;

volatile bool globalJackInConnected = false;

// 0.5-Second Audio Delay Buffer
#define DELAY_BUFFER_SIZE 22050
int16_t delayBuffer[DELAY_BUFFER_SIZE] = {0};
uint32_t bufferIndex = 0;

// -----------------------------------------------------------------
// SPEECH SYNTHESIS ENGINE REGISTERS & VARIABLE STRUCTURES
// -----------------------------------------------------------------
volatile uint8_t speechMode = 0;       // 0 = Numbers, 1 = Words, 2 = Vowel Morph
volatile uint16_t synthVolume = 4000;   

// Dynamic Formant Control Targets
volatile uint32_t incF0 = 12623725;     // ~130Hz baseline vocal pitch
volatile uint32_t incF1 = 48552695;     // Formant 1 oscillator increment
volatile uint32_t incF2 = 106815930;    // Formant 2 oscillator increment
volatile bool currentFrameIsNoise = false; 

// High-Speed Phase Accumulators
uint32_t phaseF0 = 0;
uint32_t phaseF1 = 0;
uint32_t phaseF2 = 0;

// Word Sequencer Parameters
uint32_t sampleCounter = 0;
uint8_t currentPhonemeStep = 0;
uint32_t samplesPerStep = 3300;         // Controls the speech speed (~75ms per step)
uint8_t activeWordSelection = 0;        // 0 = Left, 1 = Center, 2 = Right

// Pre-calculated Sine Wave Table
#define LUT_SIZE 256
int16_t sineLUT[LUT_SIZE];

void generateSineLUT() {
    for (int i = 0; i < LUT_SIZE; i++) {
        sineLUT[i] = (int16_t)(sin((2.0 * PI * i) / LUT_SIZE) * 3000.0); 
    }
}

// Algorithmic Phoneme Dictionary Matrix
// Returns Formant 1 Freq, Formant 2 Freq, and Noise flags for any given word step
void getPhonemeData(uint8_t mode, uint8_t wordIdx, uint8_t step, uint16_t &f1, uint16_t &f2, bool &isNoise) {
    isNoise = false;
    
    if (mode == 0) { // NUMBERS MODE
        if (wordIdx == 0) { // "ONE" (W - AH - N)
            if (step == 0)      { f1 = 300; f2 = 700; }  // OO sound
            else if (step == 1) { f1 = 550; f2 = 950; }  // transition
            else if (step == 2) { f1 = 730; f2 = 1100; } // AH sound
            else if (step == 3) { f1 = 650; f2 = 1050; } 
            else                { f1 = 300; f2 = 1500; } // Nasal N closure
        } 
        else if (wordIdx == 1) { // "TWO" (T - OO)
            if (step == 0 || step == 1) { f1 = 0; f2 = 0; isNoise = true; } // "T" Plosive Noise
            else                        { f1 = 300; f2 = 800; }             // Long OO sound
        } 
        else { // "THREE" (TH - R - EE)
            if (step == 0)      { f1 = 0; f2 = 0; isNoise = true; } // "TH" Fricative Noise
            else if (step == 1) { f1 = 400; f2 = 1000; }            // R transition
            else                { f1 = 270; f2 = 2200; }            // Bright EE sound
        }
    } 
    else { // WORDS MODE
        if (wordIdx == 0) { // "HELLO" (H - EH - L - OH)
            if (step == 0)      { f1 = 0; f2 = 0; isNoise = true; } // "H" Aspiration Noise
            else if (step == 1) { f1 = 530; f2 = 1840; }            // EH sound
            else if (step == 2) { f1 = 400; f2 = 1020; }            // L liquid glide
            else                { f1 = 570; f2 = 840; }             // OH sound
        } 
        else if (wordIdx == 1) { // "YES" (Y - EH - S)
            if (step == 0)      { f1 = 270; f2 = 2200; }            // Y / High EE start
            else if (step == 1) { f1 = 530; f2 = 1840; }            // EH sound
            else                { f1 = 0; f2 = 0; isNoise = true; } // "S" Sibilant tail
        } 
        else { // "NO" (N - OH)
            if (step == 0)      { f1 = 300; f2 = 1500; }            // N initial nasal
            else if (step == 1) { f1 = 570; f2 = 840; }             // OH vowel open
            else                { f1 = 350; f2 = 750; }             // OO vowel closing
        }
    }
}

void codecWrite(uint8_t page, uint8_t reg, uint8_t val) {
    static uint8_t currentPage = 0xFF;
    if (currentPage != page) {
        Wire.beginTransmission(CODEC_I2C_ADDR);
        Wire.write(0x00); Wire.write(page);
        Wire.endTransmission(); currentPage = page;
    }
    Wire.beginTransmission(CODEC_I2C_ADDR);
    Wire.write(reg); Wire.write(val);
    Wire.endTransmission();
}

void initTLV320AIC3100() {
    codecWrite(0, 1, 0x01); // Soft Reset
    delay(50); 
    codecWrite(0, 4, 0x07);  codecWrite(0, 5, 0x91);  codecWrite(0, 6, 0x20);  
    codecWrite(0, 7, 0x00);  codecWrite(0, 8, 0x00);  
    codecWrite(0, 11, 0x84); codecWrite(0, 12, 0x84); 
    codecWrite(0, 18, 0x84); codecWrite(0, 19, 0x84); codecWrite(0, 20, 0x80); 
    codecWrite(0, 27, 0x00); codecWrite(0, 29, 0x00); 

    codecWrite(1, 46, 0x00); // MICBIAS off
    codecWrite(1, 48, 0x50); // Line-in inputs mixed
    codecWrite(1, 49, 0x40); // CM negative
    codecWrite(1, 47, 0x80); // Preamp active (0dB)

    codecWrite(0, 81, 0x80); codecWrite(0, 82, 0x00); codecWrite(0, 83, 0x00); 
    codecWrite(1, 33, 0x4E); codecWrite(1, 31, 0xC2); codecWrite(1, 32, 0x86); 
    codecWrite(1, 35, 0x44); codecWrite(1, 36, 0x00); codecWrite(1, 37, 0x00); 
    codecWrite(1, 38, 0x80); 
    codecWrite(1, 40, 0x04); codecWrite(1, 41, 0x04); codecWrite(1, 42, 0x04); 
    codecWrite(0, 63, 0xD4); codecWrite(0, 64, 0x00); codecWrite(0, 65, 0x00); codecWrite(0, 66, 0x00); 
}

void setup() {
    Serial.begin(115200);
    generateSineLUT();

    pinMode(BUTTON_SPK_PIN, INPUT_PULLUP);
    pinMode(BUTTON_HP_PIN, INPUT_PULLUP);
    pinMode(JACK_IN_PIN, INPUT_PULLUP);
    pinMode(JACK_OUT_PIN, INPUT_PULLUP);
    pinMode(JOY_BTN_PIN, INPUT_PULLUP);
    analogReadResolution(12);

    onboardLED.begin();
    externalLEDs.begin();

    pinMode(CODEC_RESET_PIN, OUTPUT);
    digitalWrite(CODEC_RESET_PIN, LOW); delay(50);                          
    digitalWrite(CODEC_RESET_PIN, HIGH); delay(100);                         

    Wire.setSDA(4); Wire.setSCL(5); Wire.begin(); delay(50); 
    initTLV320AIC3100();

    i2sFullDuplex.setBCLK(8); i2sFullDuplex.setDOUT(11); i2sFullDuplex.setDIN(10);         
    i2sFullDuplex.begin(SAMPLE_RATE);
}

void loop() {
    unsigned long currentMillis = millis();

    // -----------------------------------------------------------------
    // 1. HARDWARE INPUT CONTROLLERS & DEBOUNCING
    // -----------------------------------------------------------------
    bool currentSpkState = digitalRead(BUTTON_SPK_PIN);
    if (currentSpkState == LOW && lastSpkButtonState == HIGH && (currentMillis - lastSpkDebounce > 250)) {
        spkMuted = !spkMuted; lastSpkDebounce = currentMillis;
        codecWrite(1, 42, spkMuted ? 0x00 : 0x04);
    }
    lastSpkButtonState = currentSpkState;

    bool currentHpState = digitalRead(BUTTON_HP_PIN);
    if (currentHpState == LOW && lastHpButtonState == HIGH && (currentMillis - lastHpDebounce > 250)) {
        hpMuted = !hpMuted; lastHpDebounce = currentMillis;
        codecWrite(1, 40, hpMuted ? 0x00 : 0x04); codecWrite(1, 41, hpMuted ? 0x00 : 0x04);
    }
    lastHpButtonState = currentHpState;

    // -----------------------------------------------------------------
    // 2. TIMED AUDIO STREAM & HIGH-SPEED SPEECH SYNTHESIS ENGINE
    // -----------------------------------------------------------------
    if (i2sFullDuplex.available() >= 4) {
        int16_t micLeftSample = 0;
        int16_t micRightSample = 0;
        i2sFullDuplex.read16(&micLeftSample, &micRightSample);
        
        int16_t delayedSample = delayBuffer[bufferIndex];
        delayBuffer[bufferIndex] = globalJackInConnected ? micLeftSample : 0;
        bufferIndex = (bufferIndex + 1) % DELAY_BUFFER_SIZE;
        
        // --- START FORMANT SPEECH WAVE GENERATOR ---
        int16_t speechSample = 0;

        if (speechMode == 0 || speechMode == 1) {
            // AUTOMATED WORD/NUMBER SEQUENCE CONTROLLER
            sampleCounter++;
            if (sampleCounter >= samplesPerStep) {
                sampleCounter = 0;
                currentPhonemeStep = (currentPhonemeStep + 1) % 4; // Step through the 4 phoneme frames
            }
            
            uint16_t targetF1 = 400, targetF2 = 1200;
            getPhonemeData(speechMode, activeWordSelection, currentPhonemeStep, targetF1, targetF2, currentFrameIsNoise);
            
            // Map frequencies to phase increments
            incF1 = (uint32_t)((targetF1 * 4294967296.0) / SAMPLE_RATE);
            incF2 = (uint32_t)((targetF2 * 4294967296.0) / SAMPLE_RATE);
        }

        if (currentFrameIsNoise) {
            // Unvoiced Consonant Synthesis: Generate White Noise
            static uint32_t noiseSeed = 12345;
            noiseSeed = noiseSeed * 1664525 + 1013904223;
            speechSample = (int16_t)((int16_t)(noiseSeed >> 16) * synthVolume / 9000);
        } 
        else {
            // Voiced Vowel Synthesis: Simulate a Glottal Pulse Excitation Train
            uint32_t f0Saw = phaseF0 >> 16;
            float glottalExcitation = (65535.0f - f0Saw) / 65535.0f; // Falling sawtooth wave pulse
            
            uint16_t idxF1 = (phaseF1 >> 24) % LUT_SIZE;
            uint16_t idxF2 = (phaseF2 >> 24) % LUT_SIZE;
            
            // Generate combined acoustic vocal tract formants
            int32_t formantSum = (int32_t)sineLUT[idxF1] + ((int32_t)sineLUT[idxF2] * 7 / 10);
            speechSample = (int16_t)((formantSum * glottalExcitation * synthVolume) / 7500);
        }

        // Advance phase tracking
        phaseF0 += incF0;
        phaseF1 += incF1;
        phaseF2 += incF2;

        // Overlay the synthesized speech 50/50 with the line-in audio
        int16_t mixedOutput = (delayedSample / 2) + (speechSample / 2);
        i2sFullDuplex.write16(mixedOutput, mixedOutput);
    }

    // -----------------------------------------------------------------
    // 3. JOYSTICK SAMPLING & SPEECH PARAMETER TRANSLATION (Every 15ms)
    // -----------------------------------------------------------------
    if (currentMillis - lastLedUpdate >= 15) {
        lastLedUpdate = currentMillis;

        int valV = analogRead(JOY_VERT_PIN);
        int valH = analogRead(JOY_HORZ_PIN);

        // --- A. Evaluate Active Speech Mode Parameter Adjustments ---
        if (speechMode == 0) {
            // MODE 0: Numbers Mode
            // Horizontal position maps to Word Selection (Left=ONE, Middle=TWO, Right=THREE)
            if (valH < 1300)      activeWordSelection = 0; 
            else if (valH < 2800) activeWordSelection = 1; 
            else                  activeWordSelection = 2;
            
            // Vertical position scales Vocal Base Pitch (F0) continuously (70Hz to 240Hz)
            uint32_t f0Freq = map(valV, 0, 4095, 70, 240);
            incF0 = (uint32_t)((f0Freq * 4294967296.0) / SAMPLE_RATE);
            samplesPerStep = 3300; // Locked comfortable speed
            currentFrameIsNoise = false; 
        } 
        else if (speechMode == 1) {
            // MODE 1: Words Mode
            // Horizontal position maps to Word Selection (Left=HELLO, Middle=YES, Right=NO)
            if (valH < 1300)      activeWordSelection = 0;
            else if (valH < 2800) activeWordSelection = 1;
            else                  activeWordSelection = 2;
            
            // Vertical position scales Talking Speed (Step Duration)
            // Lower reading speeds up the sequencer frame timing
            samplesPerStep = map(valV, 0, 4095, 1200, 7000); 
            
            uint32_t f0Freq = 135; // Stable natural vocal pitch
            incF0 = (uint32_t)((f0Freq * 4294967296.0) / SAMPLE_RATE);
        } 
        else {
            // MODE 2: Continuous Vowel Morph Mode
            // Horizontal axis morphs Formant 1 continuously (200Hz to 900Hz)
            uint32_t targetF1 = map(valH, 0, 4095, 200, 900);
            // Vertical axis morphs Formant 2 continuously (600Hz to 2300Hz)
            uint32_t targetF2 = map(valV, 0, 4095, 600, 2300);
            
            incF1 = (uint32_t)((targetF1 * 4294967296.0) / SAMPLE_RATE);
            incF2 = (uint32_t)((targetF2 * 4294967296.0) / SAMPLE_RATE);
            
            uint32_t f0Freq = 120; // Robot singing pitch
            incF0 = (uint32_t)((f0Freq * 4294967296.0) / SAMPLE_RATE);
            currentFrameIsNoise = false;
        }

        // --- B. Read Mode Cycle Joystick Button ---
        bool currentJoyBtnState = digitalRead(JOY_BTN_PIN);
        if (currentJoyBtnState == LOW && lastJoyBtnState == HIGH && (currentMillis - lastJoyBtnDebounce > 250)) {
            lastJoyBtnDebounce = currentMillis;
            speechMode = (speechMode + 1) % 3;
            sampleCounter = 0;
            currentPhonemeStep = 0;
            
            Serial.print("-> SPEECH SYNTH MODE ALTERED TO: ");
            if (speechMode == 0)      Serial.println("NUMBERS LOOP (ONE, TWO, THREE)");
            else if (speechMode == 1) Serial.println("WORDS LOOP (HELLO, YES, NO)");
            else                      Serial.println("ROBOTIC VOWEL MORPHING");
        }
        lastJoyBtnState = currentJoyBtnState;

        // --- C. Evaluate Jack Detection Pins ---
        globalJackInConnected = (digitalRead(JACK_IN_PIN) == HIGH);

        // --- D. Set External User LEDs Based On Active Speech Mode ---
        if (speechMode == 0) {
            // Mode 0: LED 1 Green, LED 2 Off
            externalLEDs.setPixelColor(0, externalLEDs.Color(0, 150, 0));
            externalLEDs.setPixelColor(1, externalLEDs.Color(0, 0, 0));
        } 
        else if (speechMode == 1) {
            // Mode 1: LED 1 Off, LED 2 Blue
            externalLEDs.setPixelColor(0, externalLEDs.Color(0, 0, 0));
            externalLEDs.setPixelColor(1, externalLEDs.Color(0, 0, 150));
        } 
        else {
            // Mode 2: Both User LEDs Active
            externalLEDs.setPixelColor(0, externalLEDs.Color(0, 150, 0));
            externalLEDs.setPixelColor(1, externalLEDs.Color(0, 0, 150));
        }
        externalLEDs.show();

        // --- E. Pulse Onboard Heartbeat LED ---
        unsigned long epoch = currentMillis % 1300;
        uint8_t pulseIntensity = 0;
        if (epoch < 120)       pulseIntensity = map(epoch, 0, 120, 0, 120);
        else if (epoch < 240)  pulseIntensity = map(epoch, 120, 240, 120, 0);
        else if (epoch < 360)  pulseIntensity = map(epoch, 240, 360, 0, 255);
        else if (epoch < 550)  pulseIntensity = map(epoch, 360, 550, 255, 0);
        
        onboardLED.setPixelColor(0, onboardLED.Color(0, pulseIntensity, pulseIntensity / 3));
        onboardLED.show();
    }
}
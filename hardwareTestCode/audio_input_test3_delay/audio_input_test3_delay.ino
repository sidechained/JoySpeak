#include <Arduino.h>
#include <Wire.h>
#include <I2S.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

#define CODEC_I2C_ADDR 0x18
#define CODEC_RESET_PIN 12 
#define LED_PIN 16

Adafruit_NeoPixel onboardLED(1, LED_PIN, NEO_GRB + NEO_KHZ800);
I2S i2sFullDuplex(INPUT_PULLUP);
const int SAMPLE_RATE = 44100;

int16_t micMin = 32767;
int16_t micMax = -32768;
uint32_t sampleCount = 0;

// Pre-computed Sine Table for Pitch Mod (Avoids floating point sinf in hot loop)
float sineTable[256];

void initSineTable() {
    for (int i = 0; i < 256; i++) {
        sineTable[i] = (sinf((float)i * 2.0f * M_PI / 256.0f) + 1.0f) * 0.5f;
    }
}

// --- OPTIMIZED DSP AUDIO EFFECTS ---

struct FastPitchMod {
    static const int BUFFER_SIZE = 256;
    float buffer[BUFFER_SIZE] = {0.0f};
    int writeIndex = 0;
    uint8_t lfoIndex = 0;
    uint16_t lfoSubStep = 0; // Fixed point counter for LFO speed

    float process(float in) {
        buffer[writeIndex] = in;

        // Advance LFO (~2 Hz rate)
        lfoSubStep += 12; 
        if (lfoSubStep >= 256) {
            lfoSubStep -= 256;
            lfoIndex++;
        }

        float mod = sineTable[lfoIndex];
        float delaySamples = 10.0f + (mod * 20.0f); // 10 to 30 sample delay
        
        float readPos = (float)writeIndex - delaySamples;
        if (readPos < 0) readPos += BUFFER_SIZE;

        int i1 = (int)readPos;
        int i2 = (i1 + 1) % BUFFER_SIZE;
        float frac = readPos - (float)i1;

        float out = buffer[i1] + frac * (buffer[i2] - buffer[i1]);
        writeIndex = (writeIndex + 1) % BUFFER_SIZE;
        return out;
    }
};

struct FastDelay {
    static const int DELAY_SAMPLES = 4410; // ~100 ms delay
    float buffer[DELAY_SAMPLES] = {0.0f};
    int writeIndex = 0;

    float process(float in) {
        float delayed = buffer[writeIndex];
        buffer[writeIndex] = in + (delayed * 0.35f);
        writeIndex = (writeIndex + 1) % DELAY_SAMPLES;
        return delayed;
    }
};

struct FastReverb {
    static const int SIZE_C1 = 800;
    static const int SIZE_C2 = 950;
    static const int SIZE_AP = 200;

    float comb1[SIZE_C1] = {0};
    float comb2[SIZE_C2] = {0};
    float allpass[SIZE_AP] = {0};

    int idxC1 = 0, idxC2 = 0, idxAP = 0;

    float process(float in) {
        // Comb filter 1
        float outC1 = comb1[idxC1];
        comb1[idxC1] = in + (outC1 * 0.6f);
        idxC1 = (idxC1 + 1) % SIZE_C1;

        // Comb filter 2
        float outC2 = comb2[idxC2];
        comb2[idxC2] = in + (outC2 * 0.55f);
        idxC2 = (idxC2 + 1) % SIZE_C2;

        float combSum = (outC1 + outC2) * 0.5f;

        // Allpass Filter
        float outAP = allpass[idxAP];
        allpass[idxAP] = combSum + (outAP * 0.5f);
        float finalReverb = -combSum + outAP;
        idxAP = (idxAP + 1) % SIZE_AP;

        return finalReverb;
    }
};

FastPitchMod pitchMod;
FastDelay delayEffect;
FastReverb reverbEffect;

void codecWrite(uint8_t page, uint8_t reg, uint8_t val) {
    static uint8_t currentPage = 0xFF;
    if (currentPage != page) {
        Wire.beginTransmission(CODEC_I2C_ADDR);
        Wire.write(0x00); 
        Wire.write(page);
        Wire.endTransmission(); 
        currentPage = page;
    }
    Wire.beginTransmission(CODEC_I2C_ADDR);
    Wire.write(reg); 
    Wire.write(val);
    Wire.endTransmission();
}

void initTLV320AIC3100() {
    codecWrite(0, 1, 0x01);
    delay(50);

    codecWrite(0, 4, 0x07);
    codecWrite(0, 5, 0x91);
    codecWrite(0, 6, 0x20);
    codecWrite(0, 7, 0x00);
    codecWrite(0, 8, 0x00);
    codecWrite(0, 11, 0x84);
    codecWrite(0, 12, 0x84);
    codecWrite(0, 18, 0x84);
    codecWrite(0, 19, 0x84);
    codecWrite(0, 20, 0x80);

    codecWrite(0, 27, 0x00);
    codecWrite(0, 29, 0x00); // 0x00 = SOFTWARE processing loop (0x10 was HW loopback)

    codecWrite(1, 46, 0x03);
    codecWrite(1, 48, 0x04);
    codecWrite(1, 49, 0x40);
    codecWrite(1, 50, 0xC0);
    codecWrite(1, 47, 0x00); // Mic PGA Gain = +24 dB

    codecWrite(0, 81, 0x80);
    codecWrite(0, 82, 0x00);
    codecWrite(0, 83, 0x00);
    codecWrite(0, 86, 0x00);

    codecWrite(1, 33, 0x4E);
    codecWrite(1, 31, 0xC2);
    codecWrite(1, 32, 0x86);

    codecWrite(1, 35, 0x44);
    codecWrite(1, 36, 0x00);
    codecWrite(1, 37, 0x00);
    codecWrite(1, 38, 0x80);
    codecWrite(1, 40, 0x04);
    codecWrite(1, 41, 0x04);
    codecWrite(1, 42, 0x04);

    codecWrite(0, 63, 0xD4);
    codecWrite(0, 64, 0x00);
    codecWrite(0, 65, 0x00);
    codecWrite(0, 66, 0x00);
}

void setup() {
    initSineTable();

    pinMode(CODEC_RESET_PIN, OUTPUT);
    digitalWrite(CODEC_RESET_PIN, LOW); 
    delay(50);
    digitalWrite(CODEC_RESET_PIN, HIGH); 
    delay(100);

    Wire.setSDA(4); 
    Wire.setSCL(5); 
    Wire.begin(); 
    delay(50); 

    i2sFullDuplex.setBCLK(8); 
    i2sFullDuplex.setDOUT(11); 
    i2sFullDuplex.setDIN(10);
    i2sFullDuplex.begin(SAMPLE_RATE);
    delay(10);

    initTLV320AIC3100();
}

void loop() {
    // Process samples strictly when available
    while (i2sFullDuplex.available() >= 4) {
        int16_t micLeft = 0;
        int16_t micRight = 0;

        i2sFullDuplex.read16(&micLeft, &micRight);

        // Normalize float (-1.0 to 1.0)
        float sample = (float)micLeft * (1.0f / 32768.0f);

        // DSP Pipeline
        float pitched = pitchMod.process(sample);
        float delayed = delayEffect.process(pitched);
        float dryWithDelay = (pitched * 0.7f) + (delayed * 0.3f);
        
        float rev = reverbEffect.process(dryWithDelay);
        float finalMix = (dryWithDelay * 0.8f) + (rev * 0.2f);

        // Hard Limit Output
        if (finalMix > 1.0f) finalMix = 1.0f;
        if (finalMix < -1.0f) finalMix = -1.0f;

        int16_t outSample = (int16_t)(finalMix * 32767.0f);

        // IMMEDIATE WRITE: Keeps TX FIFO full
        i2sFullDuplex.write16(outSample, outSample);
    }
}
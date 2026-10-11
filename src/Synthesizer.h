#ifndef SYNTHESIZER_H
#define SYNTHESIZER_H

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Simple State Variable Filter (Chamberlin SVF)
class ResonantFilter {
private:
    float lp, hp, bp;
    float f, q;
    float buf0, buf1;

public:
    ResonantFilter() : lp(0), hp(0), bp(0), f(0.01f), q(1.0f), buf0(0), buf1(0) {}

    void setParams(float cutoffHz, float resonance, float sampleRate) {
        float maxCutoff = sampleRate / 6.0f; // Stable limit for Chamberlin
        if (cutoffHz > maxCutoff) cutoffHz = maxCutoff;
        if (cutoffHz < 20.0f) cutoffHz = 20.0f;
        if (resonance < 0.707f) resonance = 0.707f;
        if (resonance > 12.0f) resonance = 12.0f;

        // SVF coefficients
        f = 2.0f * sinf(M_PI * cutoffHz / sampleRate);
        q = 1.0f / resonance;
    }

    float process(float input) {
        // Safe check for NaN/Inf in input
        if (std::isnan(input) || std::isinf(input)) {
            input = 0.0f;
        }

        hp = input - buf0 * q - buf1;
        bp = buf0 + f * hp;
        lp = buf1 + f * bp;
        
        buf0 = bp;
        buf1 = lp;
        
        // Reset filter if it becomes unstable
        if (std::isnan(buf0) || std::isinf(buf0) || std::isnan(buf1) || std::isinf(buf1)) {
            buf0 = 0.0f;
            buf1 = 0.0f;
            lp = 0.0f;
        }
        
        return lp;
    }
};

// Fast approximations or basic oscillators
class Synthesizer {
private:
    float sampleRate;
    
    // Oscillators phase accumulators
    float phase1;
    float phase2;
    
    // Filter
    ResonantFilter filter;
    
    // Active control values
    float targetX;
    float targetY;
    float currentX;
    float currentY;
    
    // Portamento / smoothing factor for controls
    float smoothFactor;

    // Helper: convert MIDI note to frequency
    float midiToFreq(float note) {
        return 440.0f * powf(2.0f, (note - 69.0f) / 12.0f);
    }

    // Helper: closed-form wavefolding (no infinite loops, 100% stable)
    float fold(float sample, float gain) {
        float x = sample * gain;
        if (std::isnan(x) || std::isinf(x)) return 0.0f;
        
        float offset = x + 1.0f;
        float num = offset - 4.0f * floorf(offset / 4.0f);
        if (num < 2.0f) {
            return num - 1.0f;
        } else {
            return 3.0f - num;
        }
    }

public:
    Synthesizer(float sr = 44100.0f) : 
        sampleRate(sr), 
        phase1(0.0f), 
        phase2(0.0f), 
        targetX(0.0f), 
        targetY(0.0f), 
        currentX(0.0f), 
        currentY(0.0f), 
        smoothFactor(0.005f) // Smooth changes to prevent clicks
    {}

    void setSampleRate(float sr) {
        sampleRate = sr;
    }

    void setControl(float x, float y) {
        targetX = x;
        targetY = y;
    }

    // Process a single audio sample (mono output)
    float process(int model) {
        // Smooth joystick coordinates to prevent sudden audio clicks
        currentX += (targetX - currentX) * smoothFactor;
        currentY += (targetY - currentY) * smoothFactor;

        float output = 0.0f;

        if (model == 0) {
            // ==========================================
            // FM Synthesis
            // ==========================================
            // X-axis: carrier base pitch (MIDI note 48 to 72)
            float carrierMidi = 48.0f + (currentX + 1.0f) * 12.0f;
            float carrierFreq = midiToFreq(carrierMidi);
            
            // Y-axis: modulator ratio and index
            float modRatio = 2.0f; // Fixed integer ratio for harmonic sound
            float modFreq = carrierFreq * modRatio;
            float modIndex = (currentY + 1.0f) * 4.0f; // Index from 0 to 8

            // Modulator Oscillator
            phase2 += (2.0f * M_PI * modFreq) / sampleRate;
            if (phase2 >= 2.0f * M_PI) phase2 -= 2.0f * M_PI;
            float modSample = sinf(phase2) * modIndex;

            // Carrier Oscillator modulated by modulator
            phase1 += (2.0f * M_PI * carrierFreq) / sampleRate;
            if (phase1 >= 2.0f * M_PI) phase1 -= 2.0f * M_PI;
            
            output = sinf(phase1 + modSample) * 0.4f;

        } else if (model == 1) {
            // ==========================================
            // Subtractive Synthesis (Lowpass Resonant Filter)
            // ==========================================
            // X-axis: oscillator base pitch (MIDI note 36 to 60 for low bass)
            float baseMidi = 36.0f + (currentX + 1.0f) * 12.0f;
            float freq1 = midiToFreq(baseMidi);
            float freq2 = freq1 * 1.01f; // Detuned second oscillator

            // Osc 1: Sawtooth (wraps around -1.0 to 1.0)
            phase1 += (2.0f * freq1) / sampleRate;
            if (phase1 >= 1.0f) phase1 -= 2.0f;
            
            // Osc 2: Sawtooth
            phase2 += (2.0f * freq2) / sampleRate;
            if (phase2 >= 1.0f) phase2 -= 2.0f;

            float rawAudio = (phase1 + phase2) * 0.5f;

            // Y-axis: Filter Cutoff (150Hz to 3000Hz) and Resonance
            float cutoffHz = 150.0f + (currentY + 1.0f) * 1425.0f;
            float resonance = 1.0f + (currentY + 1.0f) * 4.0f; // Q from 1 to 9

            filter.setParams(cutoffHz, resonance, sampleRate);
            output = filter.process(rawAudio) * 0.4f;

        } else if (model == 2) {
            // ==========================================
            // Wavefolder / Phase Distortion
            // ==========================================
            // X-axis: pitch (MIDI note 48 to 72)
            float baseMidi = 48.0f + (currentX + 1.0f) * 12.0f;
            float freq = midiToFreq(baseMidi);

            // Sine wave oscillator
            phase1 += (2.0f * M_PI * freq) / sampleRate;
            if (phase1 >= 2.0f * M_PI) phase1 -= 2.0f * M_PI;
            float baseSine = sinf(phase1);

            // Y-axis: Wavefolder gain (from 1.0 to 12.0)
            float foldGain = 1.0f + (currentY + 1.0f) * 5.5f;

            output = fold(baseSine, foldGain) * 0.35f;
        }

        return output;
    }
};

#endif // SYNTHESIZER_H

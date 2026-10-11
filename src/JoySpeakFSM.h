#ifndef JOYSPEAK_FSM_H
#define JOYSPEAK_FSM_H

#define SAMPLE_RATE_HZ 128
#define MAX_BUFFER_SECONDS 4
#define BUFFER_SIZE (SAMPLE_RATE_HZ * MAX_BUFFER_SECONDS) // 512 samples

enum State {
    STATE_IDLE,
    STATE_RECORDING,
    STATE_PLAYBACK,
    STATE_OVERDUB
};

enum PlaybackMode {
    MODE_ONESHOT,
    MODE_LOOPING,
    MODE_OVERDUB
};

struct JoySample {
    float x;
    float y;
};

class JoySpeakFSM {
private:
    State state;
    PlaybackMode mode;
    
    JoySample buffer[BUFFER_SIZE];
    int recordLength;      // Number of valid samples in the buffer
    int playIndex;         // Current index during playback
    
    float deadzone;
    bool isJoystickInCenter;
    
    // Synthesis Model index
    int synthModel;

public:
    JoySpeakFSM() : 
        state(STATE_IDLE), 
        mode(MODE_ONESHOT), 
        recordLength(0), 
        playIndex(0), 
        deadzone(0.15f),
        isJoystickInCenter(true),
        synthModel(0) 
    {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            buffer[i].x = 0.0f;
            buffer[i].y = 0.0f;
        }
    }

    State getState() const { return state; }
    PlaybackMode getMode() const { return mode; }
    int getRecordLength() const { return recordLength; }
    int getPlayIndex() const { return playIndex; }
    int getSynthModel() const { return synthModel; }
    
    void setDeadzone(float dz) { deadzone = dz; }

    // Toggle recording/playback mode (Button 1)
    void pressButton1() {
        if (state == STATE_RECORDING) {
            // Cannot change mode while recording
            return;
        }
        
        switch (mode) {
            case MODE_ONESHOT:
                mode = MODE_LOOPING;
                break;
            case MODE_LOOPING:
                mode = MODE_OVERDUB;
                break;
            case MODE_OVERDUB:
                mode = MODE_ONESHOT;
                break;
        }
    }

    // Trigger/toggle playback (Button 2)
    void pressButton2() {
        if (state == STATE_RECORDING) {
            // Stop recording
            state = STATE_IDLE;
            return;
        }
        
        if (state == STATE_PLAYBACK || state == STATE_OVERDUB) {
            // Stop playback
            state = STATE_IDLE;
            playIndex = 0;
            return;
        }
        
        if (state == STATE_IDLE && recordLength > 0) {
            if (mode == MODE_OVERDUB) {
                state = STATE_OVERDUB;
            } else {
                state = STATE_PLAYBACK;
            }
            playIndex = 0;
        }
    }

    // Cycle synthesizer models (Joystick Button)
    void pressJoystickButton(bool isLongPress = false) {
        if (isLongPress) {
            // Clear buffer and stop everything
            state = STATE_IDLE;
            recordLength = 0;
            playIndex = 0;
            for (int i = 0; i < BUFFER_SIZE; i++) {
                buffer[i].x = 0.0f;
                buffer[i].y = 0.0f;
            }
        } else {
            // Cycle synth models: 0 (FM), 1 (Subtractive), 2 (Wavefolder)
            synthModel = (synthModel + 1) % 3;
        }
    }

    // Process a single tick of the interaction loop (128Hz)
    // Returns the modulated X/Y coordinates to feed to the synthesizer
    JoySample tick(float inputX, float inputY) {
        // Calculate magnitude to check deadzone
        float magnitude = inputX * inputX + inputY * inputY;
        bool inCenter = (magnitude <= deadzone * deadzone);
        
        JoySample output = { inputX, inputY };
        
        switch (state) {
            case STATE_IDLE:
                // Auto-start recording if joystick leaves center
                if (!inCenter && isJoystickInCenter) {
                    state = STATE_RECORDING;
                    recordLength = 0;
                    buffer[0].x = inputX;
                    buffer[0].y = inputY;
                    recordLength = 1;
                }
                break;
                
            case STATE_RECORDING:
                if (inCenter) {
                    // Stopped recording
                    state = STATE_IDLE;
                } else {
                    if (recordLength < BUFFER_SIZE) {
                        buffer[recordLength].x = inputX;
                        buffer[recordLength].y = inputY;
                        recordLength++;
                    } else {
                        // Buffer full, auto stop
                        state = STATE_IDLE;
                    }
                }
                break;
                
            case STATE_PLAYBACK:
                if (recordLength > 0) {
                    output = buffer[playIndex];
                    playIndex++;
                    
                    if (playIndex >= recordLength) {
                        if (mode == MODE_LOOPING) {
                            playIndex = 0; // Loop
                        } else {
                            state = STATE_IDLE; // Done
                            playIndex = 0;
                        }
                    }
                } else {
                    state = STATE_IDLE;
                }
                break;
                
            case STATE_OVERDUB:
                if (recordLength > 0) {
                    // Playback current value
                    JoySample playbackVal = buffer[playIndex];
                    
                    // Blend new joystick movement if it is outside deadzone
                    if (!inCenter) {
                        // 50/50 blend for overdubbing, or write-over
                        buffer[playIndex].x = (playbackVal.x + inputX) * 0.5f;
                        buffer[playIndex].y = (playbackVal.y + inputY) * 0.5f;
                    }
                    
                    output = buffer[playIndex];
                    playIndex++;
                    
                    if (playIndex >= recordLength) {
                        playIndex = 0; // Loop indefinitely in overdub
                    }
                } else {
                    state = STATE_IDLE;
                }
                break;
        }
        
        isJoystickInCenter = inCenter;
        return output;
    }

    // Retrieve raw buffer for visualizer
    const JoySample* getBuffer() const { return buffer; }
};

#endif // JOYSPEAK_FSM_H

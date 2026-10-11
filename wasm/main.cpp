#include <emscripten.h>
#include "../src/JoySpeakFSM.h"
#include "../src/Synthesizer.h"

// Globals for WebAssembly simulation
static JoySpeakFSM fsm;
static Synthesizer synth;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void initSimulation(float sampleRate) {
    fsm = JoySpeakFSM();
    synth = Synthesizer(sampleRate);
}

EMSCRIPTEN_KEEPALIVE
void pressButton1() {
    fsm.pressButton1();
}

EMSCRIPTEN_KEEPALIVE
void pressButton2() {
    fsm.pressButton2();
}

EMSCRIPTEN_KEEPALIVE
void pressJoystickButton(int isLongPress) {
    fsm.pressJoystickButton(isLongPress != 0);
}

EMSCRIPTEN_KEEPALIVE
void tickFSM(float inputX, float inputY) {
    JoySample modulated = fsm.tick(inputX, inputY);
    // Feed the resulting modulated X/Y coordinates into the synthesizer
    synth.setControl(modulated.x, modulated.y);
}

EMSCRIPTEN_KEEPALIVE
void generateAudio(float* outBuffer, int numSamples) {
    int model = fsm.getSynthModel();
    for (int i = 0; i < numSamples; i++) {
        outBuffer[i] = synth.process(model);
    }
}

EMSCRIPTEN_KEEPALIVE
int getFSMState() {
    return (int)fsm.getState();
}

EMSCRIPTEN_KEEPALIVE
int getFSMMode() {
    return (int)fsm.getMode();
}

EMSCRIPTEN_KEEPALIVE
int getSynthModel() {
    return fsm.getSynthModel();
}

EMSCRIPTEN_KEEPALIVE
int getRecordLength() {
    return fsm.getRecordLength();
}

EMSCRIPTEN_KEEPALIVE
int getPlayIndex() {
    return fsm.getPlayIndex();
}

EMSCRIPTEN_KEEPALIVE
float getBufferSampleX(int index) {
    if (index >= 0 && index < BUFFER_SIZE) {
        return fsm.getBuffer()[index].x;
    }
    return 0.0f;
}

EMSCRIPTEN_KEEPALIVE
float getBufferSampleY(int index) {
    if (index >= 0 && index < BUFFER_SIZE) {
        return fsm.getBuffer()[index].y;
    }
    return 0.0f;
}

EMSCRIPTEN_KEEPALIVE
int getMaxBufferSize() {
    return BUFFER_SIZE;
}

}

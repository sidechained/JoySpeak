TARGET = joyspeak_sim.js
CC = emcc

# We export all the C++ wrapper functions, plus malloc/free and HEAP views for buffer passing.
LDFLAGS = -s WASM=1 \
          -s "EXPORTED_FUNCTIONS=['_initSimulation','_pressButton1','_pressButton2','_pressJoystickButton','_tickFSM','_generateAudio','_getFSMState','_getFSMMode','_getSynthModel','_getRecordLength','_getPlayIndex','_getBufferSampleX','_getBufferSampleY','_getMaxBufferSize','_malloc','_free']" \
          -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPF32','HEAP8']" \
          -O3

SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC) ../src/JoySpeakFSM.h ../src/Synthesizer.h
	$(CC) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f joyspeak_sim.js joyspeak_sim.wasm

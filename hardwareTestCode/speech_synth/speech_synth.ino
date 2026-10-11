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
  // SPEECH SYNTHESIS ENGINE CONFIGURATIONS
  // -----------------------------------------------------------------
  volatile uint8_t speechMode = 0;       
  volatile int32_t speechVolumeScale = 8500; // FIX: Boosted internal scale for high presence

  // Dynamic Formant Control Targets
  volatile uint32_t incF0 = 12623725;     
  volatile uint32_t incF1 = 48552695;     
  volatile uint32_t incF2 = 106815930;    

  // High-Speed Phase Accumulators
  uint32_t phaseF0 = 0;
  uint32_t phaseF1 = 0;
  uint32_t phaseF2 = 0;

  // Word Sequencer Parameters
  uint32_t sampleCounter = 0;
  uint8_t currentPhonemeStep = 0;
  uint32_t samplesPerStep = 3500;         
  uint8_t activeWordSelection = 0;        

  // Pre-calculated High-Resolution Sine Wave Table
  #define LUT_SIZE 256
  int16_t sineLUT[LUT_SIZE];

  void generateSineLUT() {
      for (int i = 0; i < LUT_SIZE; i++) {
          sineLUT[i] = (int16_t)(sin((2.0 * PI * i) / LUT_SIZE) * 8000.0); 
      }
  }

  // Fixed-Point Linear Interpolation Table Reader
  inline int32_t getInterpolatedSine(uint32_t phase) {
      uint8_t idx = phase >> 24;                 
      uint8_t fraction = (phase >> 16) & 0xFF;   
      
      int32_t currentSample = sineLUT[idx];
      int32_t nextSample = sineLUT[(idx + 1) & 0xFF];
      
      return (currentSample * (256 - fraction) + nextSample * fraction) >> 8;
  }

  // Algorithmic Phoneme Dictionary Matrix
  void getPhonemeData(uint8_t mode, uint8_t wordIdx, uint8_t step, uint16_t &f1, uint16_t &f2) {
      if (step >= 4) {
          f1 = 0; f2 = 0;
          return;
      }
      
      if (mode == 0) { // NUMBERS MODE
          if (wordIdx == 0) { // "ONE"
              if (step == 0)      { f1 = 320; f2 = 720; }  
              else if (step == 1) { f1 = 580; f2 = 1050; }  
              else if (step == 2) { f1 = 780; f2 = 1200; } 
              else                { f1 = 320; f2 = 1600; } 
          } 
          else if (wordIdx == 1) { // "TWO"
              if (step == 0 || step == 1) { f1 = 0; f2 = 0; } 
              else                        { f1 = 300; f2 = 900; }             
          } 
          else { // "THREE"
              if (step == 0)      { f1 = 0; f2 = 0; } 
              else if (step == 1) { f1 = 440; f2 = 1100; }            
              else                { f1 = 260; f2 = 2350; }            
          }
      } 
      else { // WORDS MODE
          if (wordIdx == 0) { // "HELLO"
              if (step == 0)      { f1 = 0; f2 = 0; } 
              else if (step == 1) { f1 = 550; f2 = 1950; }            
              else if (step == 2) { f1 = 400; f2 = 1100; }            
              else                { f1 = 600; f2 = 900; }             
          } 
          else if (wordIdx == 1) { // "YES"
              if (step == 0)      { f1 = 260; f2 = 2350; }            
              else if (step == 1) { f1 = 550; f2 = 1950; }            
              else                { f1 = 0; f2 = 0; } 
          } 
          else { // "NO"
              if (step == 0)      { f1 = 300; f2 = 1600; }            
              else if (step == 1) { f1 = 600; f2 = 900; }             
              else                { f1 = 340; f2 = 800; }             
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

      codecWrite(1, 46, 0x00); 
      codecWrite(1, 48, 0x50); 
      codecWrite(1, 49, 0x40); 
      codecWrite(1, 47, 0x80); 

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
      // 1. HARDWARE BUTTON CONTROLLERS & DEBOUNCING
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
      // 2. TIMED AUDIO STREAM & HIGH-PRESENCE SPEECH ENGINE
      // -----------------------------------------------------------------
      if (i2sFullDuplex.available() >= 4) {
          int16_t micLeftSample = 0;
          int16_t micRightSample = 0;
          i2sFullDuplex.read16(&micLeftSample, &micRightSample);
          
          int16_t delayedSample = delayBuffer[bufferIndex];
          delayBuffer[bufferIndex] = globalJackInConnected ? micLeftSample : 0;
          bufferIndex = (bufferIndex + 1) % DELAY_BUFFER_SIZE;
          
          int32_t speechSample = 0;

          if (speechMode == 0 || speechMode == 1) {
              sampleCounter++;
              if (sampleCounter >= samplesPerStep) {
                  sampleCounter = 0;
                  currentPhonemeStep = (currentPhonemeStep + 1) % 7; 
              }
              
              uint16_t targetF1 = 0, targetF2 = 0;
              getPhonemeData(speechMode, activeWordSelection, currentPhonemeStep, targetF1, targetF2);
              
              incF1 = (uint32_t)((targetF1 * 4294967296.0) / SAMPLE_RATE);
              incF2 = (uint32_t)((targetF2 * 4294967296.0) / SAMPLE_RATE);
          }

          if (speechMode == 2 || currentPhonemeStep < 4) {
              if (incF1 > 0 || incF2 > 0 || speechMode == 2) {
                  
                  // Smooth Raised-Glottal Flow Model 
                  float glottalExcitation = 0.0f;
                  uint32_t normPhase = phaseF0 >> 16; 
                  if (normPhase < 45875) { 
                      float angle = ((float)normPhase / 45875.0f) * PI;
                      glottalExcitation = sin(angle); 
                  }

                  int32_t sampleF1 = getInterpolatedSine(phaseF1);
                  int32_t sampleF2 = getInterpolatedSine(phaseF2);
                  
                  int32_t formantSum = sampleF1 + (sampleF2 * 6 / 10);
                  
                  // FIX: Recalibrated divisor to maintain smooth mathematical scaling at higher ranges
                  speechSample = (int32_t)(formantSum * glottalExcitation) * speechVolumeScale / 8000;
              }
          }

          phaseF0 += incF0;
          phaseF1 += incF1;
          phaseF2 += incF2;

          // FIX: High-prominence 60/40 mix equation to drive maximum vocal presence safely
          int32_t finalMix = (delayedSample * 6 / 10) + speechSample;
          int16_t mixedOutput = (int16_t)constrain(finalMix, -32768, 32767);
          
          i2sFullDuplex.write16(mixedOutput, mixedOutput);
      }

      // -----------------------------------------------------------------
      // 3. JOYSTICK SAMPLING & SPEECH PARAMETER TRANSLATION (Every 15ms)
      // -----------------------------------------------------------------
      if (currentMillis - lastLedUpdate >= 15) {
          lastLedUpdate = currentMillis;

          int valV = analogRead(JOY_VERT_PIN);
          int valH = analogRead(JOY_HORZ_PIN);

          if (speechMode == 0) {
              if (valH < 1300)      activeWordSelection = 0; 
              else if (valH < 2800) activeWordSelection = 1; 
              else                  activeWordSelection = 2;
              
              uint32_t f0Freq = map(valV, 0, 4095, 80, 160); 
              incF0 = (uint32_t)((f0Freq * 4294967296.0) / SAMPLE_RATE);
              samplesPerStep = 3800; 
              speechVolumeScale = 8500; // FIX: Full-scale vocal drive
          } 
          else if (speechMode == 1) {
              if (valH < 1300)      activeWordSelection = 0;
              else if (valH < 2800) activeWordSelection = 1;
              else                  activeWordSelection = 2;
              
              samplesPerStep = map(valV, 0, 4095, 1800, 5800); 
              
              uint32_t f0Freq = 120; 
              incF0 = (uint32_t)((f0Freq * 4294967296.0) / SAMPLE_RATE);
              speechVolumeScale = 8500; // FIX: Full-scale vocal drive
          } 
          else {
              uint32_t targetF1 = map(valH, 0, 4095, 220, 850);
              uint32_t targetF2 = map(valV, 0, 4095, 650, 2100);
              
              incF1 = (uint32_t)((targetF1 * 4294967296.0) / SAMPLE_RATE);
              incF2 = (uint32_t)((targetF2 * 4294967296.0) / SAMPLE_RATE);
              
              uint32_t f0Freq = 100; 
              incF0 = (uint32_t)((f0Freq * 4294967296.0) / SAMPLE_RATE);
              speechVolumeScale = 5500; // Slightly quieter for manual vowel tracking
          }

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

          globalJackInConnected = (digitalRead(JACK_IN_PIN) == HIGH);

          if (speechMode == 0) {
              externalLEDs.setPixelColor(0, externalLEDs.Color(0, 150, 0));
              externalLEDs.setPixelColor(1, externalLEDs.Color(0, 0, 0));
          } 
          else if (speechMode == 1) {
              externalLEDs.setPixelColor(0, externalLEDs.Color(0, 0, 0));
              externalLEDs.setPixelColor(1, externalLEDs.Color(0, 0, 150));
          } 
          else {
              externalLEDs.setPixelColor(0, externalLEDs.Color(0, 150, 0));
              externalLEDs.setPixelColor(1, externalLEDs.Color(0, 0, 150));
          }
          externalLEDs.show();

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
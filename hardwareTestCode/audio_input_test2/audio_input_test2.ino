  #include <Arduino.h>
  #include <Wire.h>
  #include <I2S.h>
  #include <Adafruit_NeoPixel.h>

  #define CODEC_I2C_ADDR 0x18
  #define CODEC_RESET_PIN 12 
  #define LED_PIN 16

  Adafruit_NeoPixel onboardLED(1, LED_PIN, NEO_GRB + NEO_KHZ800);

  I2S i2sFullDuplex(INPUT_PULLUP);
  const int SAMPLE_RATE = 44100;

  int16_t micMin = 32767;
  int16_t micMax = -32768;
  uint32_t sampleCount = 0;
  const uint32_t REPORT_INTERVAL = 4410; // 100ms interval

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
      // 1. System Reset
      codecWrite(0, 1, 0x01);
      delay(50);

      // 2. Clock Generation & PLL Configuration (Relies on active BCLK)
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

      // 3. Audio Serial Interface & Internal Loopback
      codecWrite(0, 27, 0x00);
      codecWrite(0, 29, 0x10); // BIT 4 = 1: ENABLE INTERNAL ADC-TO-DAC LOOPBACK

      // 4. MEMS Mic Front-End Setup
      codecWrite(1, 46, 0x03); // MICBIAS = AVDD (3.3V)
      codecWrite(1, 48, 0x04); // Route MIC1LM to PGA P-Terminal (10k impedance)
      codecWrite(1, 49, 0x40); // Route Common-Mode (CM) to PGA M-Terminal
      codecWrite(1, 50, 0xC0); // Tie unused MIC1LP and MIC1RP pins internally to CM
      codecWrite(1, 47, 0x00); // Set MIC PGA Gain to +24 dB (+30 dB total)

      // 5. ADC Digital Configuration
      codecWrite(0, 81, 0x80); // Power up Mono ADC
      codecWrite(0, 82, 0x00); // Unmute ADC, 0 dB fine gain
      codecWrite(0, 83, 0x00); // ADC Digital Volume = 0 dB
      codecWrite(0, 86, 0x00); // Disable AGC (Prevents gain hunting / noise amplification)

      // 6. Output Driver Power & Pop Control
      codecWrite(1, 33, 0x4E);
      codecWrite(1, 31, 0xC2); // Power up HPL/HPR
      codecWrite(1, 32, 0x86); // Power up Class-D Speaker Driver

      // 7. Analog Output Routing & Volume
      codecWrite(1, 35, 0x44);
      codecWrite(1, 36, 0x00); // HPL Volume = 0 dB
      codecWrite(1, 37, 0x00); // HPR Volume = 0 dB
      codecWrite(1, 38, 0x80); // Class-D Source = Left Volume
      codecWrite(1, 40, 0x04); // Unmute HPL driver (0 dB)
      codecWrite(1, 41, 0x04); // Unmute HPR driver (0 dB)
      codecWrite(1, 42, 0x04); // Unmute Class-D driver (6 dB)

      // 8. DAC Digital Path Setup
      codecWrite(0, 63, 0xD4); // Power up DAC channels
      codecWrite(0, 64, 0x00); // Unmute DAC
      codecWrite(0, 65, 0x00); // Left DAC Digital Volume = 0 dB
      codecWrite(0, 66, 0x00); // Right DAC Digital Volume = 0 dB
  }

  void setup() {
      Serial.begin(115200);
      while (!Serial && millis() < 2000);
      Serial.println("\n--- Hardware Loopback & MIC Monitor Test ---");
      
      onboardLED.begin();
      onboardLED.setPixelColor(0, onboardLED.Color(0, 120, 40));
      onboardLED.show();

      // 1. Hardware Codec Reset
      pinMode(CODEC_RESET_PIN, OUTPUT);
      digitalWrite(CODEC_RESET_PIN, LOW); 
      delay(50);
      digitalWrite(CODEC_RESET_PIN, HIGH); 
      delay(100);

      // 2. Initialize I2C
      Wire.setSDA(4); 
      Wire.setSCL(5); 
      Wire.begin(); 
      delay(50); 

      // 3. START I2S CLOCKS FIRST (Required for Codec PLL initialization)
      i2sFullDuplex.setBCLK(8); 
      i2sFullDuplex.setDOUT(11); 
      i2sFullDuplex.setDIN(10);
      i2sFullDuplex.begin(SAMPLE_RATE);
      delay(10);

      // 4. Initialize Codec SECOND
      initTLV320AIC3100();
  }

  void loop() {
      if (i2sFullDuplex.available() >= 4) {
          int16_t micLeft = 0;
          int16_t micRight = 0;

          // Read I2S to empty hardware RX FIFO
          i2sFullDuplex.read16(&micLeft, &micRight);

          // DO NOT write back to i2sFullDuplex in code!
          // Internal Hardware Loopback handles ADC -> DAC routing internally.

          if (micLeft < micMin) micMin = micLeft;
          if (micLeft > micMax) micMax = micLeft;
          sampleCount++;

          if (sampleCount >= REPORT_INTERVAL) {
              int32_t peakToPeak = micMax - micMin;

              Serial.print("P-to-P: ");
              if (peakToPeak < 1000) Serial.print(" ");
              if (peakToPeak < 100)  Serial.print(" ");
              Serial.print(peakToPeak);
              Serial.print(" | ");

              // Scaled VU Meter (0 to 3000 P-to-P range)
              int barWidth = map(constrain(peakToPeak, 0, 3000), 0, 3000, 0, 25);
              Serial.print("[");
              for (int i = 0; i < 25; i++) {
                  if (i < barWidth) Serial.print("=");
                  else Serial.print(" ");
              }
              Serial.println("]");

              micMin = 32767;
              micMax = -32768;
              sampleCount = 0;
          }
      }
  }
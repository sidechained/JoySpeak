I need to write C++ test code for a RP2040-Zero connected to a TLV320AIC3100A audio codec. 

I want to write the project in C++ and use the arduino-pico core by Earle Philhower to send audio over I2S from the RP2040-Zero to the TLV320AIC3100A. Prior to that the TLV320AIC3100A will need to be setup via I2C.

The RP2040-Zero is configured as follows:
GP4 - TLV320AIC3100 I2C SDA
GP5 - TLV320AIC3100 I2C SCL
GP7 - TLV320AIC3100 I2S MCLK
GP8 - TLV320AIC3100 I2S BCLK
GP9 - TLV320AIC3100 I2S WCLK
GP10 - TLV320AIC3100 I2S DOUT
GP11 - TLV320AIC3100 I2S DIN

For the TLV320AIC3100A:
- a 4W 3Ohm speaker is connected to SPKP and SPKM pins
- HPL and HPR are connected to a 3.5mm audio jack output

Send a simple test tone over I2S (Earle Philhower, PIO I believe) which goes to both the speaker and the audio output jack

Keep the code as simple and clean as possible

# Phase 2

GP0 - BUTTON 1 (GPIO INPUT)
GP1 - BUTTON 2 (GPIO INPUT)
GP13 - DATA OUTPUT TO 2 x SK6812MINI-EA LEDs (chained together)
GP26 - JOYSTICK VERTICAL POTENTIOMETER (ADC)
GP27 - JOYSTICK HORIZONTOL POTENTIOMETER (ADC)
GP28 - JOYSTICK BUTTON (GPIO INPUT)

Also test the buttons, joystick and LED

# Phase 3 - Audio Input

- Audio input is provided on another 3.5mm audio jack input, but don't configure

For context, I hand soldering the QFN chip onto a custom circuit board I made, so there is a good chance there's an electrical issue there, or an issue with my board/schematic
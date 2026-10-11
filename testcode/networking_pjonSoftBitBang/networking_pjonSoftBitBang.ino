#include <PJONThroughSerial.h>
#include <SerialPIO.h>

#define DEVICE_ID    1
#define RECIPIENT_ID 2

// Pin 28 = TX, Pin 29 = RX (No pinMode calls needed)
SerialPIO pioSerial(28, 29);
PJONThroughSerial bus(DEVICE_ID);

void setup() {
  Serial.begin(115200);
  pioSerial.begin(115200);
  bus.strategy.set_serial(&pioSerial);
  bus.begin();
  Serial.println("[Device 1] 2-Wire PIO Streaming...");
}

void loop() {
  bus.receive(100);
  bus.update();

  static uint32_t last_send = 0;
  static uint32_t last_log = 0;
  static uint8_t param_val = 0;

  if (micros() - last_send >= 10000) { // 100 Hz
    last_send = micros();
    param_val++;
    bus.send_packet(RECIPIENT_ID, &param_val, 1);
  }

  if (millis() - last_log >= 1000) {
    last_log = millis();
    Serial.print("[Device 1] Parameter: ");
    Serial.println(param_val);
  }
}
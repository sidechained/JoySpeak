#include <PJONThroughSerial.h>
#include <SerialPIO.h>
#include <Adafruit_NeoPixel.h>

#define DEVICE_ID 2
#define LED_PIN   16

// Pin 28 = TX, Pin 29 = RX
SerialPIO pioSerial(28, 29);
PJONThroughSerial bus(DEVICE_ID);
Adafruit_NeoPixel pixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

volatile uint32_t packet_count = 0;
volatile uint8_t last_param = 0;

void receiver_function(uint8_t *payload, uint16_t length, const PJON_Packet_Info &packet_info) {
  if (length >= 1) {
    last_param = payload[0];
    packet_count++;
    uint16_t hue = (uint16_t)last_param * 257; 
    pixel.setPixelColor(0, pixel.ColorHSV(hue, 255, 50));
    pixel.show();
  }
}

void setup() {
  Serial.begin(115200);
  pixel.begin();
  pixel.setBrightness(50);
  pixel.show();

  pioSerial.begin(115200);
  bus.strategy.set_serial(&pioSerial);
  bus.begin();
  bus.set_receiver(receiver_function);
  Serial.println("[Device 2] 2-Wire PIO Receiver Ready...");
}

void loop() {
  bus.receive(100);
  bus.update();

  static uint32_t last_log = 0;
  if (millis() - last_log >= 1000) {
    last_log = millis();
    Serial.print("[Device 2] Rx Packets: ");
    Serial.print(packet_count);
    Serial.print(" | Hue: ");
    Serial.println(last_param);
  }
}
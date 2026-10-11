RP2040 networking

# Approach

Connect two RP2040 zeros and use PJON (PJON_DATA pin) to send data between them

# Wiring

Data line GP29 via 47k resistor to GND (holds line stable LOW when idle)
Data line GP29 to other RP2040-Zero
Tie both RP2040-Zero GND's together

# Basic

Use GP29
	- with 47k resistor to ground
		- to hold the line at a stable 0V (logic LOW) when idle, preventing electrical noise from triggering false signals.

View the real-time communication by opening a Serial Monitor set to 115200 baud for each board's COM port.

Start with SoftwareBitBang (PJDL)
	- Needs only:
		- One data wire
		- Common ground

# LED version

- Repurpose BOOT Button to set a random LED colour of sender when pressed
- For each packet received, onboard LED turns the colour of the sender, also on the senders device (should it receive it's own or filter it out?)
- Make random patterns that I can try to observe on both devices. Allow master variable for changing speed.

# Parameter Stream Version

- 4 parameters packed into single 6-byte packed C-struct send per tick (1ms)
- What's lowest speed I can send for accurate control parameter streaming

# Advanced 

Constraints: 
- Basic goal is to sending/receiving 4 parameter control data streams with imperceptable lag (i.e. when turn knob to change pitch)
- Target 75Hz (a packet every 15ms) and run a lightweight exponential moving average or linear ramp at receiving end (smoothing)
- Should be able to handle scaling up number of nodes to 20
- Should be able to handle up to 1m distance between nodes
- Should address two types of jitter
	- bit-level timing jitter (eliminated by PIO)
	- packet-arrival jitter (depends on network topology)
- Should use Master-Slave topology, where:
	- One central Master controls all traffic on a single shared wire by sequentially polling each Slave for data. Devices only speak when commanded, eliminating bus collisions
	- How is master decided/negotiated? What if it drops out?

Switch to PIO to eliminate bottlenecks
	- cycle-accurate timing
	- zero CPU overhead
	- DMA

# Modes

## Star mode:
	- share +5V, PJON_DATA, GND
	- power provided from central box
	- one big resistor across central data knot

## Daisy-chain mode:
	- share PJON and GND only
	- nodes are powered indepedently

# Basic Code Example

```
#include <PJONSoftwareBitBang.h>

// Board 1: DEVICE_ID = 1, RECIPIENT_ID = 2
// Board 2: DEVICE_ID = 2, RECIPIENT_ID = 1
#define DEVICE_ID    1
#define RECIPIENT_ID 2
#define PJON_PIN     0 // GP0 on RP2040-Zero

PJONSoftwareBitBang bus(DEVICE_ID);

void receiver_function(uint8_t *payload, uint16_t length, const PJON_Packet_Info &packet_info) {
  Serial.print("Received from ID ");
  Serial.print(packet_info.sender_id);
  Serial.print(": ");
  Serial.write(payload, length);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  
  bus.strategy.set_pin(PJON_PIN);
  bus.begin();
  bus.set_receiver(receiver_function);
}

void loop() {
  bus.receive(1000); // Check for incoming packets (1000us window)
  bus.update();      // Handle packet delivery and retransmissions

  // Periodically transmit data
  static uint32_t last_send = 0;
  if (millis() - last_send > 2000) {
    last_send = millis();
    
    char message[] = "Ping from device!";
    bus.send_packet(RECIPIENT_ID, message, sizeof(message));
  }
}
```

## LED version approaches

Use the onboard WS2812 RGB LED (connected internally to **GP16**) on the receiving RP2040-Zero to visually verify data flow, interpolation, and transmission jitter.

**Test Setup Overview**

* **Transmitter (Board A):** Runs a software LFO (e.g., a triangle or sine wave oscillating 0–255 over 2 seconds) and broadcasts the parameter value at 50 Hz or 100 Hz.
* **Receiver (Board B):** Receives the parameter via PJON and feeds it directly into an addressable LED library (like Adafruit NeoPixel or FastLED) targeted at **GPIO16**.

**Visual Testing Strategies**

**1. Hue Rainbow Sweep (Tests Parameter Continuity & Interpolation)**

* Map incoming 0–255 parameter values to the **HSV Hue** of the WS2812 LED (0 = Red, 85 = Green, 170 = Blue).
* **What to look for:** A silky-smooth continuous color blend indicates stable data flow and clean receiving-side interpolation. Visible "steps," color jumps, or stutters betray dropped packets or severe "zipper noise."

**2. Packet Arrival & Jitter Diagnostic (Tests Bus Timing Health)**

* Calculate the time delta between arriving packets: $t_{\text{delta}} = t_{\text{current}} - t_{\text{previous}}$.
* Program the LED color based on arrival timing thresholds:
* **Solid Green:** Healthy packet arrival window (e.g., $8\text{ ms} \le t_{\text{delta}} \le 12\text{ ms}$ for a 100 Hz target).
* **Flicker Yellow/Orange:** Small arrival jitter ($13\text{ ms} \le t_{\text{delta}} \le 20\text{ ms}$).
* **Flicker Red:** Missed packet or timing stall ($t_{\text{delta}} > 20\text{ ms}$).


* **What to look for:** A solid green LED with zero red flashes proves your hardware and bus protocol are delivering jitter-free performance before you attach an oscilloscope or speaker.
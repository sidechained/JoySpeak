# Project Definition

I'm making an impromptu network of small speaker-devices with Waveshare RP2040-Zero brains where control data from a onboard 2-potentiometer joystick will be sent across a network and be availabe to any of the devices to control it's onboard synthesis model. All joysticks should be able to "talk" or send data, received by all, who can then pick which data they wish to tune into. The devices should be hot pluggable, should be able to share central power and may be up to 2 meters apart. 

What is the simplest low component-count, low overhead jitterless network protocol solution here, that doesn't require there to be a start and end node (no termination).

I'm also not sure how to handle power. I would like it to be shared alongside the networking data, but don't know whether to provide it centrally (not from a device) or directly from one or more devices

# General

Speaker could auto-mute when mic

# Code

Arduino IDE (with the standard arduino-pico core by Earle Philhower) is highly recommended over MicroPython. The C++ core provides much tighter control over audio-interrupt jitter, pairs beautifully with synth libraries like Mozzi, and natively compiles PJON out of the box.

# Audio Inputs

MEMs mic to MIC1LP (VDD pin of mic to MICBIAS)
Line in to MIC1RP
MIC1LM to analog GND (AVSS)

Not differential - this requires a balanced output
Can listen to one or the other or mix between them

Audio goes in/out via I2S

# Audio Outputs

Speaker to SPK_VOUT+ and SPK_VOUT- (differential)
Stereo line out to HPLOUT and HPROUT (through 1uF - 10uF AC-coupling cap), sleeve to AVSS
	- Can drive headphones or act as stereo line out 
	- Cannot be differential (only have two outputs) - could be if chose mono but overkill
Speaker and line out can be powered separately and have separate volume controls
Jack detection no longer needed (was being used to switch between mic and line input - could still leverage this)

# Control and Interaction

Joystick x/y 10k pots (move joystick from centre to start recording, return to centre to stop)
Joystick button (mode/model change i.e. )
Button 1 - Rec Mode (e.g. one-shot, looping, overdub, punch in - sets length)
LED 1 - shows mode
Button 2 - Trigger
LED 2 - show trigger status
Switch? (speaker mute?)

Joystick pots come in on ADC0, ADC1

# Networked Interaction

## Protocol

- Uses PJON protocol, SIO (Single-cycle I/O) block. It's best run on one core
- Does not use dedicated serial hardware peripheral like UART, I2C, SPI, or PWM

- Run three continuous wires (5V, GND, DATA) across your 2-meter space.
- Connect your external 5V power supply to the 5V and GND lines.
- Solder one 10kΩ resistor between the DATA and GND lines at the power injection point.

For each node:
- Connect the bus 5V line to the RP2040 5V pin.
- Connect the bus GND line to an RP2040 GND pin.
- Solder a 100µF capacitor directly across the RP2040 5V and GND pins (watch the polarity!)
- Connect the bus DATA line directly to the RP2040 GP2 pin.

## Connectors & Cable

## Power Sharing

Check how much current one node draws

Options:

1. Don't plug any of the nodes via USB, instead use a 5V/2A power supply and wire to supply power to the bus

2. One node shouldn't be used via USB of RP2040-Zero to power the whole network

3. It's possible to plug multiple nodes via USB but a 1N5817 or 1N5819 to prevent backfeeding

4. What kind of current needed to power 20 devices? 

To power 20 of these devices safely, you should budget for a 5V power supply capable of delivering at least 8A to 10A (40W to 50W), paired with good power distribution.

Use at least 16 AWG or 14 AWG wire for your main 5V and GND trunk lines

5. What kind of connectors?

- Each node requires two
- Nodes need to not melt with heavy current draw

## Topology

We need to handle star and daisy chain topologies

- Put two identical 3-pin connectors (Pins: 5V, GND, DATA) on every single board. Wire them completely in parallel inside the node.
- In star mode: You run one cable from your central junction box and plug it into either Port A or Port B on the node. The other port simply stays empty.
- In Daisy-Chain Mode: Skip the junction box and run a short cable from Port B of Node 1 into Port A of Node 2, then Port B of Node 2 into Port A of Node 3, and so on. Also 
	- Connect the 5V pins of Port A and Port B together.
	- Solder the Anode (the plain, unmarked side of the diode) to that joint.
	- Solder the Cathode (the side of the diode with the printed stripe) directly to the RP2040-Zero's 5V pin.

### Star Topology (for installation)

- Make a junction box, out of which run 20 2-meter hanging 3-pin connectors
- Junction box to be powered by PeakTech 6227 DC Switching Power Supply (6A max). or fanless 5V / 15A (or 20A) switching power supply (the kind used for LED matrix panels)
- Power split and sent to each node. Each cable only has to carry the electricity for one single node (0.65A max)
- Data lines all joined together, with stiff 1k  resistor between that central data knot and GND inside the box


### Daisy Chain (for fun play)

- Each node must be independently powered
- Power is not shared - 2-Wires (GND, DATA only)
- How to build up resistance?

### Connectors

- JST-XH or RJ45/CAT5?

# Form Factor / Fabrication

Given 

Speaker 
- mounts to front of PCB through hole cut in PCB
- 
- minus terminal on left, plus terminal on right
- will stick out at front 13mm, at back 4.5cm (3.5cm with PCB)

Jacks
- mount on front
- will stick out 5mm (bit more for front circle)

RP2040-Zero
- must mount on front (so LED can shine out)
- must use cutout
- use combined hole and hand solder pads
- solder via castellated edges?

Joystick
- on right
- must mount on front
- will stick out 11.7mm on front (to point where joystick is revealed)
- needs same clearance as speaker all around
- footprint should match one I can source on AliExpress
- officially tactile switch should sit on left (but orient the way that makes best sense on PCB)

Buttons
- on left
- can be capped?

LEDs
- one should be on right around joystick area
- one should be on left 

Cutout in PCB for RP2040
Hole in PCB for mounting speaker
Hexpansion connector on bottom

Mems

front panel 

# PCB + Components

Order PCB + parts, not PCBA
Use SMT components, but solder on our own (or through hole components we have already)
Order joystick connectors from LCSC
Order speakers from Ali
Ship together

# Debugging

SWCLK and SWDIO are not pinned out, so we're limited to printing statements over USB or USB-to-UART.
GDB-over-UART or "Software" SWD (Advanced)

# Power

Three potential sources:
- 5V from USB-C connector
- 5V from intercom JST connector
- 3V3 from hexpansion connector

When used individually can power RP2040 via USB-C
When networked can provide 5V power over CAN bus
Skip the battery this time around

# Hardware

RP2040-Zero w/USB - can use onboard RGB Led
TLV320AIC3100IRHBR (I2S + I2C) C478458 QFN-32-EP(5x5)
YV13S-L7.85-B10Ka(60)-0-DL01 10k Joystick controller C37323747 (dual pot and switch)
2 x Button
2 x LED
SN65HVD230 CAN Transceiver SOIC-8 
2 x PJ-393 8PJ Line In Jack C668609 (input, output)
Hexpansion Connector

# RP2040-Zero Pinout

```
5V / VBUS   : 5V Power Input (Direct from USB-C)
GND         : Common System Ground
3V3         : 3.3V Regulated Power Output (Max ~300mA)
GP0         : Digital IO | UART0 TX | I2C0 SDA | PWM0 A
GP1         : Digital IO | UART0 RX | I2C0 SCL | PWM0 B
GP2         : Digital IO | I2C1 SDA | PWM1 A
GP3         : Digital IO | I2C1 SCL | PWM1 B
GP4         : Digital IO | I2C0 SDA | PWM2 A
GP5         : Digital IO | I2C0 SCL | PWM2 B
GP6         : Digital IO | PWM3 A
GP7         : Digital IO | PWM3 B
GP8         : Digital IO | UART1 TX | PWM4 A
GP9         : Digital IO | UART1 RX | PWM4 B
GP10        : Digital IO | PWM5 A
GP11        : Digital IO | PWM5 B
GP12        : Digital IO | PWM6 A
GP13        : Digital IO | PWM6 B
GP14        : Digital IO | PWM7 A
GP15        : Digital IO | PWM7 B
GP26 / ADC0 : Analog Input 0 | Digital IO | PWM5 A
GP27 / ADC1 : Analog Input 1 | Digital IO | PWM5 B
GP28 / ADC2 : Analog Input 2 | Digital IO | PWM6 A
GP29 / ADC3 : Analog Input 3 | Digital IO | PWM6 B
```

```
-- INTERNAL / REAR SOLDER PAD PINS --
GP16        : Wired to Onboard WS2812B RGB Neopixel LED
GP17        : Digital IO | SPI1 SIN  | PWM0 B
GP18        : Digital IO | SPI1 CSN  | PWM1 A
GP19        : Digital IO | SPI1 SCK  | PWM1 B
GP20        : Digital IO | SPI1 SOUT | PWM2 A
GP21        : Digital IO | PWM2 B
GP22        : Digital IO | PWM3 A
GP23        : Digital IO / Power Configuration Pin
GP24        : Digital IO / Power Configuration Pin
GP25        : Digital IO (Located near user buttons)
```
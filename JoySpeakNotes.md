# Project Definition

I'm making an impromptu network of small speaker-devices with Waveshare RP2040-Zero brains where control data from a onboard 2-potentiometer joystick will be sent across a network and be available to any of the devices to control it's onboard synthesis model. All joysticks should be able to "talk" or send data, received by all, who can then pick which data they wish to tune into. The devices should be hot pluggable, should be able to share central power and may be up to 2 meters apart. 

# Code

Arduino IDE (with the standard arduino-pico core by Earle Philhower). The C++ core provides much tighter control over audio-interrupt jitter, pairs beautifully with synth libraries like Mozzi, and natively compiles PJON out of the box.

Simulation. Build up a browser-based simulation using Webassembly, to "knob record" the joystick movements and select good sounding synthesis models and interaction.

# Audio 

Goes in/out of RP2040 via I2S (PIO)

# Audio Inputs

MEMs mic to MIC1LP (VDD pin of mic to MICBIAS)
Line in to MIC1RP
MIC1LM to analog GND (AVSS)

Not differential - this requires a balanced output
Can listen to one or the other or mix between them

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
Hole in PCB for mounting speaker
Hexpansion connector on bottom


## Protocol

- Uses PJON protocol, SIO (Single-cycle I/O) block. It's best run on one core
- Run three continuous wires (5V, GND, DATA) across your 2-meter space.
- Connect your external 5V power supply to the 5V and GND lines.
- Solder one 10kΩ resistor between the DATA and GND lines at the power injection point.

For each node:
- Connect the bus 5V line to the RP2040 5V pin.
- Connect the bus GND line to an RP2040 GND pin.
- Solder a 100µF capacitor directly across the RP2040 5V and GND pins (watch the polarity!)
- Connect the bus DATA line directly to the RP2040 GP2 pin.

## Connectors & Cable

## Topology

We need to handle star and daisy chain topologies

- Put two RJ45 sockets on each single board. They will connect 5V power, GND, and send/receive PJON. Wire them completely in parallel inside the node.
- In star mode: You run one cable from your central junction box and plug it into either Port A or Port B on the node. The other port simply stays empty.
- In Daisy-Chain Mode: Skip the junction box and run a short cable from Port B of Node 1 into Port A of Node 2, then Port B of Node 2 into Port A of Node 3, and so on. Also 
	- Connect the 5V pins of Port A and Port B together.
	- Solder the Anode (the plain, unmarked side of the diode) to that joint.
	- Solder the Cathode (the side of the diode with the printed stripe) directly to the RP2040-Zero's 5V pin.

### Star Topology (for installation)

- Make a junction box, out of which run up to 40 RJ45 connectors
- Junction box to be powered by PeakTech 6227 DC Switching Power Supply (6A max). or fanless 5V / 15A (or 20A) switching power supply (the kind used for LED matrix panels)
- Power split and sent to each node. Each cable only has to carry the electricity for one single node (0.65A max)
- Data lines all joined together, with stiff 1k resistor between that central data knot and GND inside the box

### Daisy Chain (for fun play)

- Each node must be independently powered
- Power is not shared - 2-Wires (GND, DATA only)
- How to build up resistance?

## Power

Three potential power sources:
- 5V from USB-C connector
- 5V from RJ45
- 3V3 from tildagon hexpansion connector

### Connectors

- Use RJ45

# PCB + Components

Use SMT components, but solder on our own
Order most of part from LCSC and self assemble

# Debugging

SWCLK and SWDIO are not pinned out, so we're limited to printing statements over USB or USB-to-UART.
GDB-over-UART or "Software" SWD (Advanced)

# Other notes

Ensure to use cutout in PCB for RP2040
Speaker could potentially auto-mute when mic engaged
Speaker could be chosen to be muted when audio out jack plugged




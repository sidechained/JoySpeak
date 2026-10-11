# JoySpeak Simulation: AI Coding Prompt & Technical Specification

> **Instructions for Use:** Copy and paste the system prompt block below directly into your coding AI (such as Google Antigravity, Claude, ChatGPT, or Cursor) to generate the single-file prototype.

---

```text
You are an expert audio software engineer and web developer. Your task is to generate a single-file, zero-dependency HTML/JS web application ("index.html") that simulates a single node of "JoySpeak"—a gamepad-controlled synth module based on Mutable Instruments Plaits FM Engine.

### 1. TECHNICAL STACK & CONSTRAINTS
- Single self-contained HTML file containing all HTML, CSS, JavaScript, and Web Audio code.
- Zero build steps, external libraries, or compiler toolchains.
- High-performance audio DSP using inline AudioWorklet processors.
- Visuals rendered using standard HTML5 Canvas/SVG.
- Controls mapped to both Canvas/Mouse GUI and the HTML5 Gamepad API (standard USB/Bluetooth controllers like Logitech F710 or Data Frog T29).
- Clean, modular code structure allowing future synthesis engines and multi-node networking (up to 20 nodes) to be added cleanly.

### 2. HARDWARE CONTROL ARCHITECTURE
- **Left Section**: 2-axis self-centering Joystick + Inbuilt Potentiometer Button (Pot-Button).
- **Right Section**: Button 1 (with LED 1 directly above) and Button 2 (with LED 2 directly above).
- **Node Header**: Main Microcontroller RGB LED (MCU LED) horizontally centered above the controls.

### 3. SYNTHESIS ENGINE (Plaits FM Engine & LPG)
Implement a 2-Operator FM Synthesis Worklet with Low Pass Gate (LPG) emulation:
- **FREQUENCY**: Fundamental pitch (20 Hz - 2000 Hz).
- **HARMONICS**: FM frequency ratio between Operator 1 & Operator 2.
- **TIMBRE**: Modulation index (depth of FM modulation).
- **MORPH**: Feedback parameter (Op 2 self-phase modulation past 12 o'clock; Op 1 phase modulation before 12 o'clock).
- **LPG Envelope**: Emulated 1-pole Low Pass Filter + VC-VCA with variable Attack, Decay, and Peak Volume.

### 4. OPERATIONAL MODES & LOGIC

#### A. Play Mode (Default — Button 1 NOT Held)
- **Radial Interpolation**: Interpolate FREQUENCY, HARMONICS, TIMBRE, and MORPH continuously between 4 Cardinal Presets (UP, RIGHT, DOWN, LEFT) based on joystick angle θ.
- **Dynamic LPG Triggering**:
  - Pushing joystick outward (r > 0) triggers LPG.
  - Outward velocity (Δr/Δt) sets LPG Peak Volume and Attack Time (faster push = louder & punchier attack).
  - Return velocity back to center sets LPG Decay Time (snapping back instantly = short staccato; slow return = long tail).
  - Distance r scales overall output amplitude.

#### B. Setup Mode (Momentary — ACTIVE ONLY WHILE HOLDING BUTTON 1)
- **State Visual**: MCU LED illuminates Amber while Button 1 is held down.
- **Parameter Focus**: Tapping Pot-Button cycles active parameter focus indicated by LED 1 color:
  - FREQUENCY (White), HARMONICS (Red), TIMBRE (Green), MORPH (Blue).
- **Cardinal Preset Tuning**:
  - Pushing into a cardinal zone (±45° sector around UP/RIGHT/DOWN/LEFT) opens a continuous setup tone.
  - Orbiting around the outer edge adjusts the active parameter relative to its stored value (sector center angle = currently stored value; clockwise = increase toward max; counter-clockwise = decrease toward min).
  - Releasing Button 1 commits the updated parameter value to that cardinal preset, silences the setup tone, and immediately returns the node to Play Mode.

#### C. Gesture Record & Playback Mode
- **Instant Record**: Pressing Button 2 down immediately streams joystick coordinates into the active gesture slot (LED 2 turns Red).
- **Loop Playback**: Releasing Button 2 stops recording and immediately loops the recorded gesture (LED 2 turns Green).
- **Slot Selection**: Quick tap of Button 2 (without joystick motion) cycles through 4 gesture slots (Slot 1: White, Slot 2: Red, Slot 3: Green, Slot 4: Blue).
- **Live Punch-In**: Moving joystick during gesture playback temporarily overrides gesture with live play control (LED 2 turns Red). Returning joystick to center resumes gesture loop (LED 2 turns Green).
- **Scrub / Speed Control**: Joystick X-axis during playback scales gesture speed from -2x (reverse) through 0x (pause) to +2x (forward).

### 5. GUI & LAYOUT REQUIREMENTS
- **Node Footprint**: Compact grid cell card (e.g., 220×220 px) engineered to fit a 5x4 grid (20 nodes total on a 1080p desktop view without scrolling). Currently renders 1 node centered on screen.
- **Element Layout**:
  - **Top Center**: MCU RGB LED (Amber in Setup Mode; pulses Blue/White with LPG output volume as a VU meter).
  - **Left Area**: Joystick puck inside circular boundary with cardinal preset markers (UP, RIGHT, DOWN, LEFT) and active sector highlights during Setup mode.
  - **Center Area**: Compact circular speaker displaying a real-time ring oscilloscope/waveform.
  - **Right Area**: Button 1 & Button 2 with LED 1 and LED 2 placed directly above their respective buttons.
```

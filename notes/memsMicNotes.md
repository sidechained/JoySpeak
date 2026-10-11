MEMS MSM381ACB026 can be powered with 1.5-3.6V

Currently in code MICBIAS generator is powered down (0 V).

MIC1LM

codecWrite(1, 46, 0x10); — MICBIAS Power Control - Let's try setting MICBIAS to 2.5V

gain levels are
controlled by writing to page 1 / register 47, bits D6–D0

codecWrite(1, 47, 0x80);


### Multimeter Diagnostics (Is the Mic Alive?)

Power on your board with the latest code running and measure these DC voltages relative to Ground:

- 1. Measure Mic VDD (Pin 1 of `MIC1`)
I get 3V3

- 2. Measure Mic Output DC Bias (Pin 2 of `MIC1`, before C45)
Goes between 0 to 2.5V

- 3. Measure Codec Input Bias (Codec side of $C45$ / `MIC1LM` pin)
I get 1.35V
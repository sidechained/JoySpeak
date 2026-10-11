# Mic fix

# Do we need to jumper power at all?
# Joyspeak Next Steps

Build a second one
	- codec, mic and RP2040 first then test
	- 

Fix mic - omit 10uF cap on next build
Review networking approach and test
Speaker mounting - improve it - with box?

V2 PCB
	- RP2040 codec and mic already on board
	- 3D model casing and add mount points
	- Larger components
	- Omit tildagon connector and do it HASL

Master rule - only provide power one of three ways:
1. From RP2040 USB port
2. From RJ45 connector (either?)
3. From Tildagon hexpansion connector

JP2 - RP2040:	+5V_RP2040ZERO / +5V_EXTERNAL
	- prob
JP3 - 3V3:		+3V3_RP2040ZERO / +3V3_TILDAGON
	- Safe to bridge
JP4 - Speaker:	+5V_RP2040ZERO / +3V3_TILDAGON
	- If plugged from Tildagon hexpansion port, 5V will not be available at all at RP2050 5V pin
	- Therefore it is safe to bridge these
JP5 - LED: 		+5V / +3V3_D
	- Works reliably with 3V3, so could just use this as default
	- Cannot just bridge
	- What happens if we conne

# Needed caps

I designed a joystick controlled speaker with hand soldered surface mount components and built one version of it successfully, but with many bulk and noise caps which I think could be simplified down and largely removed. I would try to now solder a second version will all these caps not placed and add them back in order of importance when something doesn't work well. What do you think of this strategy?

## 10uf
C15 - non-nego - AMS1117-1.8 3V3A (VI)
C22 - non-nego - TLV320AIC3100 3V3A AVDD
C24 - omit - TLV320AIC3100 3V3D IOVDD
C26 - non-nego - TLV320AIC3100 1V8 DVDD
C28 - non-nego - TLV320AIC3100 SPKVDD
C33 - non-nego - +5V_EXTERNAL (from RJ45 connector)
C38 - non-nego - TILDAGON 3V3 (external power coming from a tiladagon badge hexpansion connector)
C40 - non-nego (will introduce noise) - MIC MSM381ACB026 VDD
C44 - omit - +3V3_D to +3V3_A (where 3V3 comes from local RP2040 zero or tildagon hexpansion connector)
C45 - fix - MIC MSM381ACB026 OUT
C46 - bridge (will clip - DC coupling) 3.5mm AUDIO JACK IN (AUD_IN_L) 
C47 - bridge (will clip - DC coupling) 3.5mm AUDIO JACK IN (AUD_IN_R) - bridge

## 100nF
C23 - non-nego - TLV320AIC3100 3V3A AVDD
C25 - non-nego - TLV320AIC3100 3V3A HPVDD
C27 - non-nego - TLV320AIC3100 3V3D IOVDD
C29 - non-nego - TLV320AIC3100 1V8 DVDD
C30 - omit (causes issue!) - TLV320AIC3100 RESET
C31 - non-nego (will introduce noise) - MIC MSM381ACB026 VDD
C34 - non-nego - TLV320AIC3100 SPKVDD
C35 - non-nego - +5V_EXTERNAL (from RJ45 connector)
C37 - omit (see if LEDs cause switching noise) SK6812MINI-EA LED VDD
C39 - non-nego - TILDAGON 3V3 (external power coming from a tiladagon badge hexpansion connector)
C41 - omit (smooth in software) - RP2040 JOY_H (ADC input from joystick potentiometer)
C42 - omit (smooth in software) - RP2040 JOY_V (ADC input from joystick potentiometer)
C43 - omit - TLV320AIC3100 VOL (JOY_V connections ca)
C48 - omit - +3V3_D to +3V3_A (where 3V3 comes from local RP2040 zero or tildagon hexpansion connector)

[ Bare Minimum to Prevent Destruction ]
 ├── Regulator Stability: C15 (AMS1117 Out)
 ├── Power Entry / Hot-Plug: C33, C38 (5V / 3V3 Ext)
 ├── Codec Core Logic: C26 (1.8V DVDD)
 └── Codec Analog Reference: C22 (3V3 AVDD)
Populate only these `10uF protective bulk caps first to ensure regulators are stable, chips don't fry, and logic can boot. From there, you can omit all non-essential filtering, ADC, LED, and local decoupling caps, adding them back only if you hear noise or observe instability!
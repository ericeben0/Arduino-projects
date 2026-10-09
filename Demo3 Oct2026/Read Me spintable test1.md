Recommendation: 30 LEDs per string (60 total) on a dedicated 5V 3–4 A supply.

RAM isn’t the limit. Each LED costs 3 bytes on the Uno, so even 100 per string would fit. Power is the real constraint.
Full-white current. Each WS2812 can draw about 60 mA, so 30 LEDs is up to 1.8 A per string. Your alternating pattern lights only half the LEDs at a time and usually in a color, not white. Real draw is typically under 1 A per string.
The sketch enforces a cap. setMaxPowerInVoltsAndMilliamps(5, 3000) makes FastLED dim automatically if you ever exceed 3 A.
Don’t power the strips from the Uno or USB. USB supplies 500 mA, which is only enough for roughly 8–10 LEDs. 
If you want more than 30 per string, use a bigger supply (about 1 A per 17 LEDs is a safe budget), and wire the +5V and GND into both ends of long strings.

Wiring
From			To
HC-SR04 VCC		Uno 5V
HC-SR04 GND		Uno GND
HC-SR04 Trig		Uno D7
HC-SR04 Echo		Uno D8
LD2410 VCC		Uno 5V
LD2410 GND		Uno GND
LD2410 OUT		Uno D2
Strip A (ultrasonic) DIN 330 Ω resistor, then Uno D5
Strip B (radar) DIN	330 Ω resistor, then Uno D6
Both strips’ +5V	External 5V supply +
Both strips’ GND	External 5V supply −
External supply −	Uno GND (common ground, required)

Put a 1000 µF capacitor across the supply’s + and − terminals, close to the strips.
Power the Uno from USB or its barrel jack, not from the 5V supply.
The LD2410’s OUT pin is 3.3 V logic. That reads as HIGH on a 5V Uno, so no level shifter is needed.
I used the OUT pin because the LD2410’s default 256000 baud UART is unreliable with SoftwareSerial on an Uno.
I avoided D6 conflicts with your other build only by treating this as a standalone sketch. D5/D6 are free here, and no servos or tone() are used.

Notes
Retrigger behavior. Each sensor has a HOLD_MS window that restarts on every detection. This avoids the retrigger and delay problems you had with the PIR. 
The ultrasonic also needs two consecutive hits, which filters out stray echoes.

“Approaching” vs. “present.” The sketch triggers on presence within range, which is what the LD2410 OUT pin reports. 
For true direction detection on the ultrasonic side, compare successive distances and require them to be shrinking.
LD2410 range. Its detection gates and sensitivity are set with HLK’s phone app (via Bluetooth) or the UART interface. 
Set the max range there, since the OUT pin follows whatever you configure. 
It can also detect stationary people, so strip B will stay lit longer than strip A if someone stands still.
Library. 

Install FastLED from the Library Manager.

If you’d like the LD2410 over UART for moving/stationary distance readouts, I can add that using a hardware-serial approach or an ESP32-style setup, since the Uno’s single UART is shared with USB.
# Connection Guide: Voice-Activated Blue/Gold WS2811 (Arduino Uno R3)

Companion to `voice_blue_gold.ino`.

## Parts

- Arduino Uno R3
- MAX9814 microphone amplifier module
- 20-LED WS2811 strip (5V or 12V version, check your strip)
- External power supply matched to the strip (5V or 12V)
- 330 ohm resistor (LED data line)
- 1000 uF electrolytic capacitor, 10V or higher for 5V strips, 25V or higher for 12V strips
- Optional: logic-level MOSFET, transistor or relay module if the D7 output must drive anything beyond a small logic input

## Pin map

| Connection | Arduino Uno pin |
|---|---|
| MAX9814 OUT | A0 |
| MAX9814 VDD | 3.3V |
| MAX9814 GND | GND |
| WS2811 data in (via 330 ohm) | D6 |
| Output signal (HIGH on voice) | D7 |
| Strip power ground | GND (common with Uno) |

## 1. Microphone (MAX9814)

```
MAX9814        Uno
-------        ---
VDD    ------> 3.3V
GND    ------> GND
OUT    ------> A0
GAIN   ------> (see below)
AR     ------> (leave unconnected)
SHDN   ------> (leave unconnected if present)
```

- **Use 3.3V, not 5V** for the mic. It gives a quieter signal, and the Uno ADC still reads it fine.
- **GAIN pin** sets sensitivity (per the MAX9814 datasheet; some modules route this differently, so check your board's markings):
  - Unconnected: 60 dB (most sensitive, default)
  - Tied to GND: 50 dB
  - Tied to VDD: 40 dB (least sensitive)
  - If the room is noisy or people stand close to the mic, use a lower gain.
- **AR pin** (attack/release): leave unconnected for the default 1:4000 ratio.
- Mount the mic away from the LED strip, the output load and any motors or relays to limit electrical noise.

## 2. WS2811 LED strip

```
Supply +  -----------------------> Strip +V (5V or 12V, match the strip)
Supply -  -----+-----------------> Strip GND
               |
               +-----------------> Uno GND   (common ground, required)

Uno D6 --[330 ohm]---------------> Strip DIN (data in)

1000 uF capacitor across Supply + and Supply - (long leg to +), close to the strip
```

- **Common ground is required.** The strip's GND, the supply's negative and the Uno's GND must all be connected, or the data line will not work.
- **Strip voltage:** 12V strips must be powered from a 12V supply. Do not connect 12V to the Uno 5V pin or to the mic. The Uno's 5V logic on D6 normally drives a 12V WS2811 strip fine.
- **Current:**
  - 5V strip: 20 LEDs can draw up to ~1.2A at full white. The sketch uses `BRIGHTNESS 128` and only blue/gold, so real draw is much lower, but use a supply rated at least 2A.
  - 12V strip: a 12V 1A supply is plenty.
- A 5V strip with a small current draw can be powered from the Uno 5V pin for bench tests only. For anything longer, use the external supply.
- Connect the strip power with the supply off, and keep the data wire short.
- Strips have an arrow showing data direction. Connect D6 to the end marked DIN or IN.

## 3. Output pin (D7)

D7 goes HIGH (5V) while a voice is detected and LOW otherwise.

- **Direct use:** fine for a logic input, a small LED with a resistor, or a transistor/MOSFET gate. An Uno pin supplies a maximum of 20 mA (40 mA absolute max).
- **Bigger loads (relay, motor, lamp):** use D7 to drive a transistor, MOSFET or relay module, and power the load from its own supply with common ground.
- To use a different pin, change `OUTPUT_PIN` in the sketch.

## 4. Power for the Uno

Power the Uno by USB or a 7-12V barrel jack. If the Uno is powered over USB while the strip has its own supply, still keep grounds common.

## 5. Setup and tuning

1. Install **FastLED** via Arduino IDE > Library Manager.
2. Save `voice_blue_gold.ino` inside a folder named `voice_blue_gold` (the IDE offers to do this), select **Arduino Uno**, and upload.
3. **Stay quiet for about 1 second after power-up.** The sketch measures room noise and sets its trigger level automatically.
4. Open Serial Monitor at **115200 baud** to see `level` and `threshold` values.
5. Speak normally from your usual distance. If it does not trigger, lower `MARGIN` or raise the mic gain. If it triggers on background noise, raise `MARGIN`, lower the mic gain, or increase `TRIGGER_WINDOWS`.
6. Set `DEBUG_SERIAL` to `false` once tuned.

Key settings at the top of the sketch:

| Setting | Purpose | Default |
|---|---|---|
| `MARGIN` | How far above room noise counts as voice | 40 |
| `MIN_THRESHOLD` | Floor for the trigger level | 60 |
| `TRIGGER_WINDOWS` | Consecutive loud 50 ms windows needed | 2 |
| `HOLD_MS` | Time everything stays on after voice stops | 3000 |
| `FLASH_MS` | Blue/gold swap speed | 150 |
| `BRIGHTNESS` | Strip brightness | 128 |
| `COLOR_ORDER` | Strip color order | GRB |

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Colors wrong (e.g. red instead of blue) | Change `COLOR_ORDER` to `RGB` or `BRG` |
| Strip does nothing | Check common ground, data going to DIN (not DOUT), strip supply voltage |
| First LED flickers or random colors | Add the 330 ohm resistor, shorten the data wire, add the capacitor |
| Never triggers | Check A0 wiring and the Serial `level` values; increase gain or lower `MARGIN` |
| Triggers constantly | Room noise changed since startup (reset the Uno to recalibrate), or raise `MARGIN` |
| Triggers from claps or door slams | Raise `TRIGGER_WINDOWS` to 3 or 4 |
| Gold looks too yellow or too red | Adjust `GOLD_COLOR`'s green value (e.g. 120 to 180) |

## Notes

- The sketch detects **sustained sound above room noise**, not speech specifically. A MAX9814 and an Uno cannot tell a voice from other loud sounds, so music or loud machinery will also trigger it.

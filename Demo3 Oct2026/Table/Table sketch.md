# Connection Guide: Presence-Activated Dual Blue/Gold WS2811 (Arduino Uno R3)

Companion to `presence_blue_gold.ino`.

## Parts

- Arduino Uno R3
- HC-SR04 ultrasonic distance sensor
- Two identical 20-LED WS2811 strips (5V or 12V, check your strips)
- External power supply matched to the strips (5V or 12V)
- Two 330 ohm resistors (one per strip data line)
- 1000 uF electrolytic capacitor (10V or higher for 5V strips, 25V or higher for 12V strips)
- Optional: logic-level MOSFET, transistor or relay module if the D7 output must drive anything beyond a small logic input

## Pin map

| Connection | Arduino Uno pin |
|---|---|
| HC-SR04 VCC | 5V |
| HC-SR04 GND | GND |
| HC-SR04 TRIG | D3 |
| HC-SR04 ECHO | D4 |
| Strip 1 data in (via 330 ohm) | D6 |
| Strip 2 data in (via 330 ohm) | D5 |
| Output signal (HIGH while running) | D7 |
| Strip power ground | GND (common with Uno) |

## 1. HC-SR04 sensor

```
HC-SR04        Uno
-------        ---
VCC    ------> 5V
TRIG   ------> D3
ECHO   ------> D4
GND    ------> GND
```

- The HC-SR04 is powered from the Uno's 5V pin. Its 5V echo signal is fine for the Uno's input.
- Aim the sensor at where people will stand or walk, at roughly chest height. The two "eyes" should face the area, unobstructed by cases or panels.
- Keep a clear path of at least the detection distance (default 120 cm). Nearby walls or furniture within that range will read as a constant "presence".
- Soft clothing and angled surfaces reflect sound poorly, so sensing can be less reliable at long range or at steep angles.
- Keep the sensor away from the LED strips and power wiring.

## 2. Two WS2811 strips

```
Supply +  ----+--------------------> Strip 1 +V
              +--------------------> Strip 2 +V
Supply -  ----+--------------------> Strip 1 GND
              +--------------------> Strip 2 GND
              +--------------------> Uno GND   (common ground, required)

Uno D6 --[330 ohm]----------------> Strip 1 DIN
Uno D5 --[330 ohm]----------------> Strip 2 DIN

1000 uF capacitor across Supply + and Supply - (long leg to +), close to the strips
```

- Each strip has its own data pin, so they run in identical, independent patterns.
- **Common ground is required.** The strips' GND, the supply's negative and the Uno's GND must all be connected.
- **Strip voltage:** 12V strips must be powered from a 12V supply. Do not connect 12V to the Uno 5V pin or the sensor. The Uno's 5V logic on D5/D6 normally drives 12V WS2811 strips fine.
- **Current:**
  - 5V strips: 40 LEDs total can draw up to ~2.4A at full white. With blue and gold at `BRIGHTNESS 128` the real draw is much lower, but use a supply rated at least 3A. For longer runs, feed power to both ends or to each strip separately.
  - 12V strips: a 12V 2A supply is plenty.
- Strips have an arrow showing data direction. Connect D5 and D6 to the end marked DIN or IN.
- Make connections with the supply off, and keep data wires short.

## 3. Output pin (D7)

D7 goes HIGH (5V) for the full 3 minutes and LOW otherwise.

- **Direct use:** fine for a logic input, a small LED with a resistor, or a transistor/MOSFET gate. An Uno pin supplies a maximum of 20 mA (40 mA absolute max).
- **Bigger loads (relay, motor, lamp):** use D7 to drive a transistor, MOSFET or relay module, and power the load from its own supply with common ground.

## 4. Power for the Uno

Power the Uno by USB or a 7-12V barrel jack. If the strips have their own supply, keep grounds common.

## 5. Behavior

1. **Idle:** sensor reads about every 60 ms. Outputs are off.
2. **Detection:** three consecutive readings within `DETECT_CM` start the show: D7 goes HIGH and both strips flash blue/gold.
3. **Run:** the show lasts exactly 3 minutes (`RUN_MS`). The sensor is not read while running, so people moving in or out do not extend or cut it short.
4. **Re-arm:** after a run, the sensor must see the area clear for 3 seconds (`CLEAR_MS`) before it can trigger again. This stops someone who is standing in front of it from restarting the show right away.

## 6. Setup and tuning

1. Install **FastLED** via Arduino IDE > Library Manager.
2. Save `presence_blue_gold.ino` inside a folder named `presence_blue_gold` (the IDE offers to do this), select **Arduino Uno**, and upload.
3. Open Serial Monitor at **115200 baud**. Walk in front of the sensor and watch the `distance cm` values and `PRESENT` flags.
4. Adjust `DETECT_CM` to set the trigger distance. Raise `DETECT_COUNT` if it triggers falsely.
5. Set `DEBUG_SERIAL` to `false` once tuned.

Key settings at the top of the sketch:

| Setting | Purpose | Default |
|---|---|---|
| `DETECT_CM` | Trigger distance in cm | 120 |
| `DETECT_COUNT` | Consecutive in-range readings needed | 3 |
| `RUN_MS` | Show duration | 180000 (3 minutes) |
| `CLEAR_MS` | Area must be clear this long to re-arm | 3000 |
| `FLASH_MS` | Blue/gold swap speed | 150 |
| `BRIGHTNESS` | Strip brightness | 128 |
| `COLOR_ORDER` | Strip color order | GRB |

## Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Colors wrong (e.g. red instead of blue) | Change `COLOR_ORDER` to `RGB` or `BRG` |
| One strip does nothing | Check its data wire (D5 or D6), DIN end, ground and power |
| Both strips do nothing | Check common ground and strip supply voltage |
| First LED flickers or random colors | Use the 330 ohm resistors, shorten data wires, add the capacitor |
| Distance always 0 | Check TRIG/ECHO wiring and 5V to the sensor; nothing within range also reads 0 |
| Triggers constantly | An object is inside `DETECT_CM`; reduce `DETECT_CM` or reposition the sensor |
| Does not trigger | Check `distance cm` in Serial Monitor; raise `DETECT_CM` or aim the sensor better |
| Does not retrigger after a run | Area must stay clear for `CLEAR_MS`; check that nothing is within `DETECT_CM` |
| Gold looks too yellow or too red | Adjust `GOLD_COLOR`'s green value (e.g. 120 to 180) |

## Notes

- The HC-SR04 detects any object in range, not specifically people, so a moving object or a parked cart will trigger it as well.
- Flashing lights at this rate can affect people sensitive to flashing patterns. Consider this for public installations and lengthen `FLASH_MS` if needed.

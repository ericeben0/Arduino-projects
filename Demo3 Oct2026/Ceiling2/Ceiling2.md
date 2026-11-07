I’ve updated the sketch so the blue/white show runs while the third track plays. It’s in sound_triggered_leds_3tracks.ino.

One wiring change from last time: connect the DFPlayer’s BUSY pin to Nano D2. It goes LOW while a track is playing and returns HIGH when it ends, which is how the sketch knows when track 3 is done.

Why I switched to the BUSY pin: FastLED briefly disables interrupts each time it updates 100 LEDs. That can make the serial link miss the DFPlayer’s “track finished” message, and the show would never end. The BUSY pin is a plain wire, so it can’t be missed.

Updated wiring:

WS2812 DIN  -> Nano D6 (via ~330Ω resistor)
WS2812 5V   -> external 5V supply (not the Nano)
Supply GND  -> strip GND AND Nano GND (shared ground)
MAX9814 OUT -> Nano A0     (VDD to 3V3, GND to GND)
DFPlayer TX   -> Nano D10
DFPlayer RX   -> Nano D11 via ~1kΩ
DFPlayer BUSY -> Nano D2
DFPlayer VCC/GND -> Nano 5V/GND, speaker on SPK_1/SPK_2

How it behaves:

Idle: the first 5 LEDs alternate white and green, and the rest stay dark.
Tracks 1 and 2: the strip goes dark while they play.
Track 3: each frame lights 30 random LEDs out of 100, alternating blue and white, and the picks change every 120 ms. The show lasts as long as the track does.
After track 3: the strip clears, there’s a 2-second cooldown so the speaker doesn’t retrigger the mic, then it returns to idle.

Tuning and safety notes:

SOUND_THRESHOLD starts at 150. Watch the Serial Monitor readings to see your room’s quiet level versus a clap or voice, then set it in between.
If a track never starts, the sketch prints a warning after 2 seconds and moves on. That usually means the file names or SD card are the problem. You need 0001.mp3, 0002.mp3 and 0003.mp3 in the card’s root folder.
Brightness is capped at 100/255, and FastLED is limited to 1.5A. Raise setMaxPowerInVoltsAndMilliamps only if your supply can handle more.
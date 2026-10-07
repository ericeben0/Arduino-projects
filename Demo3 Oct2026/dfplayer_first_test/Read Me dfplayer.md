Here's a standalone first-test sketch for the DFPlayer Mini on a Nano — nothing else wired in, just confirming playback works before you integrate it into anything bigger.
Wiring:
Arduino Nano          DFPlayer Mini
-----------          -------------
5V         ---------> VCC
GND        ---------> GND
Pin 10 (RX)---------> TX
Pin 11 (TX)--1kΩ---->  RX   *(see note below)*
                      SPK_1 --> Speaker (+)
                      SPK_2 --> Speaker (-)
Important note on the RX line: The DFPlayer Mini's RX pin expects 3.3V logic, but the Nano outputs 5V on its TX pin. Running it directly often still works, but a lot of people get flaky behavior without a simple voltage divider or a single ~1kΩ resistor in series on that line. If you have a 1kΩ resistor handy, put it between Nano Pin 11 and the DFPlayer's RX — cheap insurance against corrupted serial commands.
Power note: the Nano's 5V pin can struggle to drive both the Nano and a speaker-driven DFPlayer reliably over USB alone. For this bench test it's usually fine; if you get resets or the module cutting out when audio plays, feed the DFPlayer from a separate 5V supply (common ground) instead of the Nano's regulator.
Sketch:
A few things to watch on this first run:
•	Library: install DFRobotDFPlayerMini by DFRobot through the Arduino IDE Library Manager before uploading.
•	Delays are placeholders: the delay(5000) calls just give each track time to play — adjust to match your actual clip lengths, or better yet, swap to myDFPlayer.available() checks once you move past this basic test.
•	If it hangs at "DFPlayer Mini not detected!": double-check the card is seated fully, confirm 0001.mp3/0002.mp3/0003.mp3 are in the root (not a subfolder), and verify the RX/TX lines aren't swapped — that's the most common DFPlayer wiring mistake.
Once this confirms clean playback, want help folding it into the combined Trinket routine with the WS2811 sequences and servo?


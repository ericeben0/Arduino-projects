Ceiling1

I rewrote voice_triggered_angry_nano.ino (Ceiling1) around your new timeline. I haven’t compiled it.

Timeline after a voice trigger

0 to 30 seconds: the buzzer is on and the angry pattern loops continuously on the 25-LED string. It repeats the red flashes, the random orange-red chase and the strobe burst.
From 5 seconds: the servo makes 4 full-range sweeps (to max, then to min, four times), then returns to center.
30 to 60 seconds: the buzzer is off and the whole string stays solid red.
At 60 seconds: the strip fades out, a 1.5 second cooldown passes, and it returns to the white/red standby flashing.

The microphone isn’t read at all during the 60 seconds, so no repeat trigger is possible. The voice history is also cleared afterward, so the sequence’s own noise can’t retrigger it.

Why the code is structured differently
The LED pattern, buzzer and servo now overlap, so I replaced the old one-after-another blocking phases with millis()-based state machines. The servo also now creeps toward each target 6° per 20ms pulse instead of jumping. That makes a full sweep take about half a second and is gentler on the supply than sudden jumps.

Assumptions to check

“Max range” is set to 0° to 180° (SERVO_MIN_ANGLE and SERVO_MAX_ANGLE). If the servo buzzes or strains at the ends, narrow those two values.
One sweep is a round trip, to max and back to min, repeated 4 times.
The pulse mapping uses the standard 1000 to 2000µs range. Some servos won’t reach a full 180° on it, and widening the range in sendServoPulse() would fix that.

The wiring is unchanged, and the only addition is the buzzer on pin 7 from before. The servo now moves while the LEDs are animating, so your 3A supply with all grounds tied together matters more than before.
/*
  Friendly/Angry LED sequences + servo motion, phase-based (DFPlayer to be added later)
  Target: Arduino Nano (ATmega328P)
  Libraries needed: FastLED, Servo (built into the Arduino IDE)

  Wiring (adjust pin numbers to match your actual build):
    Pin 5 -> WS2811 data in, "friendly" strip (20 LEDs)
    Pin 6 -> WS2811 data in, "angry" strip (20 LEDs)
    Pin 9 -> servo signal

  Pins 0 (RX) and 1 (TX) are deliberately left untouched here --
  they're the Nano's hardware Serial pins, reserved for the
  DFPlayer Mini once that phase is added. Hardware Serial avoids
  the SoftwareSerial/interrupt-sharing concerns that would have
  applied on the Trinket.

  IMPORTANT: this version does NOT use the Servo library. The Servo
  library and FastLED both want to claim Timer1 on the ATmega328P for
  interrupt-driven pulse generation, which causes a
  "multiple definition of `__vector_11`" linker error when both are
  included. Since the servo here only ever needs to move during its
  own exclusive phase (nothing else runs concurrently), it's driven
  with plain digitalWrite() pulses timed by delayMicroseconds()/delay()
  instead -- no timer or interrupt needed at all, so there's nothing
  left to conflict with FastLED.
*/

#include <FastLED.h>

// ---------- Pin / count configuration ----------
#define LED_PIN_FRIENDLY   5
#define LED_PIN_ANGRY      6
#define SERVO_PIN          9
#define NUM_LEDS          20

#define SERVO_MIN_ANGLE    30
#define SERVO_MAX_ANGLE   150

// ---------- Globals ----------
CRGB friendlyLeds[NUM_LEDS];
CRGB angryLeds[NUM_LEDS];

enum Phase { PHASE_FRIENDLY, PHASE_ANGRY, PHASE_SERVO };
Phase currentPhase = PHASE_FRIENDLY;

void setup() {
  FastLED.addLeds<WS2811, LED_PIN_FRIENDLY, RGB>(friendlyLeds, NUM_LEDS);
  FastLED.addLeds<WS2811, LED_PIN_ANGRY, RGB>(angryLeds, NUM_LEDS);
  FastLED.setBrightness(120);
  fill_solid(friendlyLeds, NUM_LEDS, CRGB::Black);
  fill_solid(angryLeds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  pinMode(SERVO_PIN, OUTPUT);
  moveServoTo(90, 400);   // start centered
}

// Sends standard 50Hz hobby-servo pulses (1-2ms high out of a 20ms
// period) for holdMillis, which is enough time for the servo to
// physically reach the target angle and settle.
void moveServoTo(int angleDegrees, int holdMillis) {
  int pulseWidthUs = map(angleDegrees, 0, 180, 1000, 2000);
  int cycles = holdMillis / 20;
  for (int i = 0; i < cycles; i++) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseWidthUs);
    digitalWrite(SERVO_PIN, LOW);
    delay(20 - (pulseWidthUs / 1000));  // fill out the ~20ms period
  }
}

void loop() {
  switch (currentPhase) {
    case PHASE_FRIENDLY:
      runFriendlySequence();
      currentPhase = PHASE_ANGRY;
      break;

    case PHASE_ANGRY:
      runAngrySequence();
      currentPhase = PHASE_SERVO;
      break;

    case PHASE_SERVO:
      runServoMove();
      currentPhase = PHASE_FRIENDLY;
      break;
  }
}

// ================= FRIENDLY: warm, slow, gentle =================
void runFriendlySequence() {
  fill_solid(friendlyLeds, NUM_LEDS, CRGB::Black);

  // Gentle warm chase, LED by LED
  for (int i = 0; i < NUM_LEDS; i++) {
    friendlyLeds[i] = CHSV(35, 200, 200);  // warm amber
    FastLED.show();
    delay(80);
  }

  // Soft breathing pulse over the whole strip
  for (int b = 0; b < 40; b++) {
    uint8_t level = beatsin8(20, 60, 255);  // slow sine, ~20 BPM
    fill_solid(friendlyLeds, NUM_LEDS, CHSV(35, 200, level));
    FastLED.show();
    delay(20);
  }

  fadeToBlackAll(friendlyLeds, 8, 40);
}

// ================= ANGRY: hot, sharp, erratic =================
void runAngrySequence() {
  // Sharp red flash x3
  for (int f = 0; f < 3; f++) {
    fill_solid(angryLeds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(50);
    fill_solid(angryLeds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(50);
  }

  // Fast erratic chase, random order/timing
  for (int i = 0; i < NUM_LEDS; i++) {
    int idx = random(0, NUM_LEDS);
    angryLeds[idx] = CRGB::OrangeRed;
    FastLED.show();
    delay(random(10, 40));
  }

  // Rapid strobe burst
  for (int s = 0; s < 8; s++) {
    fill_solid(angryLeds, NUM_LEDS, (s % 2 == 0) ? CRGB::Red : CRGB::Black);
    FastLED.show();
    delay(random(15, 60));
  }

  fadeToBlackAll(angryLeds, 20, 10);
}

// ================= SERVO: simple gesture =================
void runServoMove() {
  moveServoTo(SERVO_MAX_ANGLE, 400);
  moveServoTo(SERVO_MIN_ANGLE, 400);
  moveServoTo(90, 400);   // return to center
}

// ================= Helper: fade all LEDs to black =================
void fadeToBlackAll(CRGB* leds, uint8_t fadeAmount, int stepDelay) {
  for (int step = 0; step < 15; step++) {
    fadeToBlackBy(leds, NUM_LEDS, fadeAmount);
    FastLED.show();
    delay(stepDelay);
  }
}

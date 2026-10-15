/*
  Voice-triggered angry LED pattern + servo gesture
  Target: Arduino Nano (ATmega328P)
  Library needed: FastLED (the Servo library is intentionally NOT used --
  it collides with FastLED over Timer1 and causes a __vector_11 linker error)

  What it does:
    1. Listens to a MAX9814 microphone on A0, measuring peak-to-peak
       amplitude over short 50ms windows.
    2. If sound stays above VOICE_THRESHOLD in enough recent windows
       (a sustained burst, like speech, rather than a single clap or bump),
       it triggers the sequence.
    3. Sequence: angry LED pattern on one WS2812 string, then a servo gesture.
    4. Cooldown, then it listens again.

  Wiring (adjust to match your build):
    Pin 5  -> WS2812 data in (through ~330 ohm resistor)
    Pin 9  -> servo signal
    A0     -> MAX9814 OUT
    A1     -> leave unconnected (used only to seed the random generator)
    Pins 0/1 untouched (reserved for the DFPlayer Mini later)

  NOTE ON "VOICE DETECTION": this detects sustained loudness, not voice
  specifically. TV audio, music, or a loud conversation nearby will trigger
  it too. Tune VOICE_THRESHOLD with DEBUG_SERIAL turned on.
*/

#include <FastLED.h>

// ---------- LED configuration ----------
#define LED_PIN         5
#define NUM_LEDS       20
#define COLOR_ORDER   GRB     // WS2812 is normally GRB; change here if colors look wrong
#define LED_BRIGHTNESS 120    // 0-255

// ---------- Servo configuration ----------
#define SERVO_PIN       9
#define SERVO_MIN_ANGLE 30
#define SERVO_MAX_ANGLE 150

// ---------- Microphone configuration ----------
#define MIC_PIN             A0
#define SAMPLE_WINDOW_MS    50    // length of each loudness measurement
#define VOICE_THRESHOLD     100   // peak-to-peak ADC counts (0-1023); tune this
#define WINDOWS_TRACKED      6    // how many recent windows to remember
#define WINDOWS_REQUIRED     3    // how many of those must be loud to trigger
#define COOLDOWN_MS       1500    // ignore the mic after a sequence (servo/LED noise)

#define DEBUG_SERIAL         1    // 1 = print mic levels to Serial Monitor (set to 0 when the DFPlayer uses pins 0/1)

// ---------- Globals ----------
CRGB leds[NUM_LEDS];
uint8_t voiceHistory = 0;   // bit history of recent windows, 1 = loud

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
#endif

  randomSeed(analogRead(A1));   // floating pin gives a different seed each power-up

  FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  pinMode(SERVO_PIN, OUTPUT);
  moveServoTo(90, 400);   // start centered
}

void loop() {
  int level = measurePeakToPeak();

#if DEBUG_SERIAL
  Serial.println(level);
#endif

  // Shift in this window's result (1 = loud), keep only the last N windows
  voiceHistory = ((voiceHistory << 1) | (level > VOICE_THRESHOLD ? 1 : 0))
                 & ((1 << WINDOWS_TRACKED) - 1);

  if (__builtin_popcount(voiceHistory) >= WINDOWS_REQUIRED) {
    runAngrySequence();
    runServoMove();
    delay(COOLDOWN_MS);
    voiceHistory = 0;   // start fresh so the sequence's own noise doesn't retrigger
  }
}

// ================= MIC: peak-to-peak loudness over one window =================
// The MAX9814 output sits on a DC bias (~1.25V), so the absolute reading
// doesn't matter -- the swing between highest and lowest sample does.
int measurePeakToPeak() {
  unsigned long start = millis();
  int sampleMin = 1023;
  int sampleMax = 0;
  while (millis() - start < SAMPLE_WINDOW_MS) {
    int s = analogRead(MIC_PIN);
    if (s < sampleMin) sampleMin = s;
    if (s > sampleMax) sampleMax = s;
  }
  return sampleMax - sampleMin;
}

// ================= ANGRY: hot, sharp, erratic =================
void runAngrySequence() {
  // Sharp red flash x3
  for (int f = 0; f < 3; f++) {
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(50);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(50);
  }

  // Fast erratic chase, random order/timing
  for (int i = 0; i < NUM_LEDS; i++) {
    int idx = random(0, NUM_LEDS);
    leds[idx] = CRGB::OrangeRed;
    FastLED.show();
    delay(random(10, 40));
  }

  // Rapid strobe burst
  for (int s = 0; s < 8; s++) {
    fill_solid(leds, NUM_LEDS, (s % 2 == 0) ? CRGB::Red : CRGB::Black);
    FastLED.show();
    delay(random(15, 60));
  }

  fadeToBlackAll(20, 10);
}

// ================= SERVO: simple gesture =================
void runServoMove() {
  moveServoTo(SERVO_MAX_ANGLE, 400);
  moveServoTo(SERVO_MIN_ANGLE, 400);
  moveServoTo(90, 400);   // return to center
}

// Sends standard 50Hz hobby-servo pulses (1-2ms high out of a 20ms period)
// for holdMillis, long enough for the servo to reach and settle at the angle.
void moveServoTo(int angleDegrees, int holdMillis) {
  int pulseWidthUs = map(angleDegrees, 0, 180, 1000, 2000);
  int cycles = holdMillis / 20;
  for (int i = 0; i < cycles; i++) {
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(pulseWidthUs);
    digitalWrite(SERVO_PIN, LOW);
    delay(20 - (pulseWidthUs / 1000));
  }
}

// ================= Helper: fade all LEDs to black =================
void fadeToBlackAll(uint8_t fadeAmount, int stepDelay) {
  for (int step = 0; step < 15; step++) {
    fadeToBlackBy(leds, NUM_LEDS, fadeAmount);
    FastLED.show();
    delay(stepDelay);
  }
}

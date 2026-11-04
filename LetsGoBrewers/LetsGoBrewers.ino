/*
  voice_blue_gold.ino
  Arduino Uno R3 - standalone

  Listens to a MAX9814 microphone. When a voice (sustained sound above the
  room's background level) is detected:
    - OUTPUT_PIN goes HIGH
    - a 20-LED WS2811 strip flashes alternating blue / gold
  After the voice stops, everything stays on for HOLD_MS, then turns off.

  Library: FastLED (install from Library Manager)

  Wiring summary (full guide in connection_guide.md):
    MAX9814 OUT -> A0        MAX9814 VDD -> 3.3V     MAX9814 GND -> GND
    WS2811 DIN  -> D6 (via 330 ohm)                  Output signal <- D7
*/

#include <FastLED.h>

// ---------------------------- Pin / strip setup ----------------------------
#define MIC_PIN      A0      // MAX9814 OUT
#define OUTPUT_PIN   7       // goes HIGH while voice is active
#define LED_PIN      6       // WS2811 data in
#define NUM_LEDS     20
#define LED_TYPE     WS2811
#define COLOR_ORDER  GRB     // If colors look wrong, try RGB or BRG (see guide)
#define BRIGHTNESS   128     // 0-255. Lower = less current draw

// ------------------------------- Colors -----------------------------------
const CRGB BLUE_COLOR = CRGB(0, 0, 255);
const CRGB GOLD_COLOR = CRGB(255, 150, 0);

// ----------------------------- Tuning values -------------------------------
const unsigned long SAMPLE_WINDOW_MS = 50;    // listening window (peak-to-peak)
const unsigned int  MARGIN           = 40;    // counts above room noise needed (0-1023 scale)
const unsigned int  MIN_THRESHOLD    = 60;    // never trigger below this level
const byte          TRIGGER_WINDOWS  = 2;     // consecutive loud windows required (rejects clicks/pops)
const unsigned long HOLD_MS          = 3000;  // stay on this long after last voice
const unsigned long FLASH_MS         = 150;   // blue/gold swap interval
const byte          CALIBRATION_WINDOWS = 20; // ~1 second of quiet at startup

#define DEBUG_SERIAL true                     // prints levels at 115200 baud

// -------------------------------- State ------------------------------------
CRGB leds[NUM_LEDS];

unsigned int  threshold   = MIN_THRESHOLD;
byte          loudWindows = 0;
bool          active      = false;
bool          phase       = false;
unsigned long lastVoiceMs = 0;
unsigned long lastFlashMs = 0;

// Measure peak-to-peak mic swing over one window (0-1023 counts)
unsigned int samplePeakToPeak() {
  unsigned long start = millis();
  unsigned int sigMin = 1023;
  unsigned int sigMax = 0;
  while (millis() - start < SAMPLE_WINDOW_MS) {
    unsigned int s = analogRead(MIC_PIN);
    if (s > sigMax) sigMax = s;
    if (s < sigMin) sigMin = s;
  }
  return sigMax - sigMin;
}

// Alternate LEDs blue/gold, swapping on each phase change
void drawFrame() {
  for (int i = 0; i < NUM_LEDS; i++) {
    bool even = (i % 2 == 0);
    leds[i] = (even == phase) ? BLUE_COLOR : GOLD_COLOR;
  }
  FastLED.show();
}

void startActive(unsigned long now) {
  active = true;
  phase = false;
  lastFlashMs = now;
  digitalWrite(OUTPUT_PIN, HIGH);
  drawFrame();
}

void stopActive() {
  active = false;
  loudWindows = 0;
  digitalWrite(OUTPUT_PIN, LOW);
  FastLED.clear();
  FastLED.show();
}

void calibrate() {
  unsigned int noiseMax = 0;
  for (byte i = 0; i < CALIBRATION_WINDOWS; i++) {
    unsigned int level = samplePeakToPeak();
    if (level > noiseMax) noiseMax = level;
  }
  threshold = noiseMax + MARGIN;
  if (threshold < MIN_THRESHOLD) threshold = MIN_THRESHOLD;

#if DEBUG_SERIAL
  Serial.print(F("Room noise (p-p): "));
  Serial.print(noiseMax);
  Serial.print(F("  Trigger threshold: "));
  Serial.println(threshold);
#endif
}

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
#endif
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  delay(500);        // let the mic bias settle
  calibrate();       // stay quiet for about a second after power-up
}

void loop() {
  unsigned int level = samplePeakToPeak();
  unsigned long now = millis();
  bool loud = (level > threshold);

#if DEBUG_SERIAL
  Serial.print(F("level:"));
  Serial.print(level);
  Serial.print(F(" threshold:"));
  Serial.println(threshold);
#endif

  if (loud) {
    if (loudWindows < 255) loudWindows++;
    if (active) {
      lastVoiceMs = now;                       // keep extending while talking
    } else if (loudWindows >= TRIGGER_WINDOWS) {
      lastVoiceMs = now;
      startActive(now);
    }
  } else {
    loudWindows = 0;
  }

  if (active) {
    if (now - lastVoiceMs >= HOLD_MS) {
      stopActive();
    } else if (now - lastFlashMs >= FLASH_MS) {
      lastFlashMs = now;
      phase = !phase;
      drawFrame();
    }
  }
}

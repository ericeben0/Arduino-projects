/*
  presence_blue_gold.ino
  Arduino Uno R3 - standalone

  Watches for a person with an HC-SR04 ultrasonic sensor. When someone is
  detected within DETECT_CM:
    - OUTPUT_PIN goes HIGH
    - two identical 20-LED WS2811 strips flash alternating blue / gold
  Everything runs for RUN_MS (3 minutes), then turns off. After a run, the
  sensor must see the area clear for CLEAR_MS before it can trigger again,
  so someone standing still does not restart it immediately.

  Library: FastLED (install from Library Manager)

  Wiring summary (full guide in connection_guide_presence.md):
    HC-SR04 VCC -> 5V   GND -> GND   TRIG -> D3   ECHO -> D4
    Strip 1 DIN -> D6 (via 330 ohm)  Strip 2 DIN -> D5 (via 330 ohm)
    Output signal <- D7
*/

#include <FastLED.h>

// ---------------------------- Pin / strip setup ----------------------------
#define TRIG_PIN      3
#define ECHO_PIN      4
#define OUTPUT_PIN    7       // goes HIGH while the show is running
#define LED_PIN_1     6       // strip 1 data in
#define LED_PIN_2     5       // strip 2 data in
#define NUM_LEDS      20      // per strip
#define LED_TYPE      WS2811
#define COLOR_ORDER   GRB     // If colors look wrong, try RGB or BRG (see guide)
#define BRIGHTNESS    128     // 0-255. Lower = less current draw

// ------------------------------- Colors -----------------------------------
const CRGB BLUE_COLOR = CRGB(0, 0, 255);
const CRGB GOLD_COLOR = CRGB(255, 150, 0);

// ----------------------------- Tuning values -------------------------------
const unsigned int  DETECT_CM       = 120;       // person closer than this triggers (2-400)
const byte          DETECT_COUNT    = 3;         // consecutive readings in range needed
const unsigned long CLEAR_MS        = 3000;      // area must stay clear this long to re-arm
const unsigned long RUN_MS          = 180000UL;  // 3 minutes
const unsigned long FLASH_MS        = 150;       // blue/gold swap interval
const unsigned long PING_INTERVAL_MS = 60;       // time between sensor readings

#define DEBUG_SERIAL true                        // prints distance at 115200 baud

// -------------------------------- State ------------------------------------
CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];

bool          active       = false;
bool          armed        = true;    // false after a run until the area is clear
bool          phase        = false;
byte          detectHits   = 0;
unsigned long runStartMs   = 0;
unsigned long lastFlashMs  = 0;
unsigned long lastPingMs   = 0;
unsigned long clearSinceMs = 0;
bool          clearTiming  = false;

// Returns distance in cm, or 0 if no echo was received (nothing in range)
unsigned int readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(4);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);  // 30 ms timeout (~5 m)
  if (duration == 0) return 0;
  return (unsigned int)(duration / 58UL);                     // microseconds to cm
}

// Alternate LEDs blue/gold on both strips, swapping on each phase change
void drawFrame() {
  for (int i = 0; i < NUM_LEDS; i++) {
    bool even = (i % 2 == 0);
    CRGB c = (even == phase) ? BLUE_COLOR : GOLD_COLOR;
    leds1[i] = c;
    leds2[i] = c;
  }
  FastLED.show();
}

void startShow(unsigned long now) {
  active = true;
  armed = false;
  phase = false;
  runStartMs = now;
  lastFlashMs = now;
  clearTiming = false;
  detectHits = 0;
  digitalWrite(OUTPUT_PIN, HIGH);
  drawFrame();
#if DEBUG_SERIAL
  Serial.println(F("Person detected - show started"));
#endif
}

void stopShow() {
  active = false;
  digitalWrite(OUTPUT_PIN, LOW);
  FastLED.clear();
  FastLED.show();
#if DEBUG_SERIAL
  Serial.println(F("Show finished"));
#endif
}

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
#endif
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);

  FastLED.addLeds<LED_TYPE, LED_PIN_1, COLOR_ORDER>(leds1, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, LED_PIN_2, COLOR_ORDER>(leds2, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  unsigned long now = millis();

  if (active) {
    // Show running: just flash, and stop after RUN_MS
    if (now - runStartMs >= RUN_MS) {
      stopShow();
    } else if (now - lastFlashMs >= FLASH_MS) {
      lastFlashMs = now;
      phase = !phase;
      drawFrame();
    }
    return;
  }

  // Idle: take a reading every PING_INTERVAL_MS
  if (now - lastPingMs < PING_INTERVAL_MS) return;
  lastPingMs = now;

  unsigned int cm = readDistanceCm();
  bool present = (cm >= 2 && cm <= DETECT_CM);

#if DEBUG_SERIAL
  Serial.print(F("distance cm: "));
  Serial.print(cm);
  Serial.println(present ? F("  PRESENT") : F(""));
#endif

  if (!armed) {
    // Waiting for the area to clear after a run
    if (present) {
      clearTiming = false;
    } else {
      if (!clearTiming) {
        clearTiming = true;
        clearSinceMs = now;
      } else if (now - clearSinceMs >= CLEAR_MS) {
        armed = true;
        detectHits = 0;
#if DEBUG_SERIAL
        Serial.println(F("Re-armed"));
#endif
      }
    }
    return;
  }

  // Armed: require several consecutive in-range readings
  if (present) {
    if (detectHits < 255) detectHits++;
    if (detectHits >= DETECT_COUNT) startShow(now);
  } else {
    detectHits = 0;
  }
}

#include <FastLED.h>

// ---------- Pins ----------
#define PIN_US_TRIG    7
#define PIN_US_ECHO    8
#define PIN_RADAR_OUT  2
#define DATA_PIN_A     5   // strip A: HC-SR04
#define DATA_PIN_B     6   // strip B: LD2410

// ---------- LED setup ----------
#define NUM_LEDS_A     30
#define NUM_LEDS_B     30
#define BRIGHTNESS     120          // 0-255
#define MAX_AMPS_MA    3000         // total cap for both strips

CRGB ledsA[NUM_LEDS_A];
CRGB ledsB[NUM_LEDS_B];

// ---------- Tuning ----------
const unsigned int TRIGGER_CM       = 150;   // ultrasonic detect distance
const uint8_t      US_CONFIRM_HITS  = 2;     // consecutive hits required
const unsigned long PING_INTERVAL   = 60;    // ms between pings
const unsigned long HOLD_MS         = 4000;  // stay lit after last detection
const unsigned long STEP_MS         = 300;   // alternating pattern speed

const CRGB COLOR_A = CRGB::Cyan;
const CRGB COLOR_B = CRGB::Magenta;

// ---------- State ----------
unsigned long lastPing = 0, lastStep = 0;
unsigned long lastUsSeen = 0, lastRadarSeen = 0;
bool usEver = false, radarEver = false;
uint8_t usHits = 0;
bool phase = false;

long readDistanceCm() {
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);
  unsigned long d = pulseIn(PIN_US_ECHO, HIGH, 25000UL);  // ~4 m timeout
  if (d == 0) return -1;
  return d / 58;
}

void drawStrip(CRGB *leds, int n, CRGB color, bool active) {
  for (int i = 0; i < n; i++) {
    bool lit = active && (((i + phase) & 1) == 0);   // every other LED, swaps each step
    leds[i] = lit ? color : CRGB::Black;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_US_TRIG, OUTPUT);
  pinMode(PIN_US_ECHO, INPUT);
  pinMode(PIN_RADAR_OUT, INPUT);

  FastLED.addLeds<WS2812B, DATA_PIN_A, GRB>(ledsA, NUM_LEDS_A);
  FastLED.addLeds<WS2812B, DATA_PIN_B, GRB>(ledsB, NUM_LEDS_B);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_AMPS_MA);
  FastLED.clear(true);
}

void loop() {
  unsigned long now = millis();

  // --- HC-SR04 ---
  if (now - lastPing >= PING_INTERVAL) {
    lastPing = now;
    long cm = readDistanceCm();
    if (cm > 2 && cm <= (long)TRIGGER_CM) {
      if (usHits < 255) usHits++;
      if (usHits >= US_CONFIRM_HITS) { lastUsSeen = now; usEver = true; }
    } else {
      usHits = 0;
    }
    // Serial.println(cm);   // uncomment to tune TRIGGER_CM
  }

  // --- LD2410 (presence output pin) ---
  if (digitalRead(PIN_RADAR_OUT) == HIGH) { lastRadarSeen = now; radarEver = true; }

  bool activeA = usEver    && (now - lastUsSeen    < HOLD_MS);
  bool activeB = radarEver && (now - lastRadarSeen < HOLD_MS);

  // --- Animate ---
  if (now - lastStep >= STEP_MS) {
    lastStep = now;
    phase = !phase;
    drawStrip(ledsA, NUM_LEDS_A, COLOR_A, activeA);
    drawStrip(ledsB, NUM_LEDS_B, COLOR_B, activeB);
    FastLED.show();
  }
}

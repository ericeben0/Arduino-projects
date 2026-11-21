/*
  Voice-triggered angry LED pattern + buzzer + servo sweeps
  Target: Arduino Nano (ATmega328P)
  Library needed: FastLED (the Servo library is intentionally NOT used --
  it collides with FastLED over Timer1 and causes a __vector_11 linker error)

  Behavior:
    STANDBY  From power-up, 3 LEDs flash white and red. The microphone is
             not armed for the first 20 seconds after power-on; after that
             the MAX9814 on A0 listens for a voice.

    TRIGGER  When sound stays above VOICE_THRESHOLD in enough recent 50ms
             windows (a sustained burst like speech, not a single clap),
             a one-minute sequence runs. The microphone is ignored for the
             whole minute, so no repeat trigger is possible during it.

               t = 0s   buzzer ON, angry pattern starts on the 25-LED string
               t = 5s   servo begins 4 full-range sweeps (to max, then to min,
                        4 times), then returns to center
               t = 15s  buzzer OFF (angry pattern keeps running)
               t = 30s  angry pattern ends, whole string goes solid red
               t = 60s  string fades out, short cooldown, back to STANDBY

    Everything during the trigger runs at the same time (LEDs, buzzer, servo)
    using millis() timing -- nothing blocks anything else.

  Wiring (adjust to match your build):
    Pin 5  -> WS2812 data in (through ~330 ohm resistor)
    Pin 7  -> active buzzer (directly if it draws ~20mA or less,
              otherwise through an NPN transistor)
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
#define NUM_LEDS       25
#define COLOR_ORDER   GRB     // WS2812 is normally GRB; change here if colors look wrong
#define LED_BRIGHTNESS 120    // 0-255

// ---------- Standby flasher (shown until a voice is detected) ----------
#define STANDBY_LEDS        3     // first 3 LEDs on the strip
#define STANDBY_FLASH_MS  300     // time between white <-> red toggles

// ---------- Buzzer configuration ----------
#define BUZZER_PIN          7
#define BUZZER_ON_MS    15000UL   // buzzer runs for the first 15 seconds after trigger

// ---------- Triggered-sequence timing ----------
#define ANGRY_PHASE_MS  30000UL   // angry pattern: first 30 seconds
#define RED_PHASE_MS    30000UL   // solid red: next 30 seconds

// ---------- Servo configuration ----------
#define SERVO_PIN           9
#define SERVO_MIN_ANGLE     0     // full range; narrow these if the servo binds at the ends
#define SERVO_MAX_ANGLE   180
#define SERVO_CENTER_ANGLE 90
#define SERVO_START_MS   5000UL   // servo starts 5 seconds after trigger
#define SERVO_SWEEPS        4     // number of max-to-min round trips
#define SERVO_STEP_DEG      6     // degrees moved per 20ms pulse (6 = about 0.5s per full sweep)
#define SERVO_PERIOD_MS    20

// ---------- Microphone configuration ----------
#define MIC_PIN             A0
#define SAMPLE_WINDOW_MS    50    // length of each loudness measurement
#define VOICE_THRESHOLD     100   // peak-to-peak ADC counts (0-1023); tune this
#define WINDOWS_TRACKED      6    // how many recent windows to remember
#define WINDOWS_REQUIRED     3    // how many of those must be loud to trigger
#define COOLDOWN_MS       1500    // extra quiet time after the sequence ends
#define ARM_DELAY_MS     20000UL  // microphone ignored for this long after power-on

#define DEBUG_SERIAL         1    // 1 = print mic levels to Serial Monitor (set to 0 when the DFPlayer uses pins 0/1)

// ---------- Globals ----------
CRGB leds[NUM_LEDS];
uint8_t voiceHistory = 0;            // bit history of recent windows, 1 = loud

// Standby flasher state
unsigned long lastStandbyToggle = 0;
bool standbyWhite = false;

// Angry pattern state machine
enum AngryPhase { ANGRY_FLASH, ANGRY_CHASE, ANGRY_STROBE };
AngryPhase angryPhase = ANGRY_FLASH;
int angryStep = 0;
unsigned long angryNextTime = 0;

// Servo state machine
int servoAngle = SERVO_CENTER_ANGLE;
int servoTarget = SERVO_CENTER_ANGLE;
int servoLeg = 0;                    // number of completed movement legs
bool servoStarted = false;
bool servoActive = false;
unsigned long servoLastTick = 0;

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
#endif

  randomSeed(analogRead(A1));   // floating pin gives a different seed each power-up

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  pinMode(SERVO_PIN, OUTPUT);
  holdServoAt(SERVO_CENTER_ANGLE, 400);   // start centered
}

void loop() {
  updateStandbyFlasher();

  // Not armed yet: keep flashing the standby LEDs but ignore the microphone
  if (millis() < ARM_DELAY_MS) {
    return;
  }

  int level = measurePeakToPeak();

#if DEBUG_SERIAL
  Serial.println(level);
#endif

  // Shift in this window's result (1 = loud), keep only the last N windows
  voiceHistory = ((voiceHistory << 1) | (level > VOICE_THRESHOLD ? 1 : 0))
                 & ((1 << WINDOWS_TRACKED) - 1);

  if (__builtin_popcount(voiceHistory) >= WINDOWS_REQUIRED) {
    runTriggeredSequence();
    voiceHistory = 0;            // start fresh after the sequence
    lastStandbyToggle = millis();
  }
}

// ================= STANDBY: 3 LEDs flash white and red =================
// Non-blocking: toggles on a millis() timer between mic measurement windows.
void updateStandbyFlasher() {
  unsigned long now = millis();
  if (now - lastStandbyToggle >= STANDBY_FLASH_MS) {
    lastStandbyToggle = now;
    standbyWhite = !standbyWhite;
    CRGB c = standbyWhite ? CRGB::White : CRGB::Red;
    for (int i = 0; i < STANDBY_LEDS; i++) {
      leds[i] = c;
    }
    FastLED.show();
  }
}

// ================= TRIGGERED SEQUENCE (60 seconds, mic ignored) =================
void runTriggeredSequence() {
  // Clear the standby flasher
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  // Reset the pattern and servo state machines
  unsigned long t0 = millis();
  angryPhase = ANGRY_FLASH;
  angryStep = 0;
  angryNextTime = t0;
  servoStarted = false;
  servoActive = false;
  servoLeg = 0;
  servoLastTick = t0;

  digitalWrite(BUZZER_PIN, HIGH);   // buzzer on at trigger
  bool buzzerStopped = false;
  bool redPhaseStarted = false;

  while (millis() - t0 < ANGRY_PHASE_MS + RED_PHASE_MS) {
    unsigned long elapsed = millis() - t0;

    if (!buzzerStopped && elapsed >= BUZZER_ON_MS) {
      buzzerStopped = true;
      digitalWrite(BUZZER_PIN, LOW);               // buzzer off at 15s
    }

    if (elapsed < ANGRY_PHASE_MS) {
      updateAngryPattern();
    } else if (!redPhaseStarted) {
      redPhaseStarted = true;
      fill_solid(leds, NUM_LEDS, CRGB::Red);       // strip goes solid red at 30s
      FastLED.show();
    }

    updateServo(elapsed);
  }

  digitalWrite(BUZZER_PIN, LOW);   // safety: make sure it is off
  fadeToBlackAll(20, 10);
  delay(COOLDOWN_MS);
}

// ================= ANGRY: hot, sharp, erratic (non-blocking, loops continuously) =================
// Cycle: 3 red flashes -> random orange-red chase -> rapid strobe burst -> repeat.
void updateAngryPattern() {
  unsigned long now = millis();
  if ((long)(now - angryNextTime) < 0) return;   // not time for the next step yet

  switch (angryPhase) {
    case ANGRY_FLASH:
      fill_solid(leds, NUM_LEDS, (angryStep % 2 == 0) ? CRGB::Red : CRGB::Black);
      FastLED.show();
      angryNextTime = now + 50;
      if (++angryStep >= 6) {
        angryStep = 0;
        angryPhase = ANGRY_CHASE;
      }
      break;

    case ANGRY_CHASE:
      leds[random(0, NUM_LEDS)] = CRGB::OrangeRed;
      FastLED.show();
      angryNextTime = now + random(10, 40);
      if (++angryStep >= NUM_LEDS) {
        angryStep = 0;
        angryPhase = ANGRY_STROBE;
      }
      break;

    case ANGRY_STROBE:
      fill_solid(leds, NUM_LEDS, (angryStep % 2 == 0) ? CRGB::Red : CRGB::Black);
      FastLED.show();
      angryNextTime = now + random(15, 60);
      if (++angryStep >= 8) {
        angryStep = 0;
        angryPhase = ANGRY_FLASH;
      }
      break;
  }
}

// ================= SERVO: 4 full-range sweeps, started 5s after trigger =================
// Moves toward the target a few degrees per 20ms pulse. Legs: MAX, MIN
// repeated SERVO_SWEEPS times, then back to center, then pulses stop.
void updateServo(unsigned long elapsed) {
  if (!servoStarted && elapsed >= SERVO_START_MS) {
    servoStarted = true;
    servoActive = true;
    servoLeg = 0;
    servoTarget = SERVO_MAX_ANGLE;
    servoLastTick = millis();
  }
  if (!servoActive) return;

  unsigned long now = millis();
  if (now - servoLastTick < SERVO_PERIOD_MS) return;
  servoLastTick = now;

  // Step toward the target
  if (servoAngle < servoTarget) {
    servoAngle = min(servoAngle + SERVO_STEP_DEG, servoTarget);
  } else if (servoAngle > servoTarget) {
    servoAngle = max(servoAngle - SERVO_STEP_DEG, servoTarget);
  }
  sendServoPulse(servoAngle);

  // Reached the target: choose the next leg
  if (servoAngle == servoTarget) {
    servoLeg++;
    if (servoLeg < SERVO_SWEEPS * 2) {
      servoTarget = (servoLeg % 2 == 0) ? SERVO_MAX_ANGLE : SERVO_MIN_ANGLE;
    } else if (servoLeg == SERVO_SWEEPS * 2) {
      servoTarget = SERVO_CENTER_ANGLE;
    } else {
      servoActive = false;   // finished, stop pulsing
    }
  }
}

// One standard hobby-servo pulse: 1000-2000us high (call about every 20ms)
void sendServoPulse(int angleDegrees) {
  int pulseWidthUs = map(angleDegrees, 0, 180, 1000, 2000);
  digitalWrite(SERVO_PIN, HIGH);
  delayMicroseconds(pulseWidthUs);
  digitalWrite(SERVO_PIN, LOW);
}

// Blocking hold, used only in setup() to center the servo at power-up
void holdServoAt(int angleDegrees, int holdMillis) {
  int cycles = holdMillis / 20;
  for (int i = 0; i < cycles; i++) {
    sendServoPulse(angleDegrees);
    delay(18);
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

// ================= Helper: fade the strip out, then force fully black =================
void fadeToBlackAll(uint8_t fadeAmount, int stepDelay) {
  for (int step = 0; step < 15; step++) {
    fadeToBlackBy(leds, NUM_LEDS, fadeAmount);
    FastLED.show();
    delay(stepDelay);
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

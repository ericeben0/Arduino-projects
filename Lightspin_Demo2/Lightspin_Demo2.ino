/*
  Multi-Sensor Interactive Installation
  Board: Arduino Uno R3

  Layers (all non-blocking, millis()-based):
    1. Sound layer     -> sound sensor drives MG995 servo sweep + WS2811 VU meter
    2. Presence layer   -> PIR sensor drives DC gear motor (L298N, EN jumpered) + MG90 servo sweep
    3. Gesture layer    -> HC-SR04 distance drives passive buzzer "poor man's theremin"

  System goes IDLE after 10s with no sound/motion/gesture activity, and wakes on sound.

  Timer plan:
    Timer0 -> millis()/delay() (core, untouched)
    Timer1 -> PWMServo hardware channels: MG995 on D9 (OC1A), MG90 on D10 (OC1B)
              (PWMServo avoids the FastLED-vs-Servo-library Timer1 ISR conflict)
    Timer2 -> tone() for the passive buzzer (independent of Timer1)

  Note: FastLED.show() briefly disables interrupts (~30us/LED). If tone() is playing
  during a show() call this can cause a faint pitch stutter. Mitigated below by only
  calling show() when the VU level actually changes.
*/

#include <FastLED.h>
#include <PWMServo.h>

// ---------- Pin assignments ----------
const uint8_t PIN_SOUND_SENSOR   = A0;
const uint8_t PIN_PIR            = 2;
const uint8_t PIN_HCSR04_TRIG    = 4;
const uint8_t PIN_HCSR04_ECHO    = 5;
const uint8_t PIN_LED_DATA       = 6;
const uint8_t PIN_L298N_IN1      = 7;
const uint8_t PIN_L298N_IN2      = 8;
const uint8_t PIN_SERVO_MG995    = 9;   // PWMServo, Timer1 OC1A
const uint8_t PIN_SERVO_MG90     = 10;  // PWMServo, Timer1 OC1B
const uint8_t PIN_BUZZER         = 12;

// ---------- LED strip config ----------
#define NUM_LEDS   30
CRGB leds[NUM_LEDS];

// ---------- Servo objects ----------
PWMServo servoSound;     // MG995
PWMServo servoPresence;  // MG90

const int SERVO1_CENTER   = 90;
const int SERVO1_SWING    = 30;   // +/-30 degrees
const int SERVO2_CENTER   = 90;
const int SERVO2_SWING    = 40;   // +/-40 degrees

const unsigned long SERVO1_STEP_INTERVAL = 15; // ms between angle steps
const unsigned long SERVO2_STEP_INTERVAL = 15;
const int SERVO_STEP_DEGREES = 1;

int servo1Angle = SERVO1_CENTER;
int servo2Angle = SERVO2_CENTER;
int servo1Target = SERVO1_CENTER + SERVO1_SWING;
int servo2Target = SERVO2_CENTER + SERVO2_SWING;
unsigned long servo1LastMoveTime = 0;
unsigned long servo2LastMoveTime = 0;

// ---------- System state ----------
enum SystemState { STATE_IDLE, STATE_ACTIVE };
SystemState systemState = STATE_IDLE;

unsigned long lastActivityTime = 0;
const unsigned long IDLE_TIMEOUT_MS = 10000;

// ---------- Sound layer ----------
// gSoundLevel now holds PEAK-TO-PEAK amplitude over a rolling window, not a raw
// snapshot reading -- a single analogRead() can't measure volume, since it just
// catches the waveform at one random instant. Tune these two like the standalone
// VU meter sketch: watch gSoundLevel in Serial Monitor at rest vs. at your target
// volume, then adjust.
const unsigned long SOUND_WINDOW_MS = 50;   // matches SAMPLE_WINDOW in standalone sketch
const int SOUND_THRESHOLD  = 60;    // peak-to-peak amplitude that counts as "activity"
const int VU_MIN_VOLUME    = 10;    // peak-to-peak floor for map() -- tune to your mic
const int VU_MAX_VOLUME    = 400;   // peak-to-peak ceiling for map() -- tune to your mic
int lastVULevel = -1;

int envMax = 0;
int envMin = 1023;
unsigned long envWindowStart = 0;

// ---------- Presence layer ----------
bool personDetected = false;
bool motorRunning = false;
unsigned long lastPirHighTime = 0;
const unsigned long PERSON_HOLD_MS = 3000; // keep "present" for 3s after last HIGH read,
                                            // absorbs brief PIR dropouts/flicker

// ---------- Gesture layer ----------
const unsigned long HCSR04_MIN_INTERVAL = 60; // ms, min time between pings
unsigned long lastPingTime = 0;
const long THEREMIN_MIN_CM = 2;
const long THEREMIN_MAX_CM = 50;
const int THEREMIN_MIN_FREQ = 100;
const int THEREMIN_MAX_FREQ = 1500;
bool thereminActive = false;

// ---------- Debug ----------
#define DEBUG_SERIAL 1   // set to 0 to silence all debug prints once tuned
const unsigned long DEBUG_PRINT_INTERVAL = 250; // ms
unsigned long lastDebugPrintTime = 0;

// last-known raw readings, updated by each layer, printed by printDebug()
int    gSoundLevel = 0;
bool   gPirRaw = false;
long   gDistanceCM = -1;

// ============================================================
void setup() {
  Serial.begin(9600);

  pinMode(PIN_SOUND_SENSOR, INPUT);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_HCSR04_TRIG, OUTPUT);
  pinMode(PIN_HCSR04_ECHO, INPUT);

  // L298N: EN jumper is populated on the board, so only direction pins are driven.
  // Motor speed is fixed by the jumper; no PWM/ENA-ENB control needed.
  pinMode(PIN_L298N_IN1, OUTPUT);
  pinMode(PIN_L298N_IN2, OUTPUT);
  digitalWrite(PIN_L298N_IN1, LOW);
  digitalWrite(PIN_L298N_IN2, LOW);

  servoSound.attach(PIN_SERVO_MG995);
  servoPresence.attach(PIN_SERVO_MG90);
  servoSound.write(SERVO1_CENTER);
  servoPresence.write(SERVO2_CENTER);
  servo1Angle = SERVO1_CENTER;
  servo2Angle = SERVO2_CENTER;

  FastLED.addLeds<WS2811, PIN_LED_DATA, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();

  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  // Ensure personDetected starts false rather than true: sentinel value is far
  // enough in the past (relative to millis() at boot) that the hold window has
  // already elapsed. Unsigned subtraction wraps correctly either way.
  lastPirHighTime = millis() - (PERSON_HOLD_MS + 1000);

  systemState = STATE_IDLE;
}

// ============================================================
void loop() {
  unsigned long currentTime = millis();

  // Sound envelope sampling runs every pass, in both IDLE and ACTIVE, so the
  // system can wake from IDLE based on actual loudness, not a single snapshot.
  updateSoundEnvelope(currentTime);

  if (systemState == STATE_IDLE) {
    if (gSoundLevel > SOUND_THRESHOLD) {
      systemState = STATE_ACTIVE;
      lastActivityTime = currentTime;
    }
    printDebug(currentTime);
    return; // skip everything else while idle
  }

  // STATE_ACTIVE
  runSoundLayer(currentTime);
  runPresenceLayer(currentTime);
  runGestureLayer(currentTime);
  checkIdleTimeout(currentTime);
  printDebug(currentTime);
}

// ============================================================
// Sound envelope: non-blocking peak-to-peak amplitude over a rolling window.
// Call every loop pass. Updates gSoundLevel once per SOUND_WINDOW_MS.
// ============================================================
void updateSoundEnvelope(unsigned long currentTime) {
  int sample = analogRead(PIN_SOUND_SENSOR);
  if (sample > envMax) envMax = sample;
  if (sample < envMin) envMin = sample;

  if (currentTime - envWindowStart >= SOUND_WINDOW_MS) {
    gSoundLevel = envMax - envMin; // peak-to-peak amplitude for the window just closed
    envMax = 0;
    envMin = 1023;
    envWindowStart = currentTime;
  }
}

// ============================================================
// Debug: print raw sensor values + derived state on an interval
// ============================================================
void printDebug(unsigned long currentTime) {
#if DEBUG_SERIAL
  if (currentTime - lastDebugPrintTime < DEBUG_PRINT_INTERVAL) {
    return;
  }
  lastDebugPrintTime = currentTime;

  Serial.print("state=");
  Serial.print(systemState == STATE_IDLE ? "IDLE" : "ACTIVE");
  Serial.print(" | sound=");
  Serial.print(gSoundLevel);
  Serial.print(" (thresh=");
  Serial.print(SOUND_THRESHOLD);
  Serial.print(")");
  Serial.print(" | pir=");
  Serial.print(gPirRaw ? "HIGH" : "low");
  Serial.print(" person=");
  Serial.print(personDetected ? "Y" : "n");
  Serial.print("(hold=");
  Serial.print(currentTime - lastPirHighTime);
  Serial.print("ms)");
  Serial.print(" motor=");
  Serial.print(motorRunning ? "ON" : "off");
  Serial.print(" | dist=");
  if (gDistanceCM < 0) {
    Serial.print("---");
  } else {
    Serial.print(gDistanceCM);
  }
  Serial.print("cm theremin=");
  Serial.print(thereminActive ? "ON" : "off");
  Serial.print(" | servo1=");
  Serial.print(servo1Angle);
  Serial.print(" servo2=");
  Serial.print(servo2Angle);
  Serial.print(" | sinceActivity=");
  Serial.print(currentTime - lastActivityTime);
  Serial.println("ms");
#endif
}

// ============================================================
// Sound layer: MG995 sweep + VU meter
// ============================================================
void runSoundLayer(unsigned long currentTime) {
  // gSoundLevel is maintained by updateSoundEnvelope(), called once per loop pass
  // in loop() -- no snapshot analogRead() here, we reuse the windowed value.

  if (gSoundLevel > SOUND_THRESHOLD) {
    lastActivityTime = currentTime;
  }

  // --- Servo1 (MG995) non-blocking back-and-forth sweep ---
  if (currentTime - servo1LastMoveTime >= SERVO1_STEP_INTERVAL) {
    if (servo1Angle < servo1Target) {
      servo1Angle += SERVO_STEP_DEGREES;
      if (servo1Angle > servo1Target) servo1Angle = servo1Target;
    } else if (servo1Angle > servo1Target) {
      servo1Angle -= SERVO_STEP_DEGREES;
      if (servo1Angle < servo1Target) servo1Angle = servo1Target;
    }
    servoSound.write(servo1Angle);

    if (servo1Angle == servo1Target) {
      // flip target between +50 and -50 from center
      if (servo1Target == SERVO1_CENTER + SERVO1_SWING) {
        servo1Target = SERVO1_CENTER - SERVO1_SWING;
      } else {
        servo1Target = SERVO1_CENTER + SERVO1_SWING;
      }
    }
    servo1LastMoveTime = currentTime;
  }

  // --- VU meter bar graph, throttled to changes only ---
  int numLit = map(constrain(gSoundLevel, VU_MIN_VOLUME, VU_MAX_VOLUME),
                    VU_MIN_VOLUME, VU_MAX_VOLUME, 0, NUM_LEDS);
  numLit = constrain(numLit, 0, NUM_LEDS);

  if (numLit != lastVULevel) {
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < numLit) {
        // simple green -> yellow -> red gradient across the strip
        if (i < NUM_LEDS / 2) {
          leds[i] = CRGB::Green;
        } else if (i < (NUM_LEDS * 3) / 4) {
          leds[i] = CRGB::Yellow;
        } else {
          leds[i] = CRGB::Red;
        }
      } else {
        leds[i] = CRGB::Black;
      }
    }
    FastLED.show();
    lastVULevel = numLit;
  }
}

// ============================================================
// Presence layer: PIR -> DC motor (L298N) + MG90 sweep
// ============================================================
void runPresenceLayer(unsigned long currentTime) {
  bool pirState = digitalRead(PIN_PIR) == HIGH;
  gPirRaw = pirState;

  if (pirState) {
    lastPirHighTime = currentTime;
    lastActivityTime = currentTime;
  }

  // Software hold: stay "present" for PERSON_HOLD_MS after the last HIGH reading.
  // Smooths over brief PIR flicker/dropouts so the motor doesn't stutter on/off
  // while someone is still standing in front of the sensor.
  personDetected = (currentTime - lastPirHighTime) < PERSON_HOLD_MS;

  // --- DC gear motor via L298N (EN jumper populated, fixed speed) ---
  if (personDetected && !motorRunning) {
    digitalWrite(PIN_L298N_IN1, HIGH);
    digitalWrite(PIN_L298N_IN2, LOW);
    motorRunning = true;
  } else if (!personDetected && motorRunning) {
    digitalWrite(PIN_L298N_IN1, LOW);
    digitalWrite(PIN_L298N_IN2, LOW);
    motorRunning = false;
  }

  // --- Servo2 (MG90) non-blocking sweep, only while person present ---
  if (personDetected) {
    if (currentTime - servo2LastMoveTime >= SERVO2_STEP_INTERVAL) {
      if (servo2Angle < servo2Target) {
        servo2Angle += SERVO_STEP_DEGREES;
        if (servo2Angle > servo2Target) servo2Angle = servo2Target;
      } else if (servo2Angle > servo2Target) {
        servo2Angle -= SERVO_STEP_DEGREES;
        if (servo2Angle < servo2Target) servo2Angle = servo2Target;
      }
      servoPresence.write(servo2Angle);

      if (servo2Angle == servo2Target) {
        if (servo2Target == SERVO2_CENTER + SERVO2_SWING) {
          servo2Target = SERVO2_CENTER - SERVO2_SWING;
        } else {
          servo2Target = SERVO2_CENTER + SERVO2_SWING;
        }
      }
      servo2LastMoveTime = currentTime;
    }
  }
}

// ============================================================
// Gesture layer: HC-SR04 -> passive buzzer theremin
// ============================================================
void runGestureLayer(unsigned long currentTime) {
  if (currentTime - lastPingTime < HCSR04_MIN_INTERVAL) {
    return;
  }
  lastPingTime = currentTime;

  long distanceCM = readUltrasonicCM();
  gDistanceCM = distanceCM;

  if (distanceCM > 0 && distanceCM >= THEREMIN_MIN_CM && distanceCM <= THEREMIN_MAX_CM) {
    lastActivityTime = currentTime;
    int freq = map(distanceCM, THEREMIN_MIN_CM, THEREMIN_MAX_CM,
                   THEREMIN_MAX_FREQ, THEREMIN_MIN_FREQ); // closer = higher pitch
    tone(PIN_BUZZER, freq);
    thereminActive = true;
  } else {
    if (thereminActive) {
      noTone(PIN_BUZZER);
      thereminActive = false;
    }
  }
}

// Returns distance in cm, or -1 if no echo (out of range / timeout)
long readUltrasonicCM() {
  digitalWrite(PIN_HCSR04_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_HCSR04_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_HCSR04_TRIG, LOW);

  // 30ms timeout (~5m round trip) prevents pulseIn() from blocking indefinitely
  unsigned long duration = pulseIn(PIN_HCSR04_ECHO, HIGH, 30000UL);
  if (duration == 0) {
    return -1;
  }
  return duration / 58; // microseconds to centimeters
}

// ============================================================
// Idle timeout: return everything to rest state
// ============================================================
void checkIdleTimeout(unsigned long currentTime) {
  if (currentTime - lastActivityTime >= IDLE_TIMEOUT_MS) {
    digitalWrite(PIN_L298N_IN1, LOW);
    digitalWrite(PIN_L298N_IN2, LOW);
    motorRunning = false;

    noTone(PIN_BUZZER);
    thereminActive = false;

    servo1Angle = SERVO1_CENTER;
    servo2Angle = SERVO2_CENTER;
    servoSound.write(SERVO1_CENTER);
    servoPresence.write(SERVO2_CENTER);
    servo1Target = SERVO1_CENTER + SERVO1_SWING;
    servo2Target = SERVO2_CENTER + SERVO2_SWING;

    FastLED.clear();
    FastLED.show();
    lastVULevel = -1;

    personDetected = false;
    systemState = STATE_IDLE;
  }
}

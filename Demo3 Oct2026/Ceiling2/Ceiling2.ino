/*
  Sound-Triggered LEDs + DFPlayer Mini, 3 Tracks (Arduino Nano) - v2
  -------------------------------------------------------------------
  1. Power-up / idle: first 5 LEDs flash white and green.
  2. MAX9814 hears a LOUD, SUSTAINED sound (a loud voice): DFPlayer
     plays track 1, then track 2 (LEDs dark during these two tracks).
  3. Track 3 plays while the full 100-LED strip flashes a blue and
     white pattern, with only 30 LEDs lit at any one moment.
  4. When track 3 ends, returns to idle and listens again.

  CHANGES IN v2
    - Higher SOUND_THRESHOLD (250)
    - Sound must stay above the threshold for REQUIRED_HITS
      consecutive sample windows (3 x 50 ms = ~150 ms) before it
      triggers, so short soft blips and background noise are ignored.

  WIRING
    WS2812 strip DIN  -> Nano D6 (through a ~330 ohm resistor)
    WS2812 5V / GND   -> EXTERNAL 5V supply (not the Nano's 5V pin)
    External supply GND must also connect to Nano GND
    (add a ~1000uF capacitor across the strip's 5V/GND)

    MAX9814 VDD       -> Nano 3V3 (5V also works)
    MAX9814 GND       -> Nano GND
    MAX9814 OUT       -> Nano A0
    MAX9814 GAIN      -> VDD (lowest gain, 40dB)

    DFPlayer VCC      -> Nano 5V (or separate 5V supply)
    DFPlayer GND      -> Nano GND
    DFPlayer TX       -> Nano D10 (Nano receives here)
    DFPlayer RX       -> Nano D11 via ~1k resistor (REQUIRED: prevents crackle)
    DFPlayer BUSY     -> Nano D2  (goes LOW while a track is playing)
    DFPlayer SPK_1/2  -> small speaker

  SD CARD: FAT32, files 0001.mp3, 0002.mp3, 0003.mp3 in the root folder.

  LIBRARIES (Library Manager): FastLED, DFRobotDFPlayerMini
*/

#include <FastLED.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ---------- Pins ----------
#define LED_PIN      6
#define MIC_PIN      A0
#define BUSY_PIN     2
#define DF_RX_PIN    10   // Nano receives <- DFPlayer TX
#define DF_TX_PIN    11   // Nano transmits -> DFPlayer RX

// ---------- LED settings ----------
#define NUM_LEDS         100
#define IDLE_LEDS        5      // first 5 LEDs used for the idle flash
#define ACTIVE_LEDS      30     // LEDs lit at once during the blue/white show
#define LED_BRIGHTNESS   100    // 0-255, keeps current draw reasonable
#define IDLE_FLASH_MS    400    // idle flash speed
#define SHOW_FRAME_MS    120    // blue/white frame speed

// ---------- Sound settings (TUNE THESE) ----------
#define SAMPLE_WINDOW_MS 50     // peak-to-peak sampling window
#define SOUND_THRESHOLD  250    // max useful value is ~400; raise to be less sensitive
#define REQUIRED_HITS    3      // consecutive loud windows needed (3 x 50 ms)
#define PRINT_LEVELS     true   // prints sound level to Serial Monitor for tuning
#define COOLDOWN_MS      2000   // ignore sound after the show (speaker echo)

// ---------- Audio settings ----------
#define DF_VOLUME             20      // 0-30
#define BUSY_START_TIMEOUT_MS 2000    // how long to wait for a track to start
#define TRACK_TIMEOUT_MS      120000  // safety: give up on a track after this long

CRGB leds[NUM_LEDS];
uint8_t order[NUM_LEDS];        // used to pick 30 random distinct LEDs

SoftwareSerial dfSerial(DF_RX_PIN, DF_TX_PIN);
DFRobotDFPlayerMini myDFPlayer;

bool idleState = false;
bool blueFirst = true;
unsigned long lastIdleToggle = 0;

// ---------------------------------------------------------------
int readSoundLevel() {
  unsigned long start = millis();
  int minVal = 1023;
  int maxVal = 0;
  while (millis() - start < SAMPLE_WINDOW_MS) {
    int s = analogRead(MIC_PIN);
    if (s < minVal) minVal = s;
    if (s > maxVal) maxVal = s;
  }
  return maxVal - minVal;
}

// ---------------------------------------------------------------
void showIdleFlash() {
  if (millis() - lastIdleToggle < IDLE_FLASH_MS) return;
  lastIdleToggle = millis();
  idleState = !idleState;

  FastLED.clear();
  for (int i = 0; i < IDLE_LEDS; i++) {
    bool even = (i % 2 == 0);
    leds[i] = (even == idleState) ? CRGB::White : CRGB::Green;
  }
  FastLED.show();
}

// ---------------------------------------------------------------
// Draws one frame: 30 random distinct LEDs, alternating blue and white
void drawBlueWhiteFrame() {
  // Partial shuffle: first ACTIVE_LEDS entries become random distinct LEDs
  for (int i = 0; i < ACTIVE_LEDS; i++) {
    int j = random(i, NUM_LEDS);
    uint8_t tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }

  FastLED.clear();
  for (int i = 0; i < ACTIVE_LEDS; i++) {
    bool blue = ((i % 2 == 0) == blueFirst);
    leds[order[i]] = blue ? CRGB::Blue : CRGB::White;
  }
  FastLED.show();

  blueFirst = !blueFirst;
}

// ---------------------------------------------------------------
// Plays a track and waits until it finishes (BUSY pin goes HIGH again).
// If withShow is true, runs the blue/white LED show during playback.
void playTrack(int track, bool withShow) {
  Serial.print(F("Playing track "));
  Serial.println(track);
  myDFPlayer.play(track);

  // Wait for playback to actually start (BUSY goes LOW)
  unsigned long start = millis();
  while (digitalRead(BUSY_PIN) == HIGH) {
    if (millis() - start > BUSY_START_TIMEOUT_MS) {
      Serial.println(F("Track did not start - check SD card and file names."));
      return;
    }
  }

  // Play until BUSY returns HIGH (track finished)
  unsigned long lastFrame = 0;
  while (digitalRead(BUSY_PIN) == LOW && millis() - start < TRACK_TIMEOUT_MS) {
    if (withShow && millis() - lastFrame >= SHOW_FRAME_MS) {
      lastFrame = millis();
      drawBlueWhiteFrame();
    }
  }
}

// ---------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  dfSerial.begin(9600);
  pinMode(BUSY_PIN, INPUT_PULLUP);

  // Start DFPlayer first, before FastLED begins disabling interrupts
  Serial.println(F("Initializing DFPlayer Mini..."));
  if (!myDFPlayer.begin(dfSerial)) {
    Serial.println(F("DFPlayer Mini not detected! Check wiring and SD card."));
    while (true) { }
  }
  myDFPlayer.volume(DF_VOLUME);
  Serial.println(F("DFPlayer ready."));

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);  // safety cap, adjust to your supply
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < NUM_LEDS; i++) order[i] = i;
  randomSeed(analogRead(A1));  // floating pin = random seed
}

// ---------------------------------------------------------------
void loop() {
  static int hits = 0;   // consecutive loud sample windows

  showIdleFlash();

  int level = readSoundLevel();
  if (PRINT_LEVELS) {
    Serial.print(F("Sound level: "));
    Serial.print(level);
    Serial.print(F("  hits: "));
    Serial.println(hits);
  }

  if (level >= SOUND_THRESHOLD) hits++;
  else hits = 0;

  if (hits >= REQUIRED_HITS) {
    hits = 0;
    Serial.println(F("Sound detected!"));

    FastLED.clear();
    FastLED.show();

    playTrack(1, false);   // LEDs dark
    playTrack(2, false);   // LEDs dark
    playTrack(3, true);    // blue/white show runs for the whole track

    FastLED.clear();
    FastLED.show();

    delay(COOLDOWN_MS);
    lastIdleToggle = millis();
  }
}

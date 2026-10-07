/*
  DFPlayer Mini - First Test (Arduino Nano)
  -------------------------------------------
  Plays tracks 1, 2, and 3 from the SD card, one after another,
  with a pause between each. Use this to confirm wiring, SD card
  formatting, and basic playback before integrating into a larger
  project.

  SD card requirements:
    - microSD, 32GB or less, FAT16/FAT32 (not exFAT)
    - Files named 0001.mp3, 0002.mp3, 0003.mp3 in the root directory

  Wiring:
    Nano 5V   -> DFPlayer VCC
    Nano GND  -> DFPlayer GND
    Nano D10  -> DFPlayer TX
    Nano D11  -> DFPlayer RX   (via ~1k ohm resistor recommended)
    DFPlayer SPK_1/SPK_2 -> small speaker

  Library required: DFRobotDFPlayerMini
    Install via Library Manager: "DFRobotDFPlayerMini" by DFRobot
*/

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// RX, TX on the Nano side (connects to DFPlayer's TX, RX respectively)
SoftwareSerial dfSerial(10, 11); // Nano RX=10 (<-DFPlayer TX), Nano TX=11 (->DFPlayer RX)

DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(9600);     // USB serial for debug messages
  dfSerial.begin(9600);   // DFPlayer Mini communicates at 9600 baud

  Serial.println(F("Initializing DFPlayer Mini..."));

  if (!myDFPlayer.begin(dfSerial)) {
    Serial.println(F("DFPlayer Mini not detected!"));
    Serial.println(F("Check wiring, SD card insertion, and power."));
    while (true) {
      // Halt here - fix wiring/card and reset
    }
  }

  Serial.println(F("DFPlayer Mini ready."));

  myDFPlayer.volume(20);  // Volume range: 0 (mute) to 30 (max)
  delay(500);
}

void loop() {
  Serial.println(F("Playing track 1 (0001.mp3)..."));
  myDFPlayer.play(1);
  delay(5000); // Adjust to roughly match your track length

  Serial.println(F("Playing track 2 (0002.mp3)..."));
  myDFPlayer.play(2);
  delay(4000);

myDFPlayer.volume(28);  // Volume range: 0 (mute) to 30 (max)
  delay(500);

  Serial.println(F("Playing track 3 (0003.mp3)..."));
  myDFPlayer.play(3);
  delay(60000);

  Serial.println(F("Cycle complete. Pausing before repeat..."));
  delay(3000);
}

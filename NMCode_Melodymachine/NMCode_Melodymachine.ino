
/*
   NMCode by this.is.NOISE inc. 

   https://github.com/thisisnoiseinc/NMCode

   Built upon:
    
    "BLE_MIDI Example by neilbags 
    https://github.com/neilbags/arduino-esp32-BLE-MIDI
    
    Based on BLE_notify example by Evandro Copercini."
*/



#include <BLEDevice.h>

#include <BLEUtils.h>

#include <BLEServer.h>

#include <BLE2902.h>

#include "esp_bt_main.h"

#include "esp_bt_device.h"

#define SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"

#define CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"


int potPin = 36; // Slider
int rotPin = 39; // Rotary Knob
bool rotMoving = true;
int midiCState = 0; // General current state
int led_Blue = 14; // BLE LED
int led_Green = 4; // CHANNEL LED
const int button = 12;
int potCstate = 0; // Slider current state
int rotCState = 0; // Rotary Knob current state
int outputValue = 0;
int ButtonNote = 0;
int Channel_SelectON = 0;
int Channel_SelectOFF = 0;
int Channel_SelectCC = 0;
int Buttonselect[button] = {  // Buttons put in order of reference board.
 16,
 17,
 18,
 21,
 19,
 25,
 22,
 23,
 27,
 26,
 35,
 34
  };
int buttonCstate[button] = {0}; // Button current state
int buttonPState[button] = {0}; // Button previous state
int OffNote[button] = {0};
int debounceDelay = 5;
int lastDebounceTime[button] = {0};
int i = 0;
const int numReadings = 15;
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average1 = 0; // average current state
int lastaverage1 = 0; // average previous state

// global Variables by Msz
int noteOffset=0;
int prevSelectState=0; // did we already select a scale?
int scale=0; // the chosen scale
int scaleOffset[10][12]={ // define all scales for scale mode
	{0,0,0,0,0,0,0,0,0,0,0,0}, // 0. Chromatic (Default)
	{0,1,2,2,3,4,5,5,6,7,7,8}, // 1. Ionian (Major)
	{0,1,1,2,3,4,4,5,6,6,7,8}, // 2. Dorian
	{0,0,1,2,3,3,4,5,5,6,7,8}, // 3. Phrygian
	{0,1,2,3,3,4,5,5,6,7,8,8}, // 4. Lydian
	{0,1,2,2,3,4,4,5,6,7,7,8}, // 5. Mixolydian
	{0,1,1,2,3,3,4,5,6,6,7,8}, // 6. Aeolian (Minor)
	{0,0,1,2,2,3,4,4,5,6,7,8}, // 7. Locrian
  {0,0,1,1,2,3,4,5,5,6,6,7}, // 8. Altered (Super Locrian)
  //{0,1,2,3,4,4,5,5,6,7,8,9}, // Augemented (Lydian augmented)
	{0,1,1,2,2,2,2,3,3,3,4,4} // 9. Minor with tritone and Maj7
};
bool chordModeMajor=false;
bool chordModeMinor=false;
int *chordNotes[] = { // define all chords for chord mode
    (int[]){ 0 }, // root note
    (int[]){ 0,5,7 }, // major chord
    (int[]){ 0,4,7 }, // minor chord
    (int[]){ 0,4,6 }, // dim chord
    (int[]){ 0 }, // sus 2
    (int[]){ 0 }, // sus 4
    (int[]){ 0 }, // min 9
    (int[]){ 0 }, // 1st inversion
    (int[]){ 0 } // 2nd inversion
};
int numberOfChordNotes[] = { //number of notes per chord. Determining Array lenght is a pain in C, so easier to just tell how long the array is.
1, // root note
3, // major chord
3, // minor chord
3, // dim chord
4, // sus 2
4, // sus 4
4, // min 9
3, // 1st inversion
3  // 2nd inversion
};
int majorScale[]={1,2,2,1,1,2,3}; // all seven chords of major scale
int minorScale[]={2,3,1,2,2,1,1}; // same for minor
int OffNotes[]={0,0,0,0,0}; // C can't do dynamic arrays, thus this has a fixed length of 5, making this the max for chord poliphony
int numberOfOffNotes=0; // How many notes have been triggered in chord mode? 

BLECharacteristic *pCharacteristic;

bool deviceConnected = false;

uint8_t midiPacket[] = {

   0x80,  // header

   0x80,  // timestamp, not implemented 

   0x00,  // status

   0x3c,  // 0x3c == 60 == middle c

   0x00   // velocity

};

class MyServerCallbacks: public BLEServerCallbacks {

    void onConnect(BLEServer* pServer) {

      deviceConnected = true;

    };



    void onDisconnect(BLEServer* pServer) {

      deviceConnected = false;

    }

};

bool initBluetooth()
{
  if (!btStart()) {
    Serial.println("Failed to initialize controller");
    return false;
  }
 
  if (esp_bluedroid_init() != ESP_OK) {
    Serial.println("Failed to initialize bluedroid");
    return false;
  }
 
  if (esp_bluedroid_enable() != ESP_OK) {
    Serial.println("Failed to enable bluedroid");
    return false;
  }
 
}

void setup() {

  Serial.begin(115200);

  initBluetooth();
  const uint8_t* point = esp_bt_dev_get_address();
 
  char str[6];
 
  sprintf(str, "NMSVE %02X %02X %02X", (int)point[3], (int)point[4], (int)point[5]);
  Serial.print(str);

  BLEDevice::init(str);

    

  // Create the BLE Server

  BLEServer *pServer = BLEDevice::createServer();

  pServer->setCallbacks(new MyServerCallbacks());



  // Create the BLE Service

  BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));



  // Create a BLE Characteristic

  pCharacteristic = pService->createCharacteristic(

    BLEUUID(CHARACTERISTIC_UUID),

    BLECharacteristic::PROPERTY_READ   |

    BLECharacteristic::PROPERTY_WRITE  |

    BLECharacteristic::PROPERTY_NOTIFY |

    BLECharacteristic::PROPERTY_WRITE_NR

  );



  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml

  // Create a BLE Descriptor

  pCharacteristic->addDescriptor(new BLE2902());



  // Start the service

  pService->start();



  // Start advertising

  BLEAdvertising *pAdvertising = pServer->getAdvertising();

  pAdvertising->addServiceUUID(pService->getUUID());

  pAdvertising->start();

  // Initialize buttons + LED's

   for (int i = 0; i < button; i++){
  pinMode (Buttonselect[i], INPUT);
  pinMode (led_Blue, OUTPUT);
  pinMode (led_Green, OUTPUT);
  }

  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }

  while (Channel_SelectON == 0) {
  
  digitalWrite(led_Green, HIGH);
  
    for (int i = 0; i < button; i++) {
      buttonCstate[i] = digitalRead(Buttonselect[i]);
      if (buttonCstate[i] == HIGH) {
        Channel_SelectON = (i + 144);
        Channel_SelectOFF = (i + 128);
        Channel_SelectCC = (i + 176);
      }
    }
      }
      
        digitalWrite(led_Green, LOW);

}

void loop(){

 // Ensure device is connected to BLE

  potCstate = analogRead(potPin);

  if (deviceConnected == false) { 
  digitalWrite(led_Blue, HIGH);
  delay(1000);
  digitalWrite(led_Blue, LOW);
  delay(1000);
}
 

  // Enter Scale Select Mode (by MSz) if slider is all to the left
  else if (potCstate <= 6){
    SELECTMODE();
  }

  // Enter Midi-Control Mode (by MSz) if slider is all to the right
  else if (potCstate >= 4090){
    CONTROLMODE();
  }



 // Enter Default Mode

else {
  prevSelectState=0;
  digitalWrite(led_Blue, HIGH);
  if (chordModeMajor==true || chordModeMinor==true) CHORDMODE();
  else {
    BUTTONS();
    ROTARY();
  }
}
}
 

// Control Button functions for Default Mode

void BUTTONS(){

 for (int i = 0; i < button; i++){
buttonCstate[i] = digitalRead(Buttonselect[i]);
potCstate = analogRead(potPin);
outputValue = map(potCstate, 0, 4095, 3, 9);
ButtonNote = (outputValue * 12 + i + noteOffset + scaleOffset[scale][i]);

if (outputValue == 3 || outputValue == 5 || outputValue == 7 || outputValue == 9) {
  digitalWrite(led_Green, HIGH);
}

else {
  digitalWrite(led_Green, LOW);
}
 
 if ((millis() - lastDebounceTime[i]) > debounceDelay) {
  
  if (buttonPState[i] != buttonCstate[i]) {
        lastDebounceTime[i] = millis();

  if (buttonCstate[i] == HIGH) {  

   midiPacket[2] = Channel_SelectON;
   Serial.println(Channel_SelectON); 

   midiPacket[3] = ButtonNote;
   Serial.println(midiPacket[3]);

   midiPacket[4] = 100;

   pCharacteristic->setValue(midiPacket, 5);

   pCharacteristic->notify();

   OffNote[i] = ButtonNote;
  }

 else {
  midiPacket[2] = Channel_SelectOFF;
   Serial.println(Channel_SelectOFF);

   midiPacket[3] = OffNote[i];

   midiPacket[4] = 0;

   pCharacteristic->setValue(midiPacket, 5);

   pCharacteristic->notify();
 }
buttonPState[i] = buttonCstate[i];
}
}
  }
}

void potaverage1() {
  
  for (int p = 0; p < 15; p++) {
  rotCState = analogRead(rotPin);
  midiCState = map(rotCState, 0, 4095, 0, 127);
  
  // subtract the last reading:
  total = total - readings[readIndex];
  // read from the sensor:
  readings[readIndex] = midiCState;
  // add the reading to the total:
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }

  // calculate the average:
  average1 = total / numReadings;
  delay(1);        // delay in between reads for stability
}
}

// Control Rotary Knob functions for Default + Split Mode

void ROTARY(){

 potaverage1();
 
 if (average1 != lastaverage1) {
    rotMoving = true;
  }

  else {
    rotMoving = false;
  }

  if (rotMoving == true) {
    
   midiPacket[2] = Channel_SelectCC;
   Serial.println(Channel_SelectCC);

   midiPacket[3] = 0x01;
   Serial.println(0x01);

   midiPacket[4] = average1;
   Serial.println(average1);

   pCharacteristic->setValue(midiPacket, 5);

   pCharacteristic->notify();

   lastaverage1 = average1;
 }
  }

// MIDI Control Mode Implemetation by MSz
void CONTROLMODE(){
  
  for (int i = 0; i < button; i++){
  buttonCstate[i] = digitalRead(Buttonselect[i]);
    if (buttonCstate[i] == HIGH) {  
      rotCState = analogRead(rotPin);
      midiCState = map(rotCState, 0, 4095, 0, 127);

      midiPacket[2] = Channel_SelectCC;
      Serial.println(Channel_SelectCC);

      midiPacket[3] = 0x01+i;
      Serial.println(0x0+i);

      midiPacket[4] = midiCState;
      Serial.println(midiCState);

      pCharacteristic->setValue(midiPacket, 5);

      pCharacteristic->notify();
    }
  }
}

  // Scale Select Mode Implemetation by MSz
void SELECTMODE(){
     
  for (int i = 0; i < button; i++){
    buttonCstate[i] = digitalRead(Buttonselect[i]);
    

    // select scale or chord mode (12)
    if (prevSelectState == 0) {
      digitalWrite(led_Blue, LOW);
      if (buttonCstate[i] == HIGH) { 

        if (i==10) chordModeMajor=true;
        else if (i==11) chordModeMinor=true;                     
        else {
          scale=i; // select scale
          chordModeMajor=false;    
          chordModeMinor=false;        
        }
        prevSelectState=1;
        
      }
    }

    // select key (note offset)
    else if (prevSelectState == 1) {
      digitalWrite(led_Blue, HIGH);
      if (buttonCstate[i] == HIGH) { 
        if (noteOffset==i){ // play the root note if it has already been selected. Should help with finding the right one maybe
            // needs to be implemented. Maybe function for sending notes?
        }
        else {
          noteOffset=i; // select root note
        }
      }
    }
  }
}




// Chord Mode by MSz

void CHORDMODE(){

 for (int i = 0; i < button; i++){
buttonCstate[i] = digitalRead(Buttonselect[i]);
potCstate = analogRead(potPin);
outputValue = map(potCstate, 0, 4095, 3, 9);
ButtonNote = (outputValue * 12 + i + noteOffset + scaleOffset[scale][i]);

if (outputValue == 3 || outputValue == 5 || outputValue == 7 || outputValue == 9) {
  digitalWrite(led_Green, HIGH);
}

else {
  digitalWrite(led_Green, LOW);
}
 
 if ((millis() - lastDebounceTime[i]) > debounceDelay) {
  
  if (buttonPState[i] != buttonCstate[i]) {
        lastDebounceTime[i] = millis();

  if (buttonCstate[i] == HIGH) {  
    if (chordModeMajor=true){
       if (i <= 6) TRIGGERNOTES(ButtonNote, chordNotes[majorScale[i]], numberOfChordNotes[majorScale[i]]);
    }
     if (chordModeMinor=true){
       if (i <= 6) TRIGGERNOTES(ButtonNote, chordNotes[minorScale[i]], numberOfChordNotes[minorScale[i]]);
    }
   
  }

 else {
  UNTRIGGERNOTES();
 }
buttonPState[i] = buttonCstate[i];
}
}
  }
}

// Function to trigger notes by MSz

void TRIGGERNOTES(int ButtonNote, int notes[], int numberOfNotes){
int note;

  for (int i = 0; i < numberOfNotes; i++){

    note = ButtonNote + notes[i];
    midiPacket[2] = Channel_SelectON;
    Serial.println(Channel_SelectON); 

    midiPacket[3] = note;
    Serial.println(midiPacket[3]);

    midiPacket[4] = 100;

    pCharacteristic->setValue(midiPacket, 5);

    pCharacteristic->notify();

    OffNotes[i] = note;
  } 
numberOfOffNotes = numberOfNotes;
}

void UNTRIGGERNOTES(){

  for (int i = 0; i < numberOfOffNotes; i++){
    midiPacket[2] = Channel_SelectOFF;
    Serial.println(Channel_SelectOFF);

    midiPacket[3] = OffNotes[i];

    midiPacket[4] = 0;

    pCharacteristic->setValue(midiPacket, 5);

    pCharacteristic->notify();
  }
numberOfOffNotes=0;
}

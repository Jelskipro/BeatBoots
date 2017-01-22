#include <SPI.h>
#include "Adafruit_BLE_UART.h"

// Connect CLK/MISO/MOSI to hardware SPI
// e.g. On UNO & compatible: CLK = 13, MISO = 12, MOSI = 11
#define ADAFRUITBLE_REQ 10
#define ADAFRUITBLE_RDY 2     // This should be an interrupt pin, on Uno thats #2 or #3
#define ADAFRUITBLE_RST 9

Adafruit_BLE_UART BTLEserial = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);

const int TRESHHOLD_PRESSED=500;
const int TRESHHOLD_NOT_PRESSED=600;
const int WAIT=100; // wait time in msec

const int STATE_FEET_IN_AIR = 0;
const int STATE_FEET_ONGROUND = 1;

int stateLeft, stateRight;
int iPressureOnGroundLeft = 0, iPressureOnGroundRight = 0;
unsigned long ulDurationOnGroundLeft = 0, ulDurationOnGroundRight = 0;
unsigned long ulTimeBetweenStepsLeft = 0, ulTimeBetweenStepsRight = 0;
unsigned long ulLeftStepTimer, ulRightStepTimer;

// initilise BT variables
aci_evt_opcode_t lastBtComStatus = ACI_EVT_DISCONNECTED;
aci_evt_opcode_t BtComStatus = ACI_EVT_DISCONNECTED;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  BTLEserial.setDeviceName("Jelski"); /* 7 characters max! */
  BTLEserial.begin();

  // pressure sensors
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  stateLeft = STATE_FEET_IN_AIR;
  stateRight = STATE_FEET_IN_AIR;
  ulLeftStepTimer = millis();
  ulRightStepTimer = millis();
}

void setState(int iValue, int *iCurrentState, int *iPressureOnGround, unsigned long *ulDurationOnGround, unsigned long *iTimeBetweenSteps, unsigned long *ulStepTimer ) {
  //Serial.println(iValue);
  if (iValue <= TRESHHOLD_PRESSED) { // Feet on ground
    if (*iCurrentState==STATE_FEET_IN_AIR) { // is Feet was in air: set new cycle.
       *iCurrentState = STATE_FEET_ONGROUND;
       *ulDurationOnGround = 0;
       *iTimeBetweenSteps = millis() - *ulStepTimer; 
       *ulStepTimer = millis();
    } else { // Feet were alreay on ground: adjust duration
       *ulDurationOnGround = millis() - *ulStepTimer;
    } 
  }

  if (iValue >= TRESHHOLD_NOT_PRESSED) { // Feet in the air
    if (*iCurrentState == STATE_FEET_ONGROUND) {
       *iCurrentState = STATE_FEET_IN_AIR;
       *ulDurationOnGround = 0;
    } 
  }

  *iPressureOnGround = map(iValue, 1023, 100, 0, 100);
  *iPressureOnGround = constrain(*iPressureOnGround, 0, 100);
}

void printState(String sFeet, int iCurrentState, int iPressureOnGroundLeft, unsigned long ulDurationOnGround, unsigned long ulTimeBetweenSteps) {
  Serial.print(sFeet);
  Serial.print(", state: ");
  Serial.print(iCurrentState);
  Serial.print(", pressure on ground: ");
  Serial.print(iPressureOnGroundLeft);
  Serial.print(", duration on ground: ");
  Serial.print(ulDurationOnGround);
  Serial.print(", time between steps (ms): ");
  Serial.println(ulTimeBetweenSteps);
}

void sendMessage(String sStatus, int iPressureOnFeet, unsigned long ulDurationOnGround) {
  String sMessage = String(sStatus + ":" + iPressureOnFeet + ":" + ulDurationOnGround );
  //Serial.println(sMessage);

  if (BtComStatus == ACI_EVT_CONNECTED) {
    // Check on data.
    if (BTLEserial.available()) {
      Serial.print("* "); Serial.print(BTLEserial.available()); Serial.println(F(" bytes available from BTLE"));
      
      // OK while we still have something to read, get a character and print it out
      while (BTLEserial.available()) {
        char c = BTLEserial.read();
        Serial.print(c);
      }
      Serial.println();
    }

    // sendMessage...we need to convert the line to bytes, no more than 20 at this time
    uint8_t sendbuffer[20];
    sMessage.getBytes(sendbuffer, 20);
    char sendbuffersize = min(20, sMessage.length());

    Serial.print(F("* Sending -> \"")); Serial.print((char *)sendbuffer); Serial.println("\"");

    // write the data
    BTLEserial.write(sendbuffer, sendbuffersize);
  } else {
    Serial.print(F("* not connected -> message \"")); Serial.print(sMessage);  Serial.println("\" not send.");
  }
}


void printBTState() {
  if (BtComStatus == ACI_EVT_DEVICE_STARTED) {
      Serial.println(F("* Advertising started"));
  }
  if (BtComStatus == ACI_EVT_CONNECTED) {
      Serial.println(F("* Connected!"));
  }
  if (BtComStatus == ACI_EVT_DISCONNECTED) {
      Serial.println(F("* Disconnected or advertising timed out"));
  }
}

void loop() {
  // Tell the nRF8001 to do whatever it should be working on.
  BTLEserial.pollACI();

  // Get current status of BT connection
  BtComStatus = BTLEserial.getState();
  if (BtComStatus != lastBtComStatus) {
    //printBTState();
    lastBtComStatus = BtComStatus;

  }

  // get state of left foot
  setState(analogRead(A0), &stateLeft, &iPressureOnGroundLeft, &ulDurationOnGroundLeft, &ulTimeBetweenStepsLeft, &ulLeftStepTimer);
  //printState("Left", stateLeft, iPressureOnGroundLeft, ulDurationOnGroundLeft, ulTimeBetweenStepsLeft);
 
  // get state of right foot
  setState(analogRead(A1), &stateRight, &iPressureOnGroundRight, &ulDurationOnGroundRight, &ulTimeBetweenStepsRight, &ulRightStepTimer);
  //printState("Right", stateRight, iPressureOnGroundRight, ulDurationOnGroundRight, ulTimeBetweenStepsRight);

  // determine situation and send info to the mobile phone
  if( (stateLeft==STATE_FEET_ONGROUND) && (stateRight==STATE_FEET_ONGROUND)) {
     // Stand still, both feet on ground
     sendMessage("S", (iPressureOnGroundLeft + iPressureOnGroundRight), ulDurationOnGroundLeft);
  } else if ((stateLeft==STATE_FEET_ONGROUND) && (ulTimeBetweenStepsLeft < 1000 )) {
     // Running, Left feet on ground
     sendMessage("RL", iPressureOnGroundLeft, ulDurationOnGroundLeft);
  } else if ((stateLeft==STATE_FEET_ONGROUND) && (ulTimeBetweenStepsLeft > 1000 )) {
     // Walking, Left  feet on ground
     sendMessage("WL", iPressureOnGroundLeft, ulDurationOnGroundLeft);
  } else if ((stateRight==STATE_FEET_ONGROUND) && (ulTimeBetweenStepsRight < 1000 )) {
     sendMessage("RR", iPressureOnGroundRight, ulDurationOnGroundRight);
  } else if ((stateRight==STATE_FEET_ONGROUND) && (ulTimeBetweenStepsRight > 1000 )) {
     // Walking, Right feet on ground
     sendMessage("WR", iPressureOnGroundRight, ulDurationOnGroundRight);
  } else {     
     // Both feet in air
     //sendMessage("IDLE", 0, 0);
  }
    
  delay(WAIT);
}


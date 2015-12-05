// Taken from http://arduino.ru/forum/apparatnye-voprosy/biblioteka-chteniya-id-brelkov-signalizatsiii-hcs301-keeloq?page=1

#include "math.h"
#include <Button.h>

#define DEBUG // Set to DEBUG to eanble debug mode

#define PIN_RECEIVE 7
#define PIN_TRANSMIT 2
#define PIN_RELAY 5
#define PIN_OPEN_CLOSE_DOOR 3 // From the alarm module, exepcts LOW


#define RELAY_SIGNAL_TIME 3000UL // number of miliseconds to set LOW to the PIN_RELAY
#define SLEEP_BEFORE_RETRANSMIT 20 // number of second before we retransmit the signal
#define TRANSMIT_SIGNAL_COUNT 1 // how many times we will retransmit the received KeeLoq signal 
#define TRANSMIT_SIGNAL_DELAY 500 // Number of miliseconds between the retransmits

// Vars for receive/transmit signal
int lastRxValue = 0;
int tempRxValue = 0;
unsigned long lastRxTime = 0;
unsigned long tempTime = 0;
unsigned long difTime = 0;
boolean bValidPacket = false;
int decodeMethod = 1; //0 real - as in manual, 1 inver as Oleg do

// Vars for keelog
int keelog_state = 0;
int keelogCounter = 0;
byte keelog_code[9];

// Other vars
Button openCloseDoorButton = Button(PIN_OPEN_CLOSE_DOOR, PULLUP);
boolean openCloseDoorSignalReceivedTriggered = false;
unsigned long relayOnStartTime;
boolean relayStateOn = false;

//============================================================


void keelogPrintCodeSingleLine(String message) {
  // Helper method to send the KeeLoq code in a single line
  Serial.print(message);

  for (int i = 0; i < 9; i++) {
    Serial.print(keelog_code[i], HEX);
  }
  Serial.println("");
}

void keelogPrintCode() {
  // Helper method to print the KeeLoq code with details to the Serial
  Serial.println("KeeLog Code Received");
  Serial.print("\tType: ");
  if (decodeMethod == 0) {
    Serial.println("Origininal");
  }
  else {
    Serial.println("Inverted");
  }
  keelogPrintCodeSingleLine("\tReceived Code: ");

  Serial.print("\t\tHope: ");
  Serial.print(keelog_code[0], HEX);
  Serial.print(keelog_code[1], HEX);
  Serial.print(keelog_code[2], HEX);
  Serial.println(keelog_code[3], HEX);
  Serial.print("\t\tFix: ");
  Serial.print(keelog_code[4], HEX);
  Serial.print(keelog_code[5], HEX);
  Serial.println(keelog_code[6], HEX);
  Serial.print("\t\tButton: ");
  Serial.println(keelog_code[7], HEX);
  Serial.print("\t\tDop: ");
  Serial.println(keelog_code[8], HEX);
}

void waitBeforeRetransmit() {
  // sleep before retransmit the KeeLoq code
  for (int i = SLEEP_BEFORE_RETRANSMIT; i >= 0; i--) {
    delay(1000);
#ifdef DEBUG
    Serial.println(i);
#endif
  }
}

void cleanKeeloqCode() {
  keelog_state = 0;
  for (int i = 0; i < 9; i++) {
    keelog_code[i] = 0;
  }
}

void transmitSignal() {
#ifdef DEBUG
  // Print the code
  keelogPrintCode();
#endif
  // Turn the relay OFF as we have valid signal
  relayTurnOFF();

  // If needed, sleep before retransmit
  waitBeforeRetransmit();

  for (int i = TRANSMIT_SIGNAL_COUNT; i > 0; i--) {
    // Transmit the KeeLoq signal
    keelog_send(keelog_code);

    // Delay between retransmits
    delay(TRANSMIT_SIGNAL_DELAY);
  }
  // Clean the code as the signal was retransmited
  cleanKeeloqCode();
}

void send_meander(int time) {
  digitalWrite(PIN_TRANSMIT, HIGH);
  delayMicroseconds(time);
  digitalWrite(PIN_TRANSMIT, LOW);
  delayMicroseconds(time);
}

void keelog_send(byte * keelog_code) {
#ifdef DEBUG
  keelogPrintCodeSingleLine("Transmitting KeeLog Code: ");
#endif
  for (int i = 0; i < 11; i++) { //посылаем преамблу
    send_meander(400);
  }
  digitalWrite(PIN_TRANSMIT, HIGH);
  delayMicroseconds(400);
  digitalWrite(PIN_TRANSMIT, LOW);
  delayMicroseconds(4000);//посылаем хедер

  for ( int i = 0; i < 9; i++) {
    if (decodeMethod == 1) {
      for (int i2 = 7; i2 >= 0; i2--) {
        if (bitRead(keelog_code[i], i2)) {
          digitalWrite(PIN_TRANSMIT, HIGH);
          delayMicroseconds(400);
          digitalWrite(PIN_TRANSMIT, LOW);
          delayMicroseconds(2 * 400);
        }
        else {
          digitalWrite(PIN_TRANSMIT, HIGH);
          delayMicroseconds(2 * 400);
          digitalWrite(PIN_TRANSMIT, LOW);
          delayMicroseconds(400);
        }
      }
    }
    else {
      for (int i2 = 0; i2 < 8; i2++) {
        if (!bitRead(keelog_code[i], i2)) {
          digitalWrite(PIN_TRANSMIT, HIGH);
          delayMicroseconds(400);
          digitalWrite(PIN_TRANSMIT, LOW);
          delayMicroseconds(2 * 400);
        }
        else {
          digitalWrite(PIN_TRANSMIT, HIGH);
          delayMicroseconds(2 * 400);
          digitalWrite(PIN_TRANSMIT, LOW);
          delayMicroseconds(400);
        }
      }
    }
  }
}
void keelog_get() {
  bValidPacket = false;
  if (keelog_state == 0) { //ждем преамбулу и хедер
    if (difTime > 280 && difTime < 620 && lastRxValue != tempRxValue) {
      keelogCounter ++;
    }
    else {
      if (keelogCounter == 23) {
        if (difTime > 2800 && difTime < 6200 && lastRxValue == 0) {
          keelog_state = 1;
        }
      }
      keelogCounter = 0;
    }
  }
  else if (keelog_state == 1) { // получаем биты
    if (difTime > 560 && difTime < 1240 && lastRxValue == 1) { // получили 1
      if (decodeMethod == 0) {
        keelog_code[round(keelogCounter / 8)] = (keelog_code[round(keelogCounter / 8)] >> 1) | B10000000;
      }
      else {
        keelog_code[round(keelogCounter / 8)] = (keelog_code[round(keelogCounter / 8)] << 1) | B00000000;
      }
      bValidPacket = true;
    }
    else if (difTime > 280 && difTime < 620 && lastRxValue == 1) {
      if (decodeMethod == 0) {
        keelog_code[round(keelogCounter / 8)] = (keelog_code[round(keelogCounter / 8)] >> 1) | B00000000;
      }
      else {
        keelog_code[round(keelogCounter / 8)] = (keelog_code[round(keelogCounter / 8)] << 1) | B00000001;
      }
      bValidPacket = true;
    }
    else if (lastRxValue == 0) {
    }
    else {
      keelog_state = 1;
      keelogCounter = 0;
    }

    if (bValidPacket) {
      keelogCounter++;
      if (keelogCounter == 66) {
        // It seems that we have valid signal, let's retrasmit it
        transmitSignal();

        keelogCounter = 0;
        keelog_state = 0;
      }
    }
  }
}
//keelog end


void relayTurnON() {
  // Turn the relay ON
  digitalWrite(PIN_RELAY, HIGH);
  relayStateOn = true;
  relayOnStartTime = millis();
#ifdef DEBUG
  Serial.println("Relay switched ON");
#endif

}

void relayTurnOFF() {
  // Turn the relay OFF
  digitalWrite(PIN_RELAY, LOW);
  relayStateOn = false;
  relayOnStartTime = 0;
  openCloseDoorSignalReceivedTriggered = false;
#ifdef DEBUG
  Serial.println("Relay switched OFF");
#endif
}

void receiveKeelogSignal() {
  tempRxValue = digitalRead(PIN_RECEIVE);

  if (tempRxValue != lastRxValue) {

    tempTime = micros();
    difTime = tempTime - lastRxTime;
    keelog_get();
    lastRxTime = tempTime;
    lastRxValue = tempRxValue;
  }
}

void setup() {
  pinMode(PIN_TRANSMIT, OUTPUT);
  pinMode(PIN_RECEIVE, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("start");
#endif
  lastRxValue = digitalRead(PIN_RECEIVE);
  lastRxTime = micros();
}

#ifdef DEBUG
int serialIncomingByte = 0;
#endif
void loop() {
#ifdef DEBUG
  // send data only when you receive data:
  if (Serial.available() > 0) {
    // read the incoming byte:
    serialIncomingByte = Serial.read();

    // say what you got:
    Serial.print("I received: ");
    Serial.println(serialIncomingByte, DEC);
    openCloseDoorSignalReceivedTriggered = true;
  }
#endif


  if ((openCloseDoorSignalReceivedTriggered == false) && (openCloseDoorButton.isPressed())) {
    openCloseDoorSignalReceivedTriggered = true;
#ifdef DEBUG
    Serial.println("Signal received from the alarm module.");
#endif
  }

  if (openCloseDoorSignalReceivedTriggered == true) {
    if (relayStateOn == true) {
      // check if it is time to turn off the relay
      if ((millis() - relayOnStartTime) >= RELAY_SIGNAL_TIME) {
        // No more time left, turn the relay OFF
        relayTurnOFF();
      } else {
        // The relay is ON and we can start receiving keelog signal from the remote control
        receiveKeelogSignal();
      }
    } else {
      // The current relay state is OFF, so turn it ON
      relayTurnON();
    }
  }
}

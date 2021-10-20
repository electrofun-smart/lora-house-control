/*
  LoRa House Control of Lora Water Pump
  LED indicates the state of relay/water pump at other end (Lora Water Pump)
  LED blinking indicates that relay/water pump at other end (Lora Water Pump) is on and controlled by a Timer of 45 minutes
  Button toggles the state of relay/water pump
  Button long press, turn on the relay/water pump with a timer.
*/

#include <SPI.h>              // include libraries
#include <LoRa.h>
#include <ezButton.h>         // handle the button press eliminating bouncing

const int csPin = 15;         // LoRa radio chip select
const int resetPin = 16;      // LoRa radio reset
const int irqPin = 4;         // change for your board; must be a hardware interrupt pin
const int LED = 2;

ezButton button1(5);

const int DEBOUNCE_DELAY = 70;
const int LONG_PRESS_TIME  = 2000; // 2000 milliseconds
const int ASK_TIME  = 30000; // 30 seconds

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0xDA;     // address of this device
byte destination = 0xDF;      // destinatioln to send to
long lastSendTime = 0;        // last send time
long lastBlinkTime = 0;
int intervalask = ASK_TIME;     // interval between sends
byte counterTimer = 0;
boolean timerRunning = false;
boolean blinkLed = false;
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;

void setup() {
  Serial.begin(9600);                   // initialize serial

  while (!Serial);

  //pinMode(button, INPUT);
  pinMode(LED, OUTPUT);
  button1.setDebounceTime(DEBOUNCE_DELAY); 
  
  Serial.println("LoRa Duplex with callback");

  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(csPin, resetPin, irqPin);// set CS, reset, IRQ pin

  if (!LoRa.begin(915E6)) {             // initialize ratio at 915 MHz
    Serial.println("LoRa init failed. Check your connections.");
    delay(100);
    while (true);                       // if failed, do nothing
  }

  LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.println("LoRa init succeeded.");
  lastSendTime = millis();

}

void loop() {
  
  button1.loop();
  
   if(button1.isPressed()){
    pressedTime = millis();             // save pressed time for release time calculation
  }

  if ((millis() - lastSendTime) > intervalask) {
    sendMessage("asking");
    lastSendTime = millis();           // timestamp the message
    Serial.println("Sending asking");
    LoRa.receive();                    // go back into receive mode
  }
  
  if(button1.isReleased()){
    Serial.println("The button is released");
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;

    if( pressDuration > LONG_PRESS_TIME ){
      Serial.println("A long press is detected, turn on Relay and time it");
      timerRunning = true; //wait for relay on feedback to blink LED
      sendMessage("timer");              // send lora message to turn on relay and time it
      Serial.println("Sending timer");
    } else { // short press actions
      sendMessage("toggle");              // send lora message to toggle relay
      Serial.println("Sending toggle");
      timerRunning = false;               // stop timer when short pressed
    }
    LoRa.receive();                     // go back into receive mode
  }
  checkTimer();
}

void sendMessage(String outgoing) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outgoing.length());        // add payload length
  LoRa.print(outgoing);                 // add payload
  LoRa.endPacket(true);                     // finish packet and send it
  msgCount++;                           // increment message ID
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  String incoming = "";                 // payload of packet

  while (LoRa.available()) {            // can't use readString() in callback, so
    incoming += (char)LoRa.read();      // add bytes one by one
  }

  if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             // skip rest of function
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress) {
    Serial.println("This message is not for me.");
    return;                             // skip rest of function
  }

  if (incoming == "relay on"){
    digitalWrite(LED, HIGH);
    if (timerRunning)
       blinkLed = true;
  }
  if (incoming == "relay off"){
    digitalWrite(LED, LOW);
    timerRunning = false;
    blinkLed = false;
  }
  if (incoming == "timer"){
    digitalWrite(LED, HIGH);
    timerRunning = true;
    blinkLed = true;
  }
  // if message is for this device, or broadcast, print details:
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
}
/*
Timer is controlled by lora-water-pump
Here we just need to blink the LED while timerRunning is true and blinkLed is true
timerRunning is set to false if message "relay off" is received 
from lora-water-pump

*/
void checkTimer(){
  if (timerRunning && blinkLed){
    if ((millis() - lastBlinkTime) > 250) {
      lastBlinkTime = millis();
      digitalWrite(LED, !digitalRead(LED));   // Toggle the led
    }
  }
}

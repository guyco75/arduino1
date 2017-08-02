
#include "IRelectra.h"
#include <dht.h>

IRsend irs;

//#define REPLY_DEBUG

String rxCmdStr;
#define IR_LED_PIN (13)

void setup() {
  Serial.begin(57600);
  Serial.println("$ready#");

  pinMode(IR_LED_PIN, OUTPUT);
  rxCmdStr.reserve(128);
}

void loop() {
  static boolean inSync = false;

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '$') {
      rxCmdStr = "";
      inSync = true;
      continue;
    }
    
    if (inSync) {
      if (c == '#') {
        handleCommand();
        inSync = false;
      } else {
        if (rxCmdStr.length() < 128) {
          rxCmdStr += c;
        } else {
          inSync = false;
        }
      }
    }
  }
}

unsigned int a, b;

static inline String getNextToken() {
  String ret;

  b = rxCmdStr.indexOf(',', a);
  if (b < 1)
    return "";

  ret = rxCmdStr.substring(a,b);
  a = b+1;
  return ret;
}

static inline boolean verifyEnding() {
  return (a == rxCmdStr.length());
}

void handleCommand() {
  int cmd;
  a = b = 0;

  cmd = getNextToken().toInt();
  switch (cmd) {
    case 1: getTemperature(); break;
    case 2: sendToAC(); break;
    case 3: setIRLed(); break;
    case 4: tvCommand(); break;
    case 5: pioneerReceiver(); break;
  }
}

#define DHT_PIN 5
dht DHT;

void getTemperature() {
  int chk;
  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}

#ifdef REPLY_DEBUG
  chk = 0;
  DHT.temperature = random(10, 37);
  DHT.humidity = random(40, 60);
#else
  chk = DHT.read22(DHT_PIN);
#endif

  Serial.print("${\"status\":\"");
  Serial.print(chk);
  Serial.print("\",\"temperature\":\"");
  Serial.print(DHT.temperature, 1);
  Serial.print("\",\"humidity\":\"");
  Serial.print(DHT.humidity, 1);
  Serial.print("\"}#");
  Serial.println();
}

IRsend irsend;
IRelectra e(&irsend);
void sendToAC() {
  int power, mode, fan, temp, iFeel, tempNotif;

  power = getNextToken().toInt();
  if (power < 0 || 1 < power) {Serial.println("${\"status\":\"ERR power\"}#");return;}

  mode = getNextToken().toInt();
  if (mode < 1 || 5 < mode) {Serial.println("${\"status\":\"ERR mode\"}#");return;}

  fan = getNextToken().toInt();
  if (fan < 0 || 3 < fan) {Serial.println("${\"status\":\"ERR fan\"}#");return;}

  tempNotif = getNextToken().toInt();
  if (tempNotif < 0 || 1 < tempNotif) {Serial.println("${\"status\":\"ERR tempNotif\"}#");return;}

  iFeel = getNextToken().toInt();
  if (iFeel < 0 || 1 < iFeel) {Serial.println("${\"status\":\"ERR iFeel\"}#");return;}

  temp = getNextToken().toInt();
  if (!tempNotif && (temp < 16 || 30 < temp)) {Serial.println("${\"status\":\"ERR temperature\"}#");return;}

  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}
  
  //Serial.println("power:"+String(power) + " mode:"+String(mode) + " fan:"+String(fan) + " temp:"+String(temp));
  e.SendElectra(power, mode, fan, temp, 0, 0, tempNotif, iFeel);
  //$2,0,1,3,1,0,21,#
  Serial.println("${\"status\":\"OK\"}#");
}

void setIRLed() {
  int led;
  static int s = 0;

  led = getNextToken().toInt();
  if (led < 0 || 2 < led) {Serial.println("${\"status\":\"ERR led\"}#");return;}

  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}
  
  switch(led) {
    case 0: digitalWrite(IR_LED_PIN, LOW);   break;
    case 1: digitalWrite(IR_LED_PIN, HIGH);  break;
    case 2: digitalWrite(IR_LED_PIN, (s=1-s)?HIGH:LOW);
  }

  Serial.println("${\"status\":\"OK\"}#");
  //$3,1,#
}

void tvCommand() {
  unsigned long cmd, scmd;

  cmd = getNextToken().toInt();
  if (!cmd) {
    Serial.println("${\"status\":\"ERR Invalid/NaN cmd\"}#");
    return;
  }

  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}

  switch (cmd) {
    case 1:  scmd = 0xE0E09966; break; // ON
    case 2:  scmd = 0xE0E019E6; break; // OFF
    case 3:  scmd = 0xE0E040BF; break; // Toggle ON/Off
    default: scmd = cmd;        break; // RAW
  }
  for (int i=0; i<3; ++i) {
    irs.sendSAMSUNG(scmd, 32); break;
    delay(50);
  }
  Serial.println("${\"status\":\"OK\"}#");
}

void pioneerReceiver() {
  unsigned long cmd, scmd;

  cmd = getNextToken().toInt();
  if (!cmd) {
    Serial.println("${\"status\":\"ERR Invalid/NaN cmd\"}#");
    return;
  }

  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}

  switch (cmd) {
    case 1:  scmd = 0xA55A58A7; break; // ON
    case 2:  scmd = 0xA55AD827; break; // OFF
    case 3:  scmd = 0xA55A38C7; break; // Toggle ON/Off
    default: scmd = cmd;        break; // RAW
  }
  for (int i=0; i<4; ++i) {
    irs.sendNEC(scmd, 32);
    delay(26);
  }

  Serial.println("${\"status\":\"OK\"}#");
}


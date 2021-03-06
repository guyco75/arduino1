
#include "IRelectra.h"
#include <DHT.h>

IRsend irs;

//#define REPLY_DEBUG

String rxCmdStr;

#define IR_EMITTER     (3)  // Place holder (hardcoded in the lib)
#define DHT_PIN        (5)
#define NIGHT_LED_PIN  (9)
#define BUILT_IN_LED   (13)
#define AC_STATE_PIN   (A5)

DHT dht(DHT_PIN, DHT22);

void setup() {
  Serial.begin(57600);
  Serial.println("$ready#");

  pinMode(BUILT_IN_LED, OUTPUT);
  pinMode(NIGHT_LED_PIN, OUTPUT);
  pinMode(AC_STATE_PIN, INPUT_PULLUP);
  rxCmdStr.reserve(128);

  dht.begin();
}

void loop() {
  static boolean inSync = false;
  static int ac_state = -1;
  static unsigned long ac_state_change_count = 0;

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

  int t = !digitalRead(AC_STATE_PIN);
  if (t != ac_state) {
    ac_state_change_count ++;
    if (ac_state_change_count > 10) {
      ac_state = t;
      Serial.print("${\"ac_state\":\"");
      Serial.print(ac_state);
      Serial.print("\"}#");
      Serial.println();
      ac_state_change_count = 0;
    }
    delay(1);
  } else {
    ac_state_change_count = 0;
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
    case 6: teacCableSAT(); break;
    case 7: nightLEDs(); break;
  }
}

void getTemperature() {
  int chk;
  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  chk = !isnan(t) && !isnan(h);

  Serial.print("${\"status\":\"");
  Serial.print(chk);
  Serial.print("\",\"temperature\":\"");
  Serial.print(t, 1);
  Serial.print("\",\"humidity\":\"");
  Serial.print(h, 1);
  Serial.print("\"}#");
  Serial.println();
  delay(1);
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
    case 0: digitalWrite(BUILT_IN_LED, LOW);   break;
    case 1: digitalWrite(BUILT_IN_LED, HIGH);  break;
    case 2: digitalWrite(BUILT_IN_LED, (s=1-s)?HIGH:LOW);
  }

  Serial.println("${\"status\":\"OK\"}#");
  //$3,1,#
  delay(1);
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
    case 4:  scmd = 0xE0E0D12E; break; // HDMI
    default: scmd = cmd;        break; // RAW
  }
  for (int i=0; i<3; ++i) {
    irs.sendSAMSUNG(scmd, 32);
    delay(46);
  }
  Serial.println("${\"status\":\"OK\"}#");
  delay(1);
}

void pioneerReceiver() {
  uint32_t cmd, scmd1, scmd2;
  uint8_t n;

  n = getNextToken().toInt();
  if (!n) {
    Serial.println("${\"status\":\"ERR Invalid/NaN n\"}#");
    return;
  }

  for (int i=0; i<n; ++i) {
    cmd = getNextToken().toInt();
    if (!cmd) {
      Serial.println("${\"status\":\"ERR Invalid/NaN cmd\"}#");
      return;
    }

    scmd2 = 0;
    switch (cmd) {
      case 1:  scmd1 = 0xA55A58A7; break; // ON
      case 2:  scmd1 = 0xA55AD827; break; // OFF
      case 3:  scmd1 = 0xA55A38C7; break; // Toggle ON/Off
      case 4:  scmd1 = 0xA55A3AC5;
               scmd2 = 0xA55A03FC; break; // BD
      case 5:  scmd1 = 0xA55A08F7; break; // Cable/SAT
      default: scmd1 = cmd;        break; // RAW
    }

    irs.sendNEC(scmd1, 32);
    delay(26);
    if (scmd2) {
      irs.sendNEC(scmd2, 32);
      delay(26);
    }
  }

  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}
  Serial.println("${\"status\":\"OK\"}#");
  delay(1);
}

void teacCableSAT() {
  unsigned int irSignal[] = {
8936,4500,540,584,544,584,512,596,540,584,544,584,516,592,536,588,540,584,520,1720,540,1692,540,1712,512,1720,540,1692,540,1716,512,596,536,1716,512,1720,540,584,512,596,540,1712,516,1720,544,576,520,1720,540,584,512,596,540,1712,512,1720,544,580,520,588,544,1708,520,588,540,1716,512,42036,8944,2260,516,65535,8916,2276,536
  };

  irs.sendRaw(irSignal, sizeof(irSignal) / sizeof(irSignal[0]), 40);

  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}
  Serial.println("${\"status\":\"OK\"}#");
  delay(26);
}

void nightLEDs() {
  int intensity;

  intensity = getNextToken().toInt();
  if (intensity < 0 || 255 < intensity) {Serial.println("${\"status\":\"ERR intensity\"}#");return;}

  if (!verifyEnding()) {Serial.println("${\"status\":\"ERR ending\"}#");return;}

  analogWrite(NIGHT_LED_PIN, intensity);
  Serial.println("${\"status\":\"OK\"}#");
  delay(1);
}


/*
 * ESP8266 Wifi Thermostat
 * Written by Harrison Pace 2017
 * INSERT GPL LICENSE HERE
 */

/* Seven Segment Display Map */

   //OFF ~0x00
   //0- ~0x3F
   //1- ~0x06
   //2- ~0x5B
   //3- ~0x4F
   //4- ~0x66
   //5- ~0x6D
   //6- ~0x7D
   //7- ~0x07
   //8- ~0x7F
   //9- ~0x67 or ~0x6F
   //C- ~0x39
   //c- ~0x58
   //-  ~0x40

 /* With Dot '.' */

   //OFF ~0x80
   //0- ~0xBF
   //1- ~0x86
   //2- ~0xDB
   //3- ~0xCF
   //4- ~0xE6
   //5- ~0xED
   //6- ~0xFD
   //7- ~0x87
   //8- ~0xFF
   //9- ~0xE7 or ~0xEF
   //C- ~0xB9
   //c- ~0xD8
   //-  ~0xC0

/* End */

#include "Arduino.h"
#include "DHT.h"
#include "ESP8266WiFi.h"
#include <BlynkSimpleEsp8266.h>
#include <EEPROM.h>
#include <SNTPtime.h>

SNTPtime NTPau("au.pool.ntp.org"); //Set NTP Pool

//#define PowerButton D8 //Define PowerButton
//#define UpButton D5 //Define Up Button
//#define DownButton D6 //Define Down Button
#define LATCH D2 //Define Shift Register Latch Pin
#define CLK D1 //Define Shift Register Clock Pin
#define DATA D0 //Define Shift Register Data Pin
#define DHTPIN D3 //Define DHT Sensor Data Pin
#define buttonInput A0 //Define Analouge Read Pin for Button Matrix
#define HeaterSSR D4 //Define Relay
#define LEDINTPWM D7 //Define LED Indicator PWM
#define PIR D5
#define ON 1 //Define ON State for Readability
#define OFF 0 //Define OFF State for Readability

#define DHTTYPE DHT22 //Define DHT Sensor
DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor.

char auth[] = ""; //blynk auth code
char ssid[] = ""; //AP SSID
char pass[] = ""; //SSID PASS

strDateTime dateTime; //set datetime

//This is the hex value of each number stored in an array by index num (Useful for Counts)
byte digitOne[10]= {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67};
byte digitTwo[10]= {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67};
byte digitThree[10]= {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67};
byte digitFour[10]= {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x67};

unsigned long lastUpdate;
unsigned long lastTempCheck;
unsigned long lastNTPUpdate;
unsigned long DisplayTimeout;
unsigned long remoteUpdate;
unsigned long debounceDelay = 100;
unsigned long autoShutoff;

float currentTemp;
int enabledState;
int requiredTemp;
int runtime;
int shutoffmode;

void setup(){

  Serial.begin(115200);
  Serial.println();
  Serial.println("Booted");

  //Redundant Wi-Fi Code (handled by Blynk API)
  /*
  Serial.println("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin (ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi connected");
  */

  Serial.println("Initialising....");
  Blynk.begin(auth, ssid, pass);

  Serial.println("Still Alive");

  Serial.println("Setting Time...");
  while (!NTPau.setSNTPtime()) Serial.print("."); // set internal clock
  Serial.println();
  Serial.println("Time set");


  lastUpdate = millis();
  lastTempCheck = millis();
  lastNTPUpdate = millis();
  remoteUpdate = 0;
  runtime = 2;

  pinMode(LATCH, OUTPUT);
  pinMode(CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  dht.begin();
  EEPROM.begin(512);

  enabledState = EEPROM.read(1);
  Serial.println("EEPROM: Heater is  " + String(enabledState ? "ENABLED" : "DISABLED"));
  requiredTemp = EEPROM.read(2);
  Serial.println("EEPROM: Target Temperature is " + String(requiredTemp) + "°C");
}

byte MapNumber (int x){
  byte a;
  switch ( x ) {
    case 0:
      a = 0x3F;
      return a;
    case 1:
      a = 0x06;
      return a;
    case 2:
      a = 0x5B;
      return a;
    case 3:
      a = 0x4F;
      return a;
    case 4:
      a = 0x66;
      return a;
    case 5:
      a = 0x6D;
      return a;
    case 6:
      a = 0x7D;
      return a;
    case 7:
      a = 0x07;
      return a;
    case 8:
      a = 0x7F;
      return a;
    case 9:
      a = 0x67;
      return a;
    default:
      a = 0x40; // something has gone wrong
      return a;
  }
}

byte AddDot (byte x){
  byte o = x + 0x80;
  return o;
}

void DisplayTemp(float t){
  t = t * 10; //move first decimal place
  int ctem = (int)t; //cast temp float as int

  int first  = ctem / 100;
  int second = ctem / 10 % 10;
  int third  = ctem % 10;
  //Remove '0' Displaying after decimal place
  byte d1;
  if (first == 0){
    d1 = 0x00;
  } else {
    d1 = MapNumber(first);
  }
  //byte d1 = MapNumber(first);
  byte d2 = AddDot(MapNumber(second));
  byte d3 = MapNumber(third);
  byte d4 = 0x58; //'c' Symbol
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLK, MSBFIRST, ~d4); // digitFour
  shiftOut(DATA, CLK, MSBFIRST, ~d3); // digitThree
  shiftOut(DATA, CLK, MSBFIRST, ~d2); // digitTwo
  shiftOut(DATA, CLK, MSBFIRST, ~d1); // digitOne
  digitalWrite(LATCH, HIGH);
}

void DisplayReq()
{
  float t = requiredTemp;
  DisplayTemp(t);
}

void DisplayDash(){
  byte dash = 0x40; //'-' Symbol
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLK, MSBFIRST, ~dash); // digitFour
  shiftOut(DATA, CLK, MSBFIRST, ~dash); // digitThree
  shiftOut(DATA, CLK, MSBFIRST, ~dash); // digitTwo
  shiftOut(DATA, CLK, MSBFIRST, ~dash); // digitOne
  digitalWrite(LATCH, HIGH);
}

void tempUpdate()
{
  float humd = dht.readHumidity(); Blynk.run();
  currentTemp = dht.readTemperature();
  DisplayTemp(currentTemp);
  Blynk.run();
  Blynk.virtualWrite(V4, currentTemp); Blynk.run();
  Blynk.virtualWrite(V5, humd); Blynk.run();

}

void motionDetect() {
  int pirState = 0;
  pirState = digitalRead(PIR);
  if (pirState == 1) {
    Serial.println("Motion Detected"); //debug - no PIR sensor = float pin
    autoShutoff = millis(); // reset timer upon motion detection
  }
}

BLYNK_CONNECTED() {
  Serial.println("CONNECTED");  Blynk.run();
  Blynk.virtualWrite(V2, requiredTemp);  Blynk.run();
  Blynk.virtualWrite(V3, requiredTemp);  Blynk.run();
  Blynk.virtualWrite(V0, enabledState);
  Blynk.setProperty(V0, "color", (enabledState ? "#00FF00" : "#FF0000"));  Blynk.run();
  Blynk.virtualWrite(V1, "Ready");  Blynk.run();
  Blynk.setProperty(V1, "color", "#00FF00");  Blynk.run();
  tempUpdate();
}

BLYNK_WRITE(V0)
{
  enabledState = !enabledState;

  EEPROM.write(1, enabledState);
  EEPROM.commit();

  Serial.println("System is " + String(enabledState ? "ENABLED" : "DISABLED"));

  Blynk.virtualWrite(V0, enabledState);
  Blynk.setProperty(V0, "color", enabledState ? "#00FF00" : "#FF0000");  Blynk.run();
  if (enabledState == 1) {
    DisplayReq(); // Display Requested Temp when turned on
    autoShutoff = millis();
  } else {
    DisplayDash(); // Display Dashes when turned off
  }
}

BLYNK_WRITE(V2)
{
  requiredTemp = param.asInt();  Blynk.run();
  Blynk.virtualWrite(V3, requiredTemp);  Blynk.run();
  DisplayReq(); // Display Requested until next update
  remoteUpdate = millis();
  //EEPROM.write(2, requiredTemp);
  //EEPROM.commit();
  Serial.println("Target Temperature is " + String(requiredTemp));
}

/*Automatic Shutoff Selection Control*/
BLYNK_WRITE(V6) {
  switch (param.asInt())
  {
    case 1: // Item 1
      Serial.println("On selected");
      shutoffmode = 1;
      break;
    case 2: // Item 2
      Serial.println("Motion selected");
      shutoffmode = 2;
      break;
    case 3: // Item 3
      Serial.println("Off selected");
      shutoffmode = 3;
      break;
    default:
      Serial.println("Unknown item selected");
  }
}

BLYNK_WRITE(V7)
{
  runtime = param.asInt();
  Serial.println("Runtime is " + String(runtime));
  Blynk.virtualWrite(V8, runtime);  Blynk.run();
}

BLYNK_WRITE(V9) {
  int schstate = 0;
  Serial.println("EEPROM: Accessed");
  switch (param.asInt())
  {
    case 1: // On
      Serial.println("On selected");
      schstate = 1;
      EEPROM.write(11, schstate);
      EEPROM.commit();
      break;
    case 2: // Item 3
      Serial.println("Off selected");
      schstate = 0;
      EEPROM.write(11, schstate);
      EEPROM.commit();
      break;
    default:
      Serial.println("Unknown item selected");
  }
}

BLYNK_WRITE(V10) {
  TimeInputParam t(param);
  Serial.println("EEPROM: Accessed");

  // Process start time

  if (t.hasStartTime())
  {
    Serial.println(String("Start: ") +
                   t.getStartHour() + ":" +
                   t.getStartMinute() + ":" +
                   t.getStartSecond());
                   EEPROM.write(12, t.getStartHour());
                   EEPROM.write(13, t.getStartMinute());
  }

  // Process stop time
  if (t.hasStopTime())
  {
    Serial.println(String("Stop: ") +
                   t.getStopHour() + ":" +
                   t.getStopMinute() + ":" +
                   t.getStopSecond());
                   EEPROM.write(14, t.getStopHour());
                   EEPROM.write(15, t.getStopMinute());
  }

  // Process weekdays

  for (int i = 1; i <= 7; i++) {
      if (t.isWeekdaySelected(i)) {
        Serial.println(String("Day ") + i + " is selected");
        EEPROM.write(20 + i, 99); //store arbitary identifier
      } else {
        EEPROM.write(20 + i, 0);
      }
    }

    Serial.println();
    EEPROM.commit();
}

BLYNK_WRITE(V11)
{
  int sch1temp = param.asInt();
  Serial.println("Schedule 1 Temp is " + String(sch1temp));
  Blynk.virtualWrite(V12, sch1temp);  Blynk.run();
  Serial.println("EEPROM: Accessed");
  EEPROM.write(16, sch1temp);
  EEPROM.commit();
}

BLYNK_WRITE(V13) {
  TimeInputParam t(param);
  Serial.println("EEPROM: Accessed");

  // Process start time

  if (t.hasStartTime())
  {
    Serial.println(String("Start: ") +
                   t.getStartHour() + ":" +
                   t.getStartMinute() + ":" +
                   t.getStartSecond());
                   EEPROM.write(32, t.getStartHour());
                   EEPROM.write(33, t.getStartMinute());
  }

  // Process stop time

  if (t.hasStopTime())
  {
    Serial.println(String("Stop: ") +
                   t.getStopHour() + ":" +
                   t.getStopMinute() + ":" +
                   t.getStopSecond());
                   EEPROM.write(34, t.getStopHour());
                   EEPROM.write(35, t.getStopMinute());
  }

  // Process weekdays

  for (int i = 1; i <= 7; i++) {
      if (t.isWeekdaySelected(i)) {
        Serial.println(String("Day ") + i + " is selected");
        EEPROM.write(40 + i, 99); //store arbitary identifier
      } else {
        EEPROM.write(40 + i, 0);
      }
    }

    Serial.println();
    EEPROM.commit();
}

BLYNK_WRITE(V14)
{
  int sch2temp = param.asInt();
  Serial.println("Schedule 2 Temp is " + String(sch2temp));
  Blynk.virtualWrite(V15, sch2temp);  Blynk.run();
  Serial.println("EEPROM: Accessed");
  EEPROM.write(36, sch2temp);
  EEPROM.commit();
}

void manualEnable() {
  enabledState = !enabledState;

  if (enabledState == 1) {
    autoShutoff = millis();
  }

  EEPROM.write(1, enabledState);
  EEPROM.commit();

  Serial.println("System is " + String(enabledState ? "ENABLED" : "DISABLED"));

  Blynk.virtualWrite(V0, enabledState);
  Blynk.setProperty(V0, "color", enabledState ? "#00FF00" : "#FF0000");  Blynk.run();
}

strDateTime getcurrentTime() {
  dateTime = NTPau.getTime(10.0, 3); // get time from internal clock
  return dateTime;
}

/*
  This function takes the NTP time and updates the ESP8266 internal timer.
  Due to the large drift in the ESP, this update should happen on a regular
  interval (every hour).
*/

void setTimeNTP() {
  while (!NTPau.setSNTPtime()) Serial.print("."); // set internal clock
  Serial.println();
  Serial.println("Time set");
}

/*
  This function maps the Hardware Buttons to coresponding software values.
  The pupose of using AnalogRead instead of DigitalRead is to save on GPIO.
  Resistor Values Distinguish the Buttons.
*/

int ButtonRead () {
  int buttonpressed = 0;
  int ButtonReadVal = 0;
  ButtonReadVal = analogRead(buttonInput);
  if (ButtonReadVal <= 20) {
    buttonpressed = 0; //no button pressed
  } else if (ButtonReadVal <= 280) {
    buttonpressed = 1; //Left button pressed
  } else if (ButtonReadVal <= 350) {
    buttonpressed = 2; //Right button pressed
  } else {
    buttonpressed = 3; //Power Button pressed
  }
  return buttonpressed;
}

void DisplayHumid(float h){
  h = h * 10; //move first decimal place
  int chum = (int)h; //cast temp float as int

  int first  = chum / 100;
  int second = chum / 10 % 10;
  int third  = chum % 10;
  //Remove '0' Displaying after decimal place
  byte d1;
  if (first == 0){
    d1 = 0x00;
  } else {
    d1 = MapNumber(first);
  }
  //byte d1 = MapNumber(first);
  byte d2 = AddDot(MapNumber(second));
  byte d3 = MapNumber(third);
  byte d4 = 0x40; //'-' Symbol
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLK, MSBFIRST, ~d4); // digitFour
  shiftOut(DATA, CLK, MSBFIRST, ~d3); // digitThree
  shiftOut(DATA, CLK, MSBFIRST, ~d2); // digitTwo
  shiftOut(DATA, CLK, MSBFIRST, ~d1); // digitOne
  digitalWrite(LATCH, HIGH);
}

void DisplayVal(int x){

  int first  = x / 100;
  int second = x / 10 % 10;
  int third  = x % 10;
  byte d1 = MapNumber(first);
  byte d2 = MapNumber(second);
  byte d3 = MapNumber(third);
  byte d4 = 0x40; //'-' Symbol
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLK, MSBFIRST, ~d4); // digitFour
  shiftOut(DATA, CLK, MSBFIRST, ~d3); // digitThree
  shiftOut(DATA, CLK, MSBFIRST, ~d2); // digitTwo
  shiftOut(DATA, CLK, MSBFIRST, ~d1); // digitOne
  digitalWrite(LATCH, HIGH);
}

void DisplayOFF(){
  byte blank = 0x00; //No Symbol
  digitalWrite(LATCH, LOW);
  shiftOut(DATA, CLK, MSBFIRST, ~blank); // digitFour
  shiftOut(DATA, CLK, MSBFIRST, ~blank); // digitThree
  shiftOut(DATA, CLK, MSBFIRST, ~blank); // digitTwo
  shiftOut(DATA, CLK, MSBFIRST, ~blank); // digitOne
  digitalWrite(LATCH, HIGH);
}

void heatingControl(boolean onOff)
{
  Blynk.virtualWrite(V1, String(onOff ? "Heating ON" : "Heating OFF"));  Blynk.run();
  Blynk.setProperty(V1, "color", String(onOff ? "#00FF00" : "#FF0000"));  Blynk.run();
}

void checkTemp()
{
  if ((currentTemp > requiredTemp) && (enabledState == ON)) heatingControl(OFF);
  if ((currentTemp < requiredTemp) && (enabledState == ON)) heatingControl(ON);
  lastTempCheck = millis();
}

void loop() {
  Blynk.run();

  if (((millis() -   lastUpdate) > 2000) && ((millis() -   remoteUpdate) > 2000))
  {
    tempUpdate();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastUpdate = millis();
  }

  if ((millis() -   lastTempCheck) > 60000) checkTemp();

  if ((millis() -   lastNTPUpdate) > 7200000)
  {
    setTimeNTP();
    lastNTPUpdate = millis();
  }

  if ((EEPROM.read(11) == 1) && ((millis() -   autoShutoff) > (runtime * 3600020))) { //THIS WILL NOT WORK FIX THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //check Schedule
    dateTime = getcurrentTime(); // get time from internal clock
    byte actualHour = dateTime.hour;
    byte actualMinute = dateTime.minute;
    byte actualdayofWeek = dateTime.dayofWeek;
    //Schedule 1 Values
    byte StartHour1 = EEPROM.read(12);
    byte StartMinute1 = EEPROM.read(13);
    byte StopHour1 = EEPROM.read(14);
    byte StopMinute1 = EEPROM.read(15);
    int sch1temp = EEPROM.read(16);
    //Schedule 2 Values
    byte StartHour2 = EEPROM.read(22);
    byte StartMinute2 = EEPROM.read(23);
    byte StopHour2 = EEPROM.read(24);
    byte StopMinute2 = EEPROM.read(25);
    int sch2temp = EEPROM.read(26);

    for (int i = 21; i <= 27; i++) {
        if (EEPROM.read(i) == 99) {
          if (((i -20)+1) == actualdayofWeek) { //THIS WILL NOT WORK FIX THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            if ((StartHour1 <= actualHour) && (actualHour <= StopHour1)){
              if (StartHour1 == actualHour) {
                if (StartMinute1 < actualMinute){

                  if (requiredTemp != sch1temp) {
                    requiredTemp = sch1temp;  Blynk.run(); //THIS WILL NOT WORK FIX THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                    Blynk.virtualWrite(V3, requiredTemp);  Blynk.run();
                    //Serial.println("Scheduled Temp");
                  }
                  if (enabledState == OFF) {
                    enabledState = ON;
                    if (EEPROM.read(1) != enabledState) {
                      EEPROM.write(1, enabledState);
                      EEPROM.commit();
                      Blynk.virtualWrite(V0, enabledState);
                      Blynk.setProperty(V0, "color", enabledState ? "#00FF00" : "#FF0000");  Blynk.run();
                    }
                  }
                }
              } else if (StopHour1 == actualHour) {
                if (StopMinute1 > actualMinute){

                  if (requiredTemp != sch1temp) {
                    requiredTemp = sch1temp;  Blynk.run();
                    Blynk.virtualWrite(V3, requiredTemp);  Blynk.run();
                    //Serial.println("Scheduled Temp");
                  }
                  if (enabledState == OFF) {
                    enabledState = ON;
                    if (EEPROM.read(1) != enabledState) {
                      EEPROM.write(1, enabledState);
                      EEPROM.commit();
                      Blynk.virtualWrite(V0, enabledState);
                      Blynk.setProperty(V0, "color", enabledState ? "#00FF00" : "#FF0000");  Blynk.run();
                    }
                  }
                }
              } else {

                if (requiredTemp != sch1temp) {
                  requiredTemp = sch1temp;  Blynk.run();
                  Blynk.virtualWrite(V3, requiredTemp);  Blynk.run();
                  //Serial.println("Scheduled Temp");
                }
                if (enabledState == OFF) { //THIS WILL NOT WORK FIX THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                  enabledState = ON;
                  if (EEPROM.read(1) != enabledState) {
                    EEPROM.write(1, enabledState);
                    EEPROM.commit();
                    Blynk.virtualWrite(V0, enabledState);
                    Blynk.setProperty(V0, "color", enabledState ? "#00FF00" : "#FF0000");  Blynk.run();
                    Blynk.run();
                  }
                }
              }
            }
          }
        }
      }
    }
/*
      for (int i = 41; i <= 47; i++) {
          if (EEPROM.read(i) == 99) {
            if (((i -40)+1) == actualdayofWeek) {
              if ((StartHour2 <= actualHour) && (actualHour <= StopHour2)){
                if (StartHour2 == actualHour) {
                  if (StartMinute2 < actualMinute){
                    //run - make
                    Serial.println("wow1");
                  }
                } else if (StopHour2 == actualHour) {
                  if (StopMinute2 > actualMinute){
                    //run
                    Serial.println("wow2");
                  }
                } else {
                  //run
                  Serial.println("wow3");
                }
              }
            }
          }
        }



  }
  */

  if ((shutoffmode == 1) && (enabledState == ON) && ((millis() -   autoShutoff) > (runtime * 3600000))) {
    enabledState = OFF;
    if (EEPROM.read(1) != enabledState) {
      Serial.println("EEPROM ACCESSED");
      EEPROM.write(1, enabledState);
      EEPROM.commit();
      Blynk.virtualWrite(V0, enabledState);
      Blynk.setProperty(V0, "color", enabledState ? "#00FF00" : "#FF0000");  Blynk.run();
    }
  }

  //Call Motion Detect to reset Timer if Motion is detected
  if (shutoffmode == 2) {
    motionDetect();
  }

  if ((shutoffmode == 2) && (enabledState == ON) && ((millis() -   autoShutoff) > (runtime * 3600000))) {
    enabledState = OFF;
    if (EEPROM.read(1) != enabledState) {
      Serial.println("EEPROM ACCESSED");
      EEPROM.write(1, enabledState);
      EEPROM.commit();
      Blynk.virtualWrite(V0, enabledState);
      Blynk.setProperty(V0, "color", enabledState ? "#00FF00" : "#FF0000");  Blynk.run();
    }
  }

  bool view = 0;
  unsigned long Delay;

  do {
    int buttonState = ButtonRead();
    Blynk.run();
    yield();
    switch(buttonState) {
      case 0  :
        //No Button Press
        break;
      case 1  :
        //Left
        requiredTemp = requiredTemp -1;  Blynk.run();
        Blynk.virtualWrite(V3, requiredTemp);  Blynk.run();
        Serial.println("Target Temperature is " + String(requiredTemp));
        DisplayReq();
        DisplayTimeout = millis();
        Delay = millis() + debounceDelay;
        view = 1;
        break;
      case 2  :
        //Right
        requiredTemp = requiredTemp +1;  Blynk.run();
        Blynk.virtualWrite(V3, requiredTemp);  Blynk.run();
        Serial.println("Target Temperature is " + String(requiredTemp));
        DisplayReq();
        DisplayTimeout = millis();
        Delay = millis() + debounceDelay;
        view = 1;
        break;
      case 3  :
        //Power
        if (enabledState == ON) {
          DisplayDash();
          manualEnable();
          DisplayTimeout = millis();
          Delay = millis() + debounceDelay;
          view = 1;
        } else {
          manualEnable();
          DisplayReq();
          DisplayTimeout = millis();
          Delay = millis() + debounceDelay;
          view = 1;
        }
        break;
     default :
        //No Defined Input
        break;
        Blynk.run();
        yield();
    }
    while ((Delay > millis()) && view == 1) {
      //wait
      Blynk.run();
      yield();
    }
    Blynk.run();
    yield();
  } while(((millis() -   DisplayTimeout) < 1500) && view == 1);

  view = 0;
}

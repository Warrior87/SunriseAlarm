//Sunrise alarm, simulates the sunrise at the time you specify though Blynk
//will also have party light capabilities

/*
 * 1/19/2019: -may have fixed bug where lights would begin wakeup routiene at midnight (00:00) 24hr format
 *            -device resets if it cannot get an NTP response, after doing so, the wakeup time would be
 *            reset to 00:00
 *            -fixed by syncing with Blynk when device first powers on (resets)
 *            
 * 2/12/2019: -added robust fading algorithm for customColor() function
 * 
 * 2/13/2019: -adding PIR functionality to automatically start the sunset funciton
 *            -should add functionality to turn on the lights during the day
 *            
 * 5/14/2019: -fixed daylight savings time bug by adding button
 *            -fixed bug where user had to wait 1 min for susnet to turn on
 *            
 * add debug for serial prints
 * add routerLogin calls
 */

//#define DEBUG

#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.print (x)
  #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <BlynkSimpleEsp8266.h>
#include <routerLogin.h>
#include <BlynkToken.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
const char* auth = AUTH;

const byte redPin = 14; //D5
const byte bluePin = 13; //D7
const byte greenPin = 12; //D6
byte r = 0;
byte g = 0;
byte b = 0;
byte custom_r;
byte custom_g;
byte custom_b;
int minutes = 0;
int seconds = 0;
int hours = 0;
bool wakeUp = false;
bool blynk_wakeUp = false;
bool blynk_sunset = false;
bool blynk_autolight = false;
bool blynk_daylightSavings = false;
bool custom = false;
bool blynk_custom = false;
bool prev_custom_state = true;
bool PIR_state = false;
bool movement_flag = false;
byte PIR_pin = 4;
//weekday();         // day of the week (1-7), Sunday is day 1
byte prevWakeUpDay;
byte prevSunsetDay;
int weekdayWakeupHour = 0;
int weekdayWakeupMinute = 0;
int weekendWakeupHour = 0;
int weekendWakeupMinute = 0;
int weekdaySunsetHour = 9;
int weekdaySunsetMinute = 40;
unsigned long prevMinTime = 0;
unsigned long prevSecTime = 0;
const int wakeupDuration = 45;                  /*duration of the total wakeup routine in minutes*/
const int sunsetDuration = 30;
unsigned long prevDimTime = 0;
unsigned long autolight_on_time;

const char* ssid = STASSID;
const char* pass = STAPSK;

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = -5;  // Eastern Standard Time (USA)


WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

//BLYNK_APP_CONNECTED() {
//    DEBUG_PRINTLN("sync event");
//    Blynk.syncAll();
//}

BLYNK_WRITE(V0)
{
    prev_custom_state = blynk_custom;
    blynk_custom = param.asInt();                        /*get state of custom switch in app*/
    DEBUG_PRINTLN(blynk_custom);
    DEBUG_PRINT("new RGB: ");
    DEBUG_PRINT(custom_r);
    DEBUG_PRINT(",");
    DEBUG_PRINT(custom_g);
    DEBUG_PRINT(",");
    DEBUG_PRINTLN(custom_b);
    if((!prev_custom_state) && (blynk_custom)){
      Blynk.syncAll();
      customColor(custom_r, custom_g, custom_b);
    }
//    if(blynk_custom){
//      customColor();
//    }
    else if(!blynk_custom){
      custom_r = 0;
      custom_g = 0;
      custom_b = 0;
      customColor(custom_r, custom_g, custom_b);
    }
}

BLYNK_WRITE(V1)
{
    blynk_wakeUp = param.asInt();                        /*get state of wakeup switch in app*/
    DEBUG_PRINT("wakeup: "); DEBUG_PRINTLN(blynk_wakeUp);
}

BLYNK_WRITE(V2)
{
    // get a RED channel value
    custom_r = param[0].asInt();
    // get a GREEN channel value
    custom_g = param[1].asInt();
    // get a BLUE channel value
    custom_b = param[2].asInt();
    
    DEBUG_PRINT(custom_r);
    DEBUG_PRINT(",");
    DEBUG_PRINT(custom_g);
    DEBUG_PRINT(",");
    DEBUG_PRINTLN(custom_b);
    
    if(blynk_custom){
      customColor(custom_r, custom_g, custom_b);
    }
}

BLYNK_WRITE(V3)                                                /*weekday start time*/
{
  TimeInputParam t(param);
  if (t.hasStartTime())
  {
    DEBUG_PRINTLN(String("Start: ") +
                   t.getStartHour() + ":" +
                   t.getStartMinute() + ":" +
                   t.getStartSecond());
    weekdayWakeupHour = t.getStartHour();
    weekdayWakeupMinute = t.getStartMinute();
    DEBUG_PRINT("weekday wakeup time  "); DEBUG_PRINT(weekdayWakeupHour); DEBUG_PRINT(":"); DEBUG_PRINTLN(weekdayWakeupMinute);
  }
//  for (int i = 1; i <= 7; i++) {
//    if (t.isWeekdaySelected(i)) {
//      DEBUG_PRINTLN(String("Day ") + i + " is selected");
//    }
//  }
  //they use 1 as monday, not sunday, shift input values
  
}

BLYNK_WRITE(V4)                                                /*weekend start time*/
{
  TimeInputParam t(param);
  if (t.hasStartTime())
  {
    DEBUG_PRINTLN(String("Start: ") +
                   t.getStartHour() + ":" +
                   t.getStartMinute() + ":" +
                   t.getStartSecond());
    weekendWakeupHour = t.getStartHour();;
    weekendWakeupMinute = t.getStartMinute();
    DEBUG_PRINT("weekend wakeup time  "); DEBUG_PRINT(weekendWakeupHour); DEBUG_PRINT(":"); DEBUG_PRINTLN(weekendWakeupMinute);
  }
//  for (int i = 1; i <= 7; i++) {
//    if (t.isWeekdaySelected(i)) {
//      DEBUG_PRINTLN(String("Day ") + i + " is selected");
//    }
//  }
  //they use 1 as monday, not sunday, shift input values
  
}

BLYNK_WRITE(V5)
{
    blynk_sunset = param.asInt();                        /*get state of sunset switch in app*/
    DEBUG_PRINT("sunset: "); DEBUG_PRINTLN(blynk_sunset);
}

BLYNK_WRITE(V6)
{
    blynk_autolight = param.asInt();                        /*get state of autolight switch in app*/
    DEBUG_PRINT("autolight: "); DEBUG_PRINTLN(blynk_autolight);
}

BLYNK_WRITE(V7)
{
    blynk_daylightSavings = param.asInt();                        /*get state of daylight savings switch in app*/
    DEBUG_PRINT("daylight savings: "); DEBUG_PRINTLN(blynk_daylightSavings);
}

void setup() {
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(PIR_pin, INPUT);
  //digitalWrite(PIR_pin, LOW);
  #ifdef DEBUG
    Serial.begin(115200);
  #endif
    /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
       would try to act as both a client and an access-point and could cause
       network-issues with your other WiFi-devices on your WiFi-network. */
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
  }
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  getNtpTime();
  digitalClockDisplay();
  checkWakeupTime();
//  setSyncInterval(300);
  Blynk.begin(auth, ssid, pass);
  Blynk.run();  
  Blynk.syncAll();
}

void loop() {         //////////////////////////////////////////////see how often we loop through the loop() function///////////////////////////////////////////////////////
  Blynk.run();

//  PIR_state = digitalRead(PIR_pin);
//  if(PIR_state){                                  /*if the movement pin is high*/   
//    DEBUG_PRINTLN("motion detected");
//    checkSunsetTime();
//    checkAutolightTime();
//  }
  if(millis() - autolight_on_time >= 60000){    /*if autolight has been on for a min, turn it off*/
    autolightOff();
  }

  //this updates the current time every second for an hour, so the server is only pinged once an hour
  if((millis() - prevMinTime) >= 60000){          /*if 60 seconds has passed*/
     prevMinTime = millis();
     minutes++;                                   /*increment minute*/
     DEBUG_PRINT("System calculated time  "); DEBUG_PRINT(hours); DEBUG_PRINT(":"); DEBUG_PRINTLN(minutes);
     checkWakeupTime();                           /*check if it is the wakeup time every minute*/
     checkSunsetTime();
     if(wakeUp){
      wakeUpRoutine();
     }
     if(minutes >= 60){    
      minutes = 0;                       
      getNtpTime();                               /*if it has been an hour, get the official time*/
      digitalClockDisplay();
      checkWakeupTime();                          /*re-check if it is the wakeup time, just to be safe*/
      checkSunsetTime();
     }
  }
}

void checkWakeupTime()
{
  if(!(isAM())){                                  /*if it is PM, exit*/
    wakeUp = false;
    return;
  }
  if(weekday() != 1 && weekday() != 7){           /*if it is a weekday*/
    DEBUG_PRINTLN("it is a weekday");
    if(hours < weekdayWakeupHour){                /*if it is before we need to wakeup hour, exit*/
      wakeUp = false;
      return;
    }
    else if(hours == weekdayWakeupHour && minutes >= weekdayWakeupMinute){
      wakeUp = true;
      return;
    }
  }
  else{
    DEBUG_PRINTLN("it is a weekend");
    if(hours < weekendWakeupHour){                /*if it is before we need to wakeup hour, exit*/
      wakeUp = false;
      return;
    }
    else if(hours == weekendWakeupHour && minutes >= weekendWakeupMinute){
      wakeUp = true;
      return;
    }
  }
  wakeUp = false;
}

void checkAutolightTime()
{
  DEBUG_PRINTLN("autolight time checking");
  if(blynk_custom){                     /*if custom color is selected, dont turn on autolight*/
    return;
  }
  if(!blynk_autolight){
    return;
  }
  if(isPM()){                                   /*want to operate between hours of 8am to 9pm*/
    if(hours < 9){
      autolightOn();
    }
    else{
      return;
    }
  }
  if(isAM()){
    if(hours >= 8){
      autolightOn();
    }
    else{
      return;
    }
  }
}

void checkSunsetTime()
{
  if(blynk_custom){                     /*if custom color is selected, dont turn on autolight*/
    return;
  }
  if(!blynk_sunset){
    return;
  }
  if(!(isPM())){
    return;
  }
  if(weekday() != 1 && weekday() != 7){           /*if it is a weekday*/
    if(hours < 9){                                /*if it is before we need to sunset hour, exit*/
      return;
    }
    if(hours >= 11){                              /*if it is after 11, exit*/
      return;
    }
    if(prevSunsetDay == weekday()){              /*if we have woken up today already, exit the routiene*/
      return;
    }
    else{
      prevSunsetDay = weekday();                  /*otherwise, reset the day we have woken up*/
      sunset();
      return;
    }
  }
}

void wakeUpRoutine()
{
  if(prevWakeUpDay == weekday()){              /*if we have woken up today already, exit the routiene*/
    wakeUp = false;
    return;
  }
  prevWakeUpDay = weekday();                  /*otherwise, reset the day we have woken up*/
  DEBUG_PRINTLN("wakeup routiene entered");
  DEBUG_PRINT("weekday wakeup time  "); DEBUG_PRINT(weekdayWakeupHour); DEBUG_PRINT(":"); DEBUG_PRINTLN(weekdayWakeupMinute);
  DEBUG_PRINT("weekend wakeup time  "); DEBUG_PRINT(weekendWakeupHour); DEBUG_PRINT(":"); DEBUG_PRINTLN(weekendWakeupMinute);
  DEBUG_PRINT("System time before contacting server  "); DEBUG_PRINT(hours); DEBUG_PRINT(":"); DEBUG_PRINTLN(minutes);
  getNtpTime();                              
  digitalClockDisplay();
  DEBUG_PRINT("System time after contacting server  "); DEBUG_PRINT(hours); DEBUG_PRINT(":"); DEBUG_PRINTLN(minutes);
  
  double brightPercent = 0;
  int wakeupElapsed = 0;                          /*elapsed time of the wakeup routine in minutes*/
  
  while((wakeupElapsed < wakeupDuration) && wakeUp && blynk_wakeUp){          /*enter into while loop for 45 min*/
    Blynk.run();
    if((millis() - prevSecTime) >= 1000){
      prevSecTime = millis();
      seconds++;
      
      
      if(seconds >= 60){                             /*if 60 seconds has passed*/
        seconds = 0;
        minutes++;                                   /*increment minute*/
        //do brightening in this area
        if(brightPercent < 1){
          brightPercent = brightPercent + 0.02;
          DEBUG_PRINT("brightPercent  "); DEBUG_PRINTLN(brightPercent);
        }
        r = 1023 * brightPercent;
        b = 80 * brightPercent;
        g = 401 * brightPercent;
        analogWrite(redPin, r);
        analogWrite(bluePin, b);
        analogWrite(greenPin, g);
        DEBUG_PRINT(r);
        DEBUG_PRINT(",");
        DEBUG_PRINT(b);
        DEBUG_PRINT(",");
        DEBUG_PRINTLN(g);
        if(minutes >= 60){    
          minutes = 0;                       
          getNtpTime();                               /*if it has been an hour, get the official time*/
          digitalClockDisplay();
    //      checkWakeupTime();                          /*re-check if it is the wakeup time, just to be safe*/
       }
        wakeupElapsed++;                             /*increment time elapsed*/
//        getNtpTime();                                /*if it has been a minute, get the official time*/
//        digitalClockDisplay();                       /*check if it is the wakeup time every minute*/
        //checkWakeupTime();                           /*re-check if it is the wakeup time, just to be safe*/
      }
    }
    /*brighten from 0 to 100 in 15 min timespan (1% increment every second)*/

    

    /*
     * sunrise values
     * red = 1023, 68.2/min, 1.13/sec
     * blue = 80, 5.33/min
     * green = 401, 26.7/min
     * 
     * red = 1023, 68.2/min, 1.13/sec
     * blue = 0
     * green = 200, 13.3/min
     */
  }
  wakeUp = false;
  analogWrite(redPin, 0);
  analogWrite(bluePin, 0);
  analogWrite(greenPin, 0);
}

void sunset()
{  
  double sunsetBrightness = 0.5;
  int sunsetElapsed = 0;
  r = 1023 * sunsetBrightness;                      /*turn the lights on so we dont have to wait 1 min*/
  b = 80 * sunsetBrightness;
  g = 401 * sunsetBrightness;
  analogWrite(redPin, r);
  analogWrite(bluePin, b);
  analogWrite(greenPin, g);
  
  while((sunsetElapsed < sunsetDuration) && blynk_sunset && (!blynk_custom)){
    Blynk.run();
    if((millis() - prevSecTime) >= 1000){
      prevSecTime = millis();
      seconds++;
    }
    if(seconds >= 60){                             /*if 60 seconds has passed*/
        seconds = 0;
        minutes++;                                   /*increment minute*/
        if(sunsetBrightness > 0){
            sunsetBrightness = sunsetBrightness - 0.01;
            DEBUG_PRINT("sunsetBrightness  "); DEBUG_PRINTLN(sunsetBrightness);
          }
          r = 1023 * sunsetBrightness;
          b = 80 * sunsetBrightness;
          g = 401 * sunsetBrightness;
          analogWrite(redPin, r);
          analogWrite(bluePin, b);
          analogWrite(greenPin, g);
          DEBUG_PRINT(r);
          DEBUG_PRINT(",");
          DEBUG_PRINT(b);
          DEBUG_PRINT(",");
          DEBUG_PRINTLN(g);
        if(minutes >= 60){    
          minutes = 0;                       
       }
        sunsetElapsed++;                             /*increment time elapsed (once a minute)*/
      }
  }
  analogWrite(redPin, 0);
  analogWrite(bluePin, 0);
  analogWrite(greenPin, 0);
  getNtpTime();                               /*if it has been an hour, get the official time*/
  digitalClockDisplay();
}


void customColor(int new_r, int new_g, int new_b)
{  
  double percent = 0.00;
  double distance_r;
  double distance_g;
  double distance_b;

  distance_r = abs(new_r - r);
  distance_g = abs(new_g - g);
  distance_b = abs(new_b - b);
  
  while((percent < 1) && ((distance_r > 5)||(distance_g > 5)||(distance_b > 5))){    /*if current pwm values dont match desired ones, keep dimming towards them*/
    Blynk.run();
    if(millis() - prevDimTime >= 5){                    /*dim a step every x milliseconds*/
      prevDimTime = millis();
      percent = percent + 0.001;
      /*first take abs(new_r - r) to get distance of actual from desired*/
      /*for incrementing pwm value, increment decimal value, multiply by distance, then add to start r*/
      /*for decrementing pwm value, increment decimal value, multiply by distance, then subtract from start r*/
      if(new_r > r){                                         /*check if we need to increment or decrement rgb value*/
        r = r + (percent * distance_r);
      }
      else if(new_r < r){
        r = r - (percent * distance_r);
      }
      if(new_g > g){                                         /*check if we need to increment or decrement rgb value*/
        g = g + (percent * distance_g);
      }
      else if(new_g < g){
        g = g - (percent * distance_g);
      }
      if(new_b > b){                                         /*check if we need to increment or decrement rgb value*/
        b = b + (percent * distance_b);
      }
      else if(new_b < b){
        b = b - (percent * distance_b);
      }
      
      distance_r = abs(new_r - r);                          /*recalculate the distance every iteration*/
      distance_g = abs(new_g - g);
      distance_b = abs(new_b - b);
      
      DEBUG_PRINT(r);
      DEBUG_PRINT("  ");
      DEBUG_PRINT(g);
      DEBUG_PRINT("  ");
      DEBUG_PRINTLN(b);
      analogWrite(redPin, r);
      analogWrite(bluePin, b);
      analogWrite(greenPin, g);
    }
  }
  r = new_r;
  g = new_g;
  b = new_b;
  analogWrite(redPin, r);
  analogWrite(bluePin, b);
  analogWrite(greenPin, g);
  DEBUG_PRINT(r);
  DEBUG_PRINT("  ");
  DEBUG_PRINT(g);
  DEBUG_PRINT("  ");
  DEBUG_PRINTLN(b);
}

void autolightOn()
{
  DEBUG_PRINTLN("autolight on");
  autolight_on_time = millis();
  customColor(1023,1023,1023);
}

void autolightOff()
{
  if(blynk_custom){
    return;
  }
  DEBUG_PRINTLN("autolight off");
  customColor(0,0,0);
}

void digitalClockDisplay()
{
  hours = hour();

  if(blynk_daylightSavings){            //compensate for daylight savings time, if true, add 1 hour
    hours = hours + 1;
  }
  
  if(hours > 12){
    hours = hours - 12;
  }
  minutes = minute();
  seconds = second();
  
  // digital clock display of the time
  DEBUG_PRINT(hour());
  printDigits(minute());
  printDigits(second());
  if(isAM()){
    DEBUG_PRINT(" AM");
  }
  else{
    DEBUG_PRINT(" PM");
  }
  DEBUG_PRINT(" ");
  DEBUG_PRINT(day());
  DEBUG_PRINT(".");
  DEBUG_PRINT(month());
  DEBUG_PRINT(".");
  DEBUG_PRINT(year());
  DEBUG_PRINTLN();
  
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  DEBUG_PRINT(":");
  if (digits < 10)
    DEBUG_PRINT('0');
  DEBUG_PRINT(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  DEBUG_PRINTLN("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  DEBUG_PRINT(ntpServerName);
  DEBUG_PRINT(": ");
  DEBUG_PRINTLN(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      DEBUG_PRINTLN("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  DEBUG_PRINTLN("No NTP Response :-(");  
  ESP.restart();
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

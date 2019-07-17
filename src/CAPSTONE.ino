#include <Wire.h> // I2C library
#include <Adafruit_Sensor.h> //BNO055 9DOF Sensor library dependency
#include <Adafruit_BNO055.h> //BNO055 9DOF Sensor library
#include <utility/imumaths.h>
#include <neomatrix.h> // Adafruit Neopixel RGB board display word library
#include <neopixel.h> // Adafruit Neopixel RGB board library
#include "ntp-time.h" // Current UTC time library
#include <blynk.h> // BLYNK library
#include "SparkCorePolledTimer.h" // Timer library
#include <google-maps-device-locator.h>
#include "Particle.h"
#include "application.h"
#include <JsonParserGeneratorRK.h>
#define PIXEL_PIN A3             // Set up for Neopixel RGB board side 1 Analog Pin
#define PIXEL_PIN_1 A4           // Set up for Neopixel RGB board side 2 Analog Pin
#define PIXEL_COUNT 128          // Neopixel RGB board total pixel count
#define PIXEL_TYPE WS2812B       // Neopixel RGB board total pixel type
SYSTEM_THREAD(ENABLED);
Thread main;
Thread weather;
int currentTimeZone = 5;     
NtpTime* ntpTime;
float x,y,z,ori;
String timeNow;
int timerTime = 10000;
int timerDone, timerOn;
int minutes, seconds, stopWatchOn;
int brightness = 30;
int r = 255;
int g =255;
int b = 255;
float secondsdisplay;
unsigned int nextTime = 30;    // Next time to contact the server
String tempReceived = "";
String currentStatus;
String precipProb;
JsonParserStatic<512, 50> jsonParser;
String global_lat = ""; //variables for location data
String global_lon = "";
String global_ip = ""; //through here
SparkCorePolledTimer updateTimer(timerTime);
Adafruit_BNO055 bno = Adafruit_BNO055(55);
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 8, PIXEL_PIN,
  NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE,
  PIXEL_TYPE);
Adafruit_NeoMatrix matrix_1 = Adafruit_NeoMatrix(16, 8, PIXEL_PIN_1,
  NEO_MATRIX_TOP     + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE,
  PIXEL_TYPE);
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN_1, PIXEL_TYPE);
int x1 = matrix.width();
int x2 = matrix_1.width();
static unsigned long waitMillis;
struct epochMillis now;
void rainbow(uint8_t wait);
uint32_t Wheel(byte WheelPos);


String hhmmss(unsigned long int now, int timeZone)  //format value as "hh:mm:ss"
{
    String hour;
    if(Time.hour(now)-timeZone<0){
        hour = String::format("%02i",Time.hour(now)-timeZone+24);
    } else if(Time.hour(now)-timeZone>24) {
        hour = String::format("%02i",Time.hour(now)-timeZone-24);
    } else {
        hour = String::format("%02i",Time.hour(now)-timeZone);
    }
    String minute = String::format("%02i",Time.minute(now));
    return hour + ":" + minute;
}

void setup(void)
{
  Serial.begin(9600);
  if(!bno.begin())
  {
    while(1);
    Serial.println("not working");
  }
  delay(1000);
  bno.setExtCrystalUse(true);
  pinMode(A3, OUTPUT);
  pinMode(A3,OUTPUT);
  Serial.println("complete");
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(30);
  matrix.setTextColor(matrix.Color(80,255,0));
  matrix_1.begin();
  matrix_1.setTextWrap(false);
  matrix_1.setBrightness(30);
  matrix_1.setTextColor(matrix.Color(80,255,0));
  updateTimer.SetCallback(OnTimer);
  Blynk.begin("8d7cb678e67a48d49e443ce542cbf215",IPAddress(167,99,150,124),8080);
  Particle.subscribe("particle/device/ip", ipHandler);//subscribes to global IP get
  Particle.subscribe("hook-response/geoip", geoIpHandler, MY_DEVICES);
  Particle.subscribe("hook-response/tempRead", tempHandler, MY_DEVICES);
  weather = Thread("name", weatherFunction);
  main = Thread("nubeds", mainFunction);
  pinMode(A5, OUTPUT);
  pinMode(D2, OUTPUT);
  strip.begin();
  strip.show();
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

void weatherFunction(){
  while(1){
  // Step 1: get IP
  Particle.publish("particle/device/ip"); //requests public ip from particle
  delay(4000);
  // Step 2: get geo location
  Particle.publish("geoip", global_ip.c_str(), PRIVATE); //requests data from particle and translates it into a char*
  delay(4000);
  // Step 3: get temp
  char data[64]; //data buffer for integrating lat and lon for webhook
  sprintf(data, "%s,%s", global_lat.c_str(), global_lon.c_str());//move to geoip handler so is step by step
  Particle.publish("tempRead", data, PRIVATE);//                    that way it is event driven
  delay(10000);
  }
}

void tempHandler(const char *event, const char *data) { //callback for temp webhook
  // Handle the integration response
  jsonParser.addString(data); //adds json to buffer
  if (jsonParser.parse()) {
    jsonParser.getOuterValueByKey("tempReceived", tempReceived); //parses
    jsonParser.getOuterValueByKey("currentStatus", currentStatus);
    jsonParser.getOuterValueByKey("percipProb", precipProb);
    // Put code to do something with tempMin and tempMax here
    Serial.printlnf("tempReceived=%s, currentStatus=%s, precipitationProbability:%s", tempReceived.c_str(), currentStatus.c_str(), precipProb.c_str()); //translates String to char* 
  }
  jsonParser.clear();//clears buffer so that geographical data can be parsed correctly
}

void geoIpHandler(const char *event, const char *data) { //callback for geoip webhook
  jsonParser.addString(data); //this input uses a response template on the particle console to simplify the response
  if (jsonParser.parse()) {
    jsonParser.getOuterValueByKey("latitude", global_lat);
    jsonParser.getOuterValueByKey("longitude", global_lon);
    Serial.printlnf("lat=%s,lon=%s", global_lat.c_str(), global_lon.c_str());
  }
  jsonParser.clear(); //clears the parser buffer so the temp can be parsed correctly
}

void ipHandler(const char *event, const char *data) {
  global_ip = data; //sets ip
  Serial.printlnf("global_ip=%s", global_ip.c_str()); //prints ip
}

BLYNK_WRITE(V0){
  timerTime = param.asInt() * 1000;
  Serial.println(timerTime);
}

BLYNK_WRITE(V1){
  Serial.println(param.asInt());
  if(param.asInt()==1){
    timerOn = 1;
  } else {
    timerOn = 0;
  }
}

BLYNK_WRITE(V2){
  Serial.println(param.asInt());
  if(param.asInt()==1){
    stopWatchOn = 1;
  } else {
    stopWatchOn = 0;
  }
}

BLYNK_WRITE(V3){
  brightness = param.asInt();
}

BLYNK_WRITE(V4){
    r = param[0].asInt();
    g = param[1].asInt();
    b = param[2].asInt();
}

BLYNK_WRITE(V6){
  switch(param.asInt()){
    case 1:{
      currentTimeZone = 7;
      break;  
    }
    case 2:{
      currentTimeZone = 6;
      break;
    }
    case 3:{
      currentTimeZone = 5;
      break;
    }
    case 4:{
      currentTimeZone = 4;
      break;
    }
    case 5:{
      currentTimeZone = 3;
      break;
    }
    case 6:{
      currentTimeZone = 0;
      break;
    }
    case 7:{
      currentTimeZone = -1;
      break;
    }
    case 8:{
      currentTimeZone = -2;
      break;
    }
    case 9:{
      currentTimeZone = -3;
      break;
    }
    case 10:{
      currentTimeZone = -4;
      break;
    }
    case 11:{
      currentTimeZone = -8;
      break;
    }
    case 12:{
      currentTimeZone = -9;
      break;
    }
    case 13:{
      currentTimeZone = -10;
      break;
    }
    case 14:{
      currentTimeZone = -12;
      break;
    }
  }
}

void OnTimer() {
  timerDone = true;
  timerOn = false;
}

void timer(){
  if (timerDone==true){
    Blynk.virtualWrite(V1, LOW);
    Blynk.notify("Time's UP!!!");
    x2 = matrix_1.width();
    for(int i = 0;i<54;i++){
    matrix_1.fillScreen(0);
    matrix_1.setCursor(x2, 0);
    tone(D2, 3401, 500); //connect two to D2
    tone(A5, 3817, 500);
    tone(D2, 3817, 500);
    tone(A5, 3401, 500); // connect two to A5
    matrix_1.print(F("Time's Up   "));
    if(--x2 < -36) {
        x2 = matrix_1.width();
        matrix_1.setTextColor(matrix_1.Color(80, 255, 0));
    }
    tone(D2, 3401, 500); //connect two to D2
    tone(A5, 3817, 500);
    tone(D2, 3817, 500);
    tone(A5, 3401, 500); // connect two to A5
    matrix_1.show();
    delay(100);
    }
    timerDone = false;
  } else if(timerOn == true){
    for(int i = timerTime;i>0;i-=1000){
    updateTimer.Update();
    Serial.println(i/1000);
    matrix_1.fillScreen(0);
    matrix_1.setCursor(2, 0);
    matrix_1.print(i/1000);
    if(--x2 < -36) {
      x2 = matrix_1.width();
      matrix_1.setTextColor(matrix_1.Color(80, 255, 0));
    }
    matrix_1.show();
    delay(1000);
    }
  } else {
    matrix_1.fillScreen(0);
    matrix_1.setCursor(x2, 0);
    matrix_1.print(F("TIMER"));
    if(--x2 < -36) {
      x2 = matrix_1.width();
      matrix_1.setTextColor(matrix_1.Color(80, 255, 0));
    }
    matrix_1.show();
    delay(100);
    }
}

void stopwatch(){
  if(stopWatchOn==1){
  secondsdisplay+= 0.01;
  seconds += 1;
  if(seconds==60){
    minutes+= 1;
    secondsdisplay= 0;
    seconds = 0;
    matrix.fillScreen(0);
    matrix.setCursor(1, 0);
    matrix.print(minutes);
    matrix.show();
    delay(500);
    matrix.fillScreen(0);
    matrix.setCursor(0, 0);
    matrix.print("M");
    matrix.show();
    delay(500);
  } else {
    matrix.fillScreen(0);
    matrix.setCursor(1, 0);
    matrix.print(seconds);
    matrix.show();
    delay(1000);
  }
  } else {
    if(seconds!=0 || minutes!=0){
      matrix.fillScreen(0);
      matrix.setCursor(1, 0);
      matrix.print(minutes);
      matrix.show();
      delay(500);
      x1 = matrix.width();
      for(int i =0;i<45;i++){
        matrix.fillScreen(0);
        matrix.setCursor(x1, 0);
        matrix.print("minutes");
        if(--x1 < -36) {
        x1 = matrix.width();
        matrix.setTextColor(matrix.Color(80, 255, 0));
        }
        matrix.show();
        delay(50);
      }
      matrix.fillScreen(0);
      matrix.setCursor(1, 0);
      matrix.print(seconds);
      matrix.show();
      delay(500);
      x1 = matrix.width();
      for(int i =0;i<45;i++){
        matrix.fillScreen(0);
        matrix.setCursor(x1, 0);
        matrix.print("seconds");
        if(--x1 < -36) {
        x1 = matrix.width();
        matrix.setTextColor(matrix.Color(80, 255, 0));
        }
        matrix.show();
        delay(50);
      }
      seconds = 0;
      minutes = 0;
      secondsdisplay = 0;
    } else {
      matrix.fillScreen(0);
      matrix.setCursor(x1, 0);
      matrix.print(F("STOPWATCH"));
      if(--x1 < -36) {
        x1 = matrix.width();
        matrix.setTextColor(matrix.Color(80, 255, 0));
      }
      matrix.show();
      delay(100);
    }
  }
}

void mainFunction()
{
  while(1){
  Blynk.run();
  BLYNK_WRITE(V1);
  BLYNK_WRITE(V0);
  BLYNK_WRITE(V2);
  BLYNK_WRITE(V3);
  matrix.setBrightness(brightness);
  matrix_1.setBrightness(brightness);
  matrix.setTextColor(matrix.Color(r,g,b));
  matrix_1.setTextColor(matrix.Color(r,g,b));
  sensors_event_t accelData, orientationData;
  bno.getEvent(&accelData, Adafruit_BNO055::VECTOR_ACCELEROMETER);
  bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER);
  x = accelData.acceleration.x;
  y = accelData.acceleration.y;
  z = accelData.acceleration.z;
  ori = orientationData.orientation.x;
  ntpTime->nowMillis(&now);
  timeNow = hhmmss(now.seconds, currentTimeZone);
  if(z>9&&x<3.00&&y<3.00){ 
    matrix_1.clear();
    matrix_1.show();
    if(millis() > waitMillis) {
      matrix.fillScreen(0);
      matrix.setCursor(x1, 0);
      matrix.print(timeNow);
      if(--x1 < -36) {
        x1 = matrix.width();
      }
      matrix.show();
      waitMillis = millis() + (300);
    }
  } else if(z<-9&&y<3.00&&x<3.00){
    matrix.clear();
    matrix.show();
    timer();
  } else if(y>9&&x<3.00&&z<3.00){
    matrix_1.clear();
    matrix_1.show();
    stopwatch();
  } else if(y<-9&&x<3.00&&z<3.00){
    matrix.clear();
    matrix.show();
    if (tempReceived!="")
  {
    matrix_1.fillScreen(0);
    matrix_1.setCursor(x1, 0);
    matrix_1.print(tempReceived + "F " + currentStatus + " Rain: " + precipProb + "%");
    if(--x1 < -200) {
    x1 = matrix_1.width();
    }
    matrix_1.show();
    delay(100);
  }
  } else if(x>9&&z<3.00&&y<3.00){
    if (ori>0&&ori<90){
      matrix_1.clear();
      matrix_1.show();
      matrix.fillScreen(matrix.Color(200,200,200));
      matrix.show();
      Serial.println("light");
    } else if (ori>90&&ori<180){
      matrix_1.clear();
      matrix_1.show();
      matrix.fillScreen(matrix.Color(255,255,255));
      matrix.setBrightness(100);
      matrix.show();
      delay(500);
      matrix.setBrightness(0);
      matrix.show();
      delay(500);
      Serial.println("flash");
    } else if (ori>180&&ori<270){
      matrix.clear();
      matrix.show();
      matrix_1.fillScreen(matrix.Color(r,g,b));
      matrix_1.setBrightness(brightness);
      matrix_1.show();
      Serial.println("customlight");
    } else if (ori>270&&ori<360){
      matrix.clear();
      matrix.show();
      rainbow(20);
  } else if(x<-9&&z<3.00&&y<3.00){
    //function
  }
  }
}
}

void loop(){
  delay(CONCURRENT_WAIT_FOREVER);
}
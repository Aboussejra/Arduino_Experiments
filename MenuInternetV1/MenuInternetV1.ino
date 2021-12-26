#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "DHT.h"
#include <analogWrite.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h> 
#include <ArduinoHttpClient.h>

#define BUTTON_GREY 15
#define BUTTON_PINK 32
#define POTENTIOMETER A0
#define LIGHT_LEVEL A1
#define LIGHT_DATA A5
#define BUTTON_A 15
#define BUTTON_B 32
#define BUTTON_C 14
#define DHTPIN 33
#define DHTTYPE DHT22
#define MIN_DELAY 100
#define SLEEP_DELAY 25000
#define SERIAL_DISPLAY_MODE false

// OLED FeatherWing 
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
DHT dht(DHTPIN, DHTTYPE);
Adafruit_NeoPixel strip(1, LIGHT_DATA, NEO_GRB + NEO_KHZ800);


int grey_button_state = LOW;
int pink_button_state = LOW;
int grey_button_state_previous = LOW;
int pink_button_state_previous = LOW;
int analogDiff = 0;
unsigned long grey_previous_press_date = 0;
unsigned long pink_previous_press_date = 0;
unsigned long grey_rise_date = 0;
unsigned long pink_rise_date = 0;
unsigned long grey_fall_date = 0;
unsigned long pink_fall_date = 0;
unsigned long grey_hold_start = 0;
unsigned long pink_hold_start = 0;
unsigned long grey_hold_end = 0;
unsigned long pink_hold_end = 0;
unsigned long analogChangeDate = 0;
bool grey_held = false;
bool pink_held = false;
//WIFI CONNECTION FOR LIGHTS
const char* ssid = "NETGEAR95";           
const char* password = "purpleunicorn212"; 
const char serverAddress[] = "192.168.1.149";
const int port = 80;
WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, port);
bool wifiConnectionAvailable = false;
//DHT stuff
RTC_DATA_ATTR bool RetardUnitsMode = false;
//Potentiometer stuff
RTC_DATA_ATTR bool DimMode = true;
//Light stuff
RTC_DATA_ATTR int LightLevel = 2048;
RTC_DATA_ATTR int LightData = 4095;
RTC_DATA_ATTR bool LightOn = true;
//Menu stuff
bool processed_current_hold = false;
RTC_DATA_ATTR int currentMenuItem = 0;
RTC_DATA_ATTR int menuItemCount = 5;
RTC_DATA_ATTR int menu_previous_analog_val = 0;
RTC_DATA_ATTR bool menu_forceRender = true;
//forceRender only useful for Serial rendering,
//since screen is constantly refreshed

bool isRise(int state, int prevState){
  return (prevState==LOW and state==HIGH);
}
bool isFall(int state, int prevState){
  return (prevState==HIGH and state==LOW);
}
bool isHold(int state, int prevState){
  return (prevState==HIGH and state==HIGH);
}

bool MenuInteraction(unsigned long b1_completed_press_length, unsigned long b1_current_hold_length, unsigned long b2_completed_press_length, unsigned long b2_current_hold_length, int analog_val, float temp, float humid) {
  unsigned long hold_threshold = 3000;
  //ignore completed hold, it's already been processed
  if (b1_completed_press_length > hold_threshold) {
    b1_completed_press_length = 0;
  }
  if (b2_completed_press_length > hold_threshold) {
    b2_completed_press_length = 0;
  }
  analogDiff = abs(analog_val-menu_previous_analog_val);
  
  //Doing stuff
  if (b1_completed_press_length > 0) {
    //we got a simple press on b1 (grey)
    //increment menu item number
    currentMenuItem = (currentMenuItem+1)%menuItemCount;
  }
  if (b2_completed_press_length > 0) {
    //we got a simple press on b2 (pink)
    //do contextual stuff
    contextualButtonPress();
  }
  if (analog_val != menu_previous_analog_val) {
    updateLightInfo(analog_val);
  }
  //We have no use for holding, but at least it's there
  //(thought it'd be usefull for going to sleep, missread)
  
  //Serial NO REFRESH
  //Big oof on that one
  // and b1_current_hold_length==0 and b2_current_hold_length==0
  if (SERIAL_DISPLAY_MODE) {
    if ((menu_forceRender or !(b1_completed_press_length==0 and b2_completed_press_length==0 and analogDiff<64))) {
      RenderMenuSerial(analog_val, temp, humid);
      menu_forceRender = false;
    }
  }
  else {
  //Normally we'd do it all the time to erase possible display errors
  //(and because I think it's good practice)
  RenderMenu(analog_val, temp, humid);
  }

  //update reads previous call values
  menu_previous_analog_val = analog_val;
  return true;
}

void RenderMenu(int analog_val, float temp, float humid) {
  display.clearDisplay();
  display.setCursor(0,0);
  switch (currentMenuItem) {
    case 0:
      //Temperature and humidity
      display.println("   Temperature and \n      humidity");
      display.println();
      display.print("Temperature : ");display.print(temp);display.println(((RetardUnitsMode) ? "F":"C"));
      display.print("Humidity : ");display.print(humid);display.println("%");
      break;
    case 1:
      display.println("  Temperature unit");
      display.println();display.println();display.println();
      display.print("Current : ");display.println(((RetardUnitsMode) ? "F":"C"));
      display.println("                         ");
      break;
    case 2:
      display.println("     Light Toggle");
      display.println();display.println();display.println();
      display.print("Current : ");display.println(((LightOn) ? "ON":"OFF"));
      break;
    case 3:
      display.println(" Potentiometer Action");
      display.println();display.println();display.println();
      display.print("Current : ");display.println(((DimMode) ? "DIM":"COLOR"));
      break;
    case 4:
      display.println("     Go to Sleep ?");
      break;
    default:
      display.println("                         ");
      display.println("  Achievement Unlocked ! ");
      display.println("  How did we get there ? ");
      display.println("                         ");
      break;
  }
  display.display();
}

void RenderMenuSerial(int analog_val, float temp, float humid) {
  switch (currentMenuItem) {
    case 0:
      //Temperature and humidity
      Serial.println("Temperature and humidity ");
      Serial.print("Temperature : ");Serial.print(temp);Serial.println(((RetardUnitsMode) ? "°F":"°C"));
      Serial.print("Humidity : ");Serial.print(humid);Serial.println("%");
      Serial.println("                         ");
      break;
    case 1:
      Serial.println("Temperature unit         ");
      Serial.println("                         ");
      Serial.print("Current : ");Serial.println(((RetardUnitsMode) ? "°F":"°C"));
      Serial.println("                         ");
      break;
    case 2:
      Serial.println("Light Toggle             ");
      Serial.println("                         ");
      Serial.print("Current : ");Serial.println(((LightOn) ? "ON":"OFF"));
      Serial.println("                         ");
      break;
    case 3:
      Serial.println("Potentiometer Action     ");
      Serial.println("                         ");
      Serial.print("Current : ");Serial.println(((DimMode) ? "DIM":"COLOR"));
      Serial.println("                         ");
      break;
    case 4:
      Serial.println("Go to Sleep ?            ");
      Serial.println("                         ");
      Serial.println("                         ");
      Serial.println("                         ");
      break;
    default:
      Serial.println("                         ");
      Serial.println("  Achievement Unlocked ! ");
      Serial.println("  How did we get there ? ");
      Serial.println("                         ");
      break;
  }
}

void a_mimir() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("\n\n\n    Going to sleep");
  display.display();
  delay(200);
  display.clearDisplay();
  display.display();
  menu_forceRender = true;
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15,1);
  esp_deep_sleep_start();
}

void contextualButtonPress() {
  switch (currentMenuItem) {
    case 0:
      //Temperature and humidity
      //Do Nothing
      break;
    case 1:
      //Temperature unit
      //toggle between °F and °C
      RetardUnitsMode = !RetardUnitsMode;
      break;
    case 2:
      //Light toggle
      LightOn = !LightOn;
      LightLevel = (LightOn) ? HIGH:LOW;
      if (wifiConnectionAvailable) {
        String contentType = "application/json";
        String lampStatus;
        if (LightOn) {
          lampStatus = "true";
        }
        else {
          lampStatus = "false";
        }
        String putData = "{\"on\":"+lampStatus+"}";
        display.print(putData);
        client.beginRequest();
        client.put("/api/tVKw0YvtjpSlSKrbBprO1dBnR4aaCygeARmK5Etr/lights/6/state",contentType,putData);
        client.endRequest();
        // read the status code and body of the response
        /*
        int statusCode = client.responseStatusCode();
        String response = client.responseBody();
        
        display.clearDisplay();
        display.setCursor(0,0);
        display.println(statusCode);
        display.println(response);
        display.display();
        delay(2000);*/
      }
      break;
    case 3:
      //Potentiometer action
      DimMode = !DimMode;
      break;
    case 4:
      //Sleep
      a_mimir();
      break;
    default:
      //TODO deactivate this is we're not using serial in general
      if (SERIAL_DISPLAY_MODE) {
        Serial.println("                         ");
        Serial.println("Achievement Unlocked !   ");
        Serial.println("How did we get there ?   ");
        Serial.println("                         ");
      }
      break;
  }
}

void updateLightInfo(int analog_val) {
  //update global variables for output
  if (DimMode) {
    LightLevel = analog_val;
  }
  else {
    LightData = analog_val;
  }
}







void setup() {
  if (SERIAL_DISPLAY_MODE) {
    Serial.begin(9600);
  }
  else {
    display.begin(0x3C, true);
    display.clearDisplay();
    display.display();
    display.setRotation(1);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.clearDisplay();
    display.setCursor(0,0);
    WiFi.begin(ssid, password);
    display.print("Connecting");
    display.display();
    int timeoutCounter=0;
    while (WiFi.status() != WL_CONNECTED && timeoutCounter<20) {
      delay(500);
      display.print(".");
      display.display();
      ++timeoutCounter;
    }
    if (WiFi.status() == WL_CONNECTED) {
      display.println();
      display.print("Connected, IP address: ");
      display.println(WiFi.localIP());
      display.display();
      wifiConnectionAvailable = true;
    }
    else {
      display.println("Failed WiFi Connection");
      display.display();
      wifiConnectionAvailable = false;
    }
  }
  
  //We have resistors so normal input mode
  pinMode(BUTTON_GREY, INPUT);
  pinMode(BUTTON_PINK, INPUT);
  pinMode(LIGHT_LEVEL, OUTPUT);
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(128); // Set BRIGHTNESS (max = 255)
  dht.begin();
}

void loop() {
  //Handling inputs locally
  unsigned long current_time;
  grey_button_state_previous = grey_button_state;
  grey_button_state = digitalRead(BUTTON_GREY);
  pink_button_state_previous = pink_button_state;
  pink_button_state = digitalRead(BUTTON_PINK);
  float temperature = dht.readTemperature(RetardUnitsMode);
  float humidity = dht.readHumidity();
  int analog_level = analogRead(POTENTIOMETER);
  
  current_time = millis();

  if (isnan(temperature)) temperature = 0.0f;
  if (isnan(humidity)) humidity = 0.0f;
  grey_held = false;
  if (isHold(grey_button_state, grey_button_state_previous)) {
    //better track start and finish since loop has no garanteed length
    grey_hold_end = current_time;
    grey_held = true;
  }
  grey_fall_date = grey_rise_date;
  if (isRise(grey_button_state, grey_button_state_previous)) {
    grey_rise_date = current_time;
    grey_hold_start = current_time;
    grey_fall_date = current_time;
  }
  else if (isFall(grey_button_state, grey_button_state_previous)) {
    grey_fall_date = current_time;
    grey_hold_start = grey_hold_end;
    //completed a press
    //checking if it's far enough from the previous one
    if (grey_fall_date-grey_previous_press_date < MIN_DELAY) {
      //ignore the press by 0 length-ing it
      grey_rise_date=grey_fall_date;
    }
    //update last press date
    grey_previous_press_date=grey_fall_date;
    //yes, even if we ignored last press because it was too fast after the last one
    // this will allow us to ignore segments of time where superfast inputs occur
    //Since superfast fake input occurs after real one, should be enough
  }
  //same for pink
  pink_held = false;
  if (isHold(pink_button_state, pink_button_state_previous)) {
    //better track start and finish since loop has no garanteed length
    pink_hold_end = current_time;
    pink_held = true;
  }
  pink_fall_date = pink_rise_date;
  if (isRise(pink_button_state, pink_button_state_previous)) {
    pink_rise_date = current_time;
    pink_hold_start = current_time;
    pink_fall_date = current_time;
  }
  else if (isFall(pink_button_state, pink_button_state_previous)) {
    pink_fall_date = current_time;
    pink_hold_start = pink_hold_end;
    //completed a press
    //checking if it's far enough from the previous one
    if (pink_fall_date-pink_previous_press_date < MIN_DELAY) {
      //ignore the press by 0 length-ing it
      pink_rise_date=pink_fall_date;
    }
    //update last press date
    pink_previous_press_date=pink_fall_date;
    //yes, even if we ignored last press because it was too fast after the last one
    // this will allow us to ignore segments of time where superfast inputs occur
    //Since superfast fake input occurs after real one, should be enough
  }
  if (analogDiff>63) {
    analogChangeDate = current_time;
  }
  MenuInteraction(grey_fall_date-grey_rise_date, grey_hold_end-grey_hold_start, pink_fall_date-pink_rise_date, pink_hold_end-pink_hold_start, analog_level, temperature, humidity);
  //Third one is for analog input change
  if (current_time-grey_previous_press_date > SLEEP_DELAY and current_time-pink_previous_press_date > SLEEP_DELAY and current_time-analogChangeDate > SLEEP_DELAY) {
    a_mimir();
  }

  //At the end, write to output pins for the LED
  digitalWrite(LIGHT_LEVEL, LightOn);
  strip.setBrightness(map(LightLevel,0,4095,0,255));
  //LightData is 12 bits, so each group of 4 should be expanded to 8 bits to make a color
  strip.setPixelColor(0,((LightData>>8)<<4),(((LightData>>4)&15)<<4),((LightData&15)<<4));
  strip.show();
}

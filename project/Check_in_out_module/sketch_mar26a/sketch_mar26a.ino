#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//WiFi connection
const char* ssid = "FRITZ!Box 7590 XO";
const char* password = "96087252974805885212";

const int potentiometer_pin = A0;
int poteValue;

const int push_button_pin1 = D7;
int push_button_value1;
int push_button_count_value1 = 0;

const int push_button_pin2 = D6;
int push_button_value2;
int push_button_count_value2 = 0;

const int switch_pin = D0;
int switch_value;

const char* host = "192.168.178.71";

const int numAvailableFoods = 5;
const String availableFoods[numAvailableFoods] = {"apples", "bananas", "brocolli", "cherries", "grapes"};
StaticJsonDocument<512> foodstuffs;

WiFiClient client;
HTTPClient http;
StaticJsonDocument<512> doc;

int tempSensorValue;
bool tempSensorInRange;
int tempSensorLowParameter;
int tempSensorHighParameter;
int tempSensorOutOfRangeEvents;
int humSensorValue;
bool humSensorInRange;
int humSensorLowParameter;
int humSensorHighParameter;
int humSensorOutOfRangeEvents;

void setup() {
  pinMode(potentiometer_pin, INPUT);
  pinMode(push_button_pin1, INPUT);
  pinMode(push_button_pin2, INPUT);
  pinMode(switch_pin, INPUT);
  
  // Initialize Serial port
  Serial.begin(9600);
  while (!Serial) continue;

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  // Display welcome message
  display.println("Welcome to");
  display.println("the Smart");
  display.println("Pantry!");
  display.display();
  
  // Initialize WiFi library
  WiFi.begin(ssid, password);
  delay(1000);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.println("Waiting to connect...");
  }
  Serial.println("Connected!");

  for(int i = 0; i < numAvailableFoods ; i++){
    JsonObject obj = foodstuffs.createNestedObject(availableFoods[i]);
    obj["name"] = availableFoods[i];
    if(i % 2 == 0){
      obj["present"] = true;  
    } else {
      obj["present"] = false;  
    }
    obj["dateBought"] = NULL;
    obj["goodForDays"] = 3;
  }

//  JsonObject apples = foodstuffs.createNestedObject("apples");
//  apples["name"] = "apples";
//  apples["present"] = true;
//  apples["dateBought"] = "26.03.22";
//  apples["goodForDays"] = 7;
//  
//  JsonObject bananas = foodstuffs.createNestedObject("bananas");
//  bananas["name"] = "bananas";
//  bananas["present"] = true;
//  bananas["dateBought"] = "23.03.22";
//  bananas["goodForDays"] = 5;

  String output;
  serializeJson(foodstuffs, output);
}

void loop(){
  poteValue = analogRead(potentiometer_pin);
  int foodToDisplay = map(poteValue, 1023, 0, 0, numAvailableFoods -1);
//  int displayValue = map(poteValue, 1023, 0, 0, 1);
  if(isMonitorModeActive()){
    getAmbientSensorModuleDataJson();
    setGlobalConditionsVariablesFromJson();
    displayAmbientSensorModuleCurrentConditions();
    delayWithModeChecking(10);
//  } else if(displayValue == 1){
//    display.clearDisplay();
//    display.setTextSize(1);
//    display.setCursor(0,0);
//    display.print("count1: ");
//    display.println(push_button_count_value1);
//    display.print("count2: ");
//    display.println(push_button_count_value2);
//    display.display();
  } else {
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(2);
    display.println(foodstuffs[availableFoods[foodToDisplay]]["name"].as<String>());
    display.setTextSize(1);
    display.print("In pantry: ");
    display.println(foodstuffs[availableFoods[foodToDisplay]]["present"].as<String>());
    if(foodstuffs[availableFoods[foodToDisplay]]["present"].as<bool>()){
      display.print("Date bought: ");
      display.println(foodstuffs[availableFoods[foodToDisplay]]["dateBought"].as<String>());
      display.print("Good for: ");
      display.print(foodstuffs[availableFoods[foodToDisplay]]["goodForDays"].as<String>());
      display.print(" days");  
    }
    display.display();
  }
  
  int prevPushButtonValue1 = push_button_value1;
  if(isPushButtonPressed1()){
    if(prevPushButtonValue1 != push_button_value1 && push_button_value1 == 1){
      if(push_button_count_value1 > 1){
        push_button_count_value1 = 0;  
      } else {
        push_button_count_value1++;
      }
    }
  }  
  int prevPushButtonValue2 = push_button_value2;
  if(isPushButtonPressed2()){
    if(prevPushButtonValue2 != push_button_value2 && push_button_value2 == 1){
      push_button_count_value2++;
    }
  }  
}

void getAmbientSensorModuleDataJson(){
  http.useHTTP10(true);
  http.begin(client, "http://192.168.178.71/json/sensors");
  http.GET();
  deserializeJson(doc, http.getStream());
  Serial.println(doc["temperature sensor"].as<String>());
  Serial.println(doc["humidity sensor"].as<String>());
  http.end();
}

void setGlobalConditionsVariablesFromJson(){
  tempSensorValue = doc["temperature sensor"]["sensorValue"].as<int>();
  tempSensorInRange = doc["temperature sensor"]["inRange"].as<bool>();
  tempSensorLowParameter = doc["temperature sensor"]["lowParameter"].as<int>();
  tempSensorHighParameter = doc["temperature sensor"]["highParameter"].as<int>();
  tempSensorOutOfRangeEvents = doc["temperature sensor"]["outOfRangeEvents"].as<int>();
  humSensorValue = doc["humidity sensor"]["sensorValue"].as<int>();
  humSensorInRange = doc["humidity sensor"]["inRange"].as<bool>();
  humSensorLowParameter = doc["humidity sensor"]["lowParameter"].as<int>();
  humSensorHighParameter = doc["humidity sensor"]["highParameter"].as<int>();
  humSensorOutOfRangeEvents = doc["humidity sensor"]["outOfRangeEvents"].as<int>();
}

void displayAmbientSensorModuleCurrentConditions(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Storage conditions");
  display.setCursor(0,16);
  display.print("T: ");
  display.print(tempSensorValue);
  display.print("C");
  if(tempSensorInRange){
    display.println(" - okay!");
  } else {
    if(tempSensorValue < tempSensorLowParameter){
      display.println(" - too low!");
    } else {
      display.println(" - too high!");  
    }
  }
  display.setCursor(0,30);
  display.print("H: ");
  display.print(humSensorValue);
  display.print("%");
    if(humSensorInRange){
    display.println(" - okay!");
  } else {
    if(humSensorValue < humSensorLowParameter){
      display.println(" - too low!");
    } else {
      display.println(" - too high!");  
    }
  }
  display.display();
}

bool isPushButtonPressed1(){
  push_button_value1 = digitalRead(push_button_pin1);
  if (push_button_value1 == 1){
    return true;
  }
  return false;
}

bool isPushButtonPressed2(){
  push_button_value2 = digitalRead(push_button_pin2);
  if (push_button_value2 == 1){
    return true;
  }
  return false;
}

bool isMonitorModeActive(){
  bool prevSwitchValue = switch_value;
  switch_value = digitalRead(switch_pin);
  if(prevSwitchValue != switch_value){
    display.clearDisplay();
    display.println("loading...");
    display.display();
  }
  if (switch_value == 0){
    return false;
  }
  return true;
}

//delay the polling of the server for 10 seconds 
//but check every second if the user has changed 
//the Switch position away from monitor mode.
//Allows us not to poll the server too often while 
//maintaining responsiveness
void delayWithModeChecking(int waitSeconds){
  for(int i = waitSeconds; i > 0; i--){
    delay(1000);
    if(!isMonitorModeActive()){
      break;
    }
  }
}

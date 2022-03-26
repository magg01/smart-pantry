#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WifiUdp.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//WiFi connection
const char* ssid = "FRITZ!Box 7590 XO";
const char* password = "96087252974805885212";

//define the NTP client for getting the time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
unsigned long epochTime;

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

const int numAvailableFoods = 10;
const String availableFoods[numAvailableFoods][2] = {
  {"Apples", "6"}, {"Bananas", "4"}, {"Blueberry", "3"}, {"brocolli", "6"}, {"cauliflwr", "6"}, {"cherries", "3"}, 
  {"grapes", "5"}, {"potatoes", "7"}, {"onions", "14"}, {"oranges", "6"}
};
StaticJsonDocument<4000> foodstuffs;

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

  //initialise the time client
  timeClient.begin();
  timeClient.setTimeOffset(3600); //GMT +1

  //initialise the fileSystem
  LittleFS.begin();

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
  display.println("Smart");
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
    JsonObject obj = foodstuffs.createNestedObject(availableFoods[i][0]);
    obj["name"] = availableFoods[i][0];
    obj["present"] = false;
    obj["timeEntered"] = NULL;
    obj["goodForDays"] = availableFoods[i][1];
  }

  String output;
  serializeJson(foodstuffs, output);
}

void loop(){
  timeClient.update();
  epochTime = timeClient.getEpochTime();
  
  poteValue = analogRead(potentiometer_pin);
  int currentFood = map(poteValue, 1023, 0, 0, numAvailableFoods -1);
//  int displayValue = map(poteValue, 1023, 0, 0, 1);
  if(isMonitorModeActive()){
    if(push_button_count_value1 == 0){
      getAmbientSensorModuleDataJson();
      setGlobalConditionsVariablesFromJson();
      displayAmbientSensorModuleCurrentConditions();
      delayWithModeChecking(20);
    } else if (push_button_count_value1 == 1){
      displayUrgentItems();
      int prevPushButtonValue1 = push_button_value1;
      if(isPushButtonPressed1()){
        if(prevPushButtonValue1 != push_button_value1 && push_button_value1 == 1){
          if(push_button_count_value1 == 1){
            push_button_count_value1 = 0;
          } else {
            push_button_count_value1++;
          }
        }
      }
    }
  } else {
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(2);
    display.println(foodstuffs[availableFoods[currentFood][0]]["name"].as<String>());
    display.setTextSize(1);
    display.print("In pantry: ");
    display.println(foodstuffs[availableFoods[currentFood][0]]["present"].as<String>());
    if(foodstuffs[availableFoods[currentFood][0]]["present"].as<bool>()){
      display.print("Days kept: ");
      display.println(getDaysSinceEnteredForFoodstuff(availableFoods[currentFood][0]));
      display.print("Eat within: ");
      display.print(getDaysRemainingForFoodstuff(availableFoods[currentFood][0]));
      display.print(" days");
    }
    display.display();
    int prevPushButtonValue1 = push_button_value1;
    if(isPushButtonPressed1()){
      if(prevPushButtonValue1 != push_button_value1 && push_button_value1 == 1){
        foodstuffs[availableFoods[currentFood][0]]["present"] = !foodstuffs[availableFoods[currentFood][0]]["present"].as<bool>();
        foodstuffs[availableFoods[currentFood][0]]["timeEntered"] = epochTime;
      }
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
    display.display();
    if(switch_value == 1){
      display.setCursor(0,0);
      display.setTextSize(1);
      display.println("Storage conditions"); 
      display.setCursor(0, 16);
      display.println("loading...");
      display.display();  
    }
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
  for(int i = waitSeconds * 10; i > 0; i--){
    delay(100);
    if(!isMonitorModeActive()){
      break;
    }
    int prevPushButtonValue1 = push_button_value1;
    if(isPushButtonPressed1()){
      if(prevPushButtonValue1 != push_button_value1 && push_button_value1 == 1){
        if(push_button_count_value1 == 1){
          push_button_count_value1 = 0;
        } else {
          push_button_count_value1++;
        }
        break;
      }
    }
  }
}

void displayUrgentItems(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println("Eat today:");
  display.setTextSize(1);
  for(int i = 0; i < numAvailableFoods; i++){
    if(foodstuffs[availableFoods[i][0]]["present"]){
      if(getDaysRemainingForFoodstuff(availableFoods[i][0]) <= 0){
        display.print(foodstuffs[availableFoods[i][0]]["name"].as<String>());
        display.print(", ");
      }
    }
  }
  display.display();
}

int getDaysRemainingForFoodstuff(String foodstuffName){
  int daysSinceEntered = getDaysSinceEnteredForFoodstuff(foodstuffName);
  return foodstuffs[foodstuffName]["goodForDays"].as<int>() - daysSinceEntered;
}

int getDaysSinceEnteredForFoodstuff(String foodstuffName){ 
  int secondsSinceEntered = epochTime - foodstuffs[foodstuffName]["timeEntered"].as<int>() + 86399 * 3; //simulate almost three days later
  return secondsSinceEntered / 60 / 60 / 24;
}
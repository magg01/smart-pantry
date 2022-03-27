#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <WifiUdp.h>
#include <ESP8266WebServer.h>
#include <HX711_ADC.h>
#include <Wire.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// set the port for the web server
ESP8266WebServer server(80);
//WiFi connection
const char* ssid = "FRITZ!Box 7590 XO";
const char* password = "96087252974805885212";

//define the NTP client for getting the time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
unsigned long epochTime;

//set up the loadcell
HX711_ADC LoadCell(14,12);


const int potentiometer_pin = A0;
int poteValue;
int current_food;

const int push_button_pin1 = D7;
int push_button_value1;
int monitor_mode_screen_selection_value = 0;
int add_remove_mode_screen_selection_value = 0;

const int push_button_pin2 = D8;
int push_button_value2;
int push_button_count_value2 = 0;

const int switch_pin = D0;
int switch_value;

const char* host = "192.168.178.71";

const int numAvailableFoods = 11;
const String availableFoods[numAvailableFoods][2] = {
  {"Apples", "6"}, {"Bananas", "4"}, {"Blueberry", "3"}, {"Brocolli", "6"}, {"Cauliflwr", "6"}, {"Cherries", "3"}, 
  {"Grapes", "5"}, {"Potatoes", "7"}, {"Onions", "14"}, {"Oranges", "6"}, {"Leftovers", "2"}
};
StaticJsonDocument<4000> foodstuffs;

WiFiClient client;
HTTPClient http;
StaticJsonDocument<512> doc;

int numScreensInMonitorMode = 3;
int numScreensInAddRemoveMode = 2;

bool displayIpAddress = false;

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

  LoadCell.begin();
  LoadCell.start(2000);
  LoadCell.setCalFactor(416); //specific to my load cell setup

  //initialise the time client
  timeClient.begin();
  timeClient.setTimeOffset(3600); //GMT +1

  //initialise the fileSystem
  LittleFS.begin();

  //set the server routes
  server.on("/", get_index);
  server.on("/json/pantry", get_pantry_json);
  //initialise the web server
  server.begin();
  
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
    obj["amountWasted[g]"] = NULL;
  }

  String output;
  serializeJson(foodstuffs, output);
}

void loop(){
  timeClient.update();
  epochTime = timeClient.getEpochTime();
  server.handleClient();
  // This keeps the server and serial monitor available 
  Serial.println("Server is running");
  
  poteValue = analogRead(potentiometer_pin);
  current_food = map(poteValue, 1023, 0, 0, numAvailableFoods -1);
  if(isMonitorModeActive()){
    if(monitor_mode_screen_selection_value == 0){
      displayEatTodayItems();
      if(checkForButton1Press()){
        cycleMonitorModeScreens();
      }
    } else if(monitor_mode_screen_selection_value == 1){
      displayEatTomorrowItems();
      if(checkForButton1Press()){
        cycleMonitorModeScreens();
      }
    } else if (monitor_mode_screen_selection_value == 2){
      getAmbientSensorModuleDataJson();
      setGlobalConditionsVariablesFromJson();
      displayAmbientSensorModuleCurrentConditions();
      delayWithResponsiveButtons(20);
    } 
  } else {
    if(add_remove_mode_screen_selection_value == 0){
      display.clearDisplay();
      display.setCursor(0,0);
      display.setTextSize(2);
      display.println(foodstuffs[availableFoods[current_food][0]]["name"].as<String>());
      display.setTextSize(1);
      display.print("In pantry: ");
      display.println(foodstuffs[availableFoods[current_food][0]]["present"].as<String>());
      if(foodstuffs[availableFoods[current_food][0]]["present"].as<bool>()){
        display.print("Days kept: ");
        display.println(getDaysSinceEnteredForFoodstuff(availableFoods[current_food][0]));
        display.print("Eat within: ");
        display.print(getDaysRemainingForFoodstuff(availableFoods[current_food][0]));
        display.print(" days");
      }
      display.display();
      if(checkForButton2Press()){
        foodstuffs[availableFoods[current_food][0]]["present"] = !foodstuffs[availableFoods[current_food][0]]["present"].as<bool>();
        foodstuffs[availableFoods[current_food][0]]["timeEntered"] = epochTime;
      }
    } else if (add_remove_mode_screen_selection_value == 1){
      displayAddWasteScreen();
    }
    if(checkForButton1Press()){
      cycleAddRemoveModeScreens();
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
  display.setCursor(0,32);
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
//but check every 1/10th second if the user has changed 
//the Switch position away from monitor mode, or pushed a
//button. Also keeps the server handling requests.
//Allows us not to poll the server too often while 
//maintaining responsiveness.
void delayWithResponsiveButtons(int waitSeconds){
  for(int i = waitSeconds * 10; i > 0; i--){
    delay(100);
    if(!isMonitorModeActive()){
      break;
    }
    if(checkForButton1Press()){
      cycleMonitorModeScreens();
      break;
    }
    if(checkForButton2Press()){
      display.setCursor(0,48);
      display.print("IP: ");
      display.print(WiFi.localIP().toString());
      display.display();
    }
    server.handleClient();
  }
}

void displayEatTodayItems(){
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

void displayEatTomorrowItems(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("Eat by tomorrow:");
  display.setCursor(0,16);
  for(int i = 0; i < numAvailableFoods; i++){
    if(foodstuffs[availableFoods[i][0]]["present"]){
      if(getDaysRemainingForFoodstuff(availableFoods[i][0]) == 1){
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
  int secondsSinceEntered = epochTime - foodstuffs[foodstuffName]["timeEntered"].as<int>() + 86400 * 3 - 3; //simulate almost three days later
  return secondsSinceEntered / 60 / 60 / 24;
}

bool checkForButton1Press(){
  int prevPushButtonValue1 = push_button_value1;
  if(isPushButtonPressed1()){
    if(prevPushButtonValue1 != push_button_value1 && push_button_value1 == 1){
      return true;
    }
  }
  return false;
}

bool checkForButton2Press(){
  int prevPushButtonValue2 = push_button_value2;
  if(isPushButtonPressed2()){
    if(prevPushButtonValue2 != push_button_value2 && push_button_value2 == 1){
      return true;
    }
  }
  return false;
}

void cycleMonitorModeScreens(){
  if(monitor_mode_screen_selection_value == numScreensInMonitorMode - 1){
    monitor_mode_screen_selection_value = 0;
  } else {
    monitor_mode_screen_selection_value++;
  }
}


void cycleAddRemoveModeScreens(){
  if(add_remove_mode_screen_selection_value == numScreensInAddRemoveMode - 1){
    add_remove_mode_screen_selection_value = 0;
  } else {
    add_remove_mode_screen_selection_value++;
  }
}

///////////////////web server routes//////////////////////
//index
void get_index(){
  String html = "<!DOCTYPE html> <html>";
  html += "<head><meta http_equiv=\"refresh\" content=\"2\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head>";
  html += "<body> <h1>The Smart Pantry Check-in/out Module Dashboard</h1>";
  html += "<p>Welcome to the Smart Pantry dashboard</p>";
  html += "<h2>Pantry contents</h2>";
  html += "<table>";
  html += "<th style=\"border-bottom: 2px solid black;\">Foodstuff</th><th style=\"border-bottom: 2px solid black;\">Days in pantry</th><th style=\"border-bottom: 2px solid black;\">Days until spoilage</th>";
  int presentCount = 0;
  for(int i = 0; i < numAvailableFoods; i++){
    if(foodstuffs[availableFoods[i][0]]["present"].as<bool>()){
      presentCount++;
      String daysSinceEntered = String(getDaysSinceEnteredForFoodstuff(availableFoods[i][0]));
      String daysRemaining = String(getDaysRemainingForFoodstuff(availableFoods[i][0]));
      html += "<tr>";
      html += "<td style=\"border-bottom: 1px solid black;\">" + availableFoods[i][0] + "</td>";
      html += "<td style=\"border-bottom: 1px solid black;\">" + daysSinceEntered + "</td>";
      html += "<td style=\"border-bottom: 1px solid black;\">" + daysRemaining + "</td>";
      html += "</tr>";
    }
  }
  html += "</table>";
  if(presentCount == 0){
    html += "<p>The Pantry is empty!</p>";
  }
  html += "</body> </html>";
  server.send(200, "text/html", html);
}

//json pantry contents
void get_pantry_json(){
  String jsonStr;  
  serializeJsonPretty(foodstuffs, jsonStr);    
  server.send(200, "application/json", jsonStr);
}

void displayAddWasteScreen(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(foodstuffs[availableFoods[current_food][0]]["name"].as<String>());
  display.setTextSize(1);
  display.println("             Add waste");
  display.println("");
  display.print("Wasted[Kg]: ");
  display.println(foodstuffs[availableFoods[current_food][0]]["amountWasted[g]"].as<float>() / 1000);
  display.println("");
  LoadCell.update();
  float weightValue = LoadCell.getData();
  display.print("Add waste[g]: ");
  display.print(weightValue);
  display.display();
  if(checkForButton2Press()){
    foodstuffs[availableFoods[current_food][0]]["amountWasted[g]"] = foodstuffs[availableFoods[current_food][0]]["amountWasted[g]"].as<float>() + weightValue;  
  }
}

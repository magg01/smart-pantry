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

//
const char* host = "192.168.178.71";

WiFiClient client;
HTTPClient http;
DynamicJsonDocument doc(1024);

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
}

void loop(){
  poteValue = analogRead(potentiometer_pin);
  int displayValue = map(poteValue, 1024, 0, 0, 1);
  if(displayValue == 0){
    getAmbientSensorModuleDataJson();
    setGlobalConditionsVariablesFromJson();
    displayAmbientSensorModuleCurrentConditions();    
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("pote: ");
    display.println(poteValue);
    display.display();
  }
  
  delay(1000);
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

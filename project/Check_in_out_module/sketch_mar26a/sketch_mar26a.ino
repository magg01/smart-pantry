#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//WiFi connection
const char* ssid = "FRITZ!Box 7590 XO";
const char* password = "96087252974805885212";

//
const char* host = "192.168.178.71";


void setup() {
  // Initialize Serial port
  Serial.begin(9600);
  while (!Serial) continue;

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Display static text
  display.println("Hello, world!");
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
  Serial.println("I'm here1");
  WiFiClient client;
  HTTPClient http;
  Serial.println("I'm here2");
//  Serial.printf("\n[Connecting to %s ... ", host);
//  if (client.connect(host, 80))
//  {
//    Serial.println("connected]");
//
//    Serial.println("[Sending a request]");
//    client.print(String("GET /json/sensors") + " HTTP/1.1\r\n" +
//                 "Host: " + host + "\r\n" +
//                 "Connection: close\r\n" +
//                 "\r\n"
//                );
//
//    Serial.println("[Response:]");
//    while (client.connected() || client.available())
//    {
//      if (client.available())
//      {
//        String line = client.readStringUntil('\n');
//        Serial.println(line);
//      }
//    }
    http.useHTTP10(true);
    http.begin(client, "http://192.168.178.71/json/sensors");
    Serial.println("I'm here3");
    http.GET();
    Serial.println("I'm here4");
    DynamicJsonDocument doc(1024);
    Serial.println("I'm here5");
    deserializeJson(doc, http.getStream());       
    Serial.println("I'm here6");
    Serial.println(doc["temperature sensor"].as<String>());
    Serial.println(doc["temperature sensor"]["sensorValue"].as<String>());
    Serial.println(doc["humidity sensor"].as<String>());
    http.end();    
    delay(5000);
//    
//    // Allocate the JSON document
//    DynamicJsonDocument doc(1024);
//  
//    // Parse JSON object
//    DeserializationError error = deserializeJson(doc, client);
//    if (error) {
//      Serial.print(F("deserializeJson() failed: "));
//      Serial.println(error.f_str());
//      client.stop();
//      return;
//    }
  
//    // Extract values
//    Serial.println(F("Response:"));
//    Serial.println(doc["temperature sensor"].as<char*>());
//    Serial.println(doc["humidity sensor"].as<char*>());
    
//    client.stop();
//    Serial.println("\n[Disconnected]");
    
//  }
//  else
//  {
//    Serial.println("connection failed!]");
//    client.stop();
//  }
//  delay(5000);
}

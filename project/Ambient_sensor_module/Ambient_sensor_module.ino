#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// set the port for the web server
ESP8266WebServer server(80);

// set wifi credentials - these would need to be changed for the locally available network
const char* ssid = "FRITZ!Box 7590 XO";
const char* password = "96087252974805885212";

//allocate the JSON document for responding to /json/pantry requests
DynamicJsonDocument doc(1024);
//allocate the JSON document for keeping track of the 
StaticJsonDocument<256> parameterConfigs;

const int switch_pin = D0;
int switch_value;

String parameter_configs_filename = "parameterConfigs.txt";

const int led_pin = D2;
const int pir_sensor_pin = D7;
int pir_sensor_value;
const int push_button_pin = D8;
int push_button_value;
int push_button_count_value = 0;

const int potentiometer_pin = A0;
int poteValue;

//initialise the buzzer pin
const int buzzer_pin = D5;
bool tempAlarmSignaled = false;

//initialise the temperature and humidity pin
const int temp_hum_pin = D6;
bool humAlarmSignaled = false;

//initialise the DHT11 component
DHT dht(temp_hum_pin, DHT11);

int minMeasurableTemp = 0;
int maxMeasurableTemp = 60;
int minMeasurableHum = 5; 
int maxMeasurableHum = 95;

int temperature = 0;
int humidity = 0;

int display_screen = 0;
const String welcome_message_line1 = "Welcome to the";
const String welcome_message_line2 = "Smart Pantry.";
const String welcome_message_line3 = "Put this sensor";
const String welcome_message_line4 = "module where...";
const String welcome_message_line5 = "you store your ";
const String welcome_message_line6 = "perishalbes.";
const String setLowTempMessage1 = "Set low temp:";
const String setHighTempMessage1 = "Set high temp:";
const String setLowHumMessage1 = "Set low humid:";
const String setHighHumMessage1 = "Set high humid:";
const String save_parameters_message_line1 = "Go to monitor";
const String save_parameters_message_line2 = "mode to save.";

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // put your setup code here, to run once:
  pinMode(switch_pin, INPUT);
  pinMode(potentiometer_pin, INPUT);
  pinMode(led_pin, OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(push_button_pin, INPUT);
  pinMode(pir_sensor_pin, INPUT);
  digitalWrite(buzzer_pin, LOW);
  
  Serial.begin(9600);
  delay(2000);

  //initialise the fileSystem
  LittleFS.begin();
  String parameterConfigsStr = loadParameterConfigsFromFile();
  if(parameterConfigsStr == ""){
    //if no parameterConfigFile then initialise the ranges as wide as possible
    JsonObject temperatureParams = parameterConfigs.createNestedObject("temperature");
    temperatureParams["lowParameter"] = minMeasurableTemp;
    temperatureParams["highParameter"] = maxMeasurableTemp;
    temperatureParams["outOfRangeEvents"] = 0;
    JsonObject humidityParams = parameterConfigs.createNestedObject("humidity");
    humidityParams["lowParameter"] = minMeasurableHum;
    humidityParams["highParameter"] = maxMeasurableHum;
    humidityParams["outOfRangeEvents"] = 0;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.println("Waiting to connect...");
  }

  // Print the board IP address
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", get_index);
  server.on("/setBuzzerStatus", setBuzzerStatus);
  server.on("/json/sensors", sendSensorJsonData);

  server.begin();
  Serial.println("Server listening...");

  //start the dht component reading
  dht.begin();

  lcd.init();
  lcd.backlight();

  displayBootWelcomeMessage();
}

void loop() {
  //handle incoming http requests
  server.handleClient();

  // This keeps the server and serial monitor available 
  Serial.println("Server is running");

  pir_sensor_value = digitalRead(pir_sensor_pin);
  if(pir_sensor_value == 1){
    lcd.display();
    lcd.backlight();
  } else {
    lcd.noDisplay();
    lcd.noBacklight();
  }

  //always read the current value of the potentiometer    
  poteValue = analogRead(potentiometer_pin);

  //is the position switch set to Set Mode or Monitor Mode
  if(!isSetModeActive()){
    //if in monitor mode
    //read the temperature and humidity into their global variables
    readTempHum();

    //trigger the buzzer if either parameters are out of their specified range
    trigBuzzerWhenOutsideSpecifiedRange();
    
    //chose which display to show by the potentiometer position
    int displayValue = map(poteValue, 0, 1023, 0, 2);
    //if the display to show is different from the last loop, clear the LCD screen and set the new value of the display
    if(displayValue != display_screen){
      lcd.clear();
      display_screen = displayValue;
    }
    //display the current measurement values
    if(displayValue == 0){
      displayData();  
    //else display the current set parameters
    } else if (displayValue == 1){
      displayParameterValues();
    } else {
      displayWebserverIpAddress();
    }
  //if monitor mode is chosen by the switch position
  } else {
    //depending on how many times the push button is pressed cycle through the parameter settings
    if(push_button_count_value == 0){
      setLowTempParameter();
      displaySetLowTempParameterMessage(setLowTempMessage1);
    } else if(push_button_count_value == 1){
      setHighTempParameter();
      displaySetHighTempParameterMessage(setHighTempMessage1);
    } else if(push_button_count_value == 2){
      setLowHumParameter();
      displaySetLowHumParameterMessage(setLowHumMessage1);
    } else if(push_button_count_value == 3) {
      setHighHumParameter();
      displaySetHighHumParameterMessage(setHighHumMessage1);
    } else {
      displaySaveParametersMessage();
    }

    int prevPushButtonValue = push_button_value;
    if(isPushButtonPressed()){
      if(prevPushButtonValue != push_button_value && push_button_value == 1){
        lcd.clear();
        if(push_button_count_value == 4){
          push_button_count_value = 0;
        } else {
          push_button_count_value += 1;
        }
      }
    }
  }
}

bool isSetModeActive(){
  bool prevSwitchValue = switch_value;
  switch_value = digitalRead(switch_pin);
  if(prevSwitchValue != switch_value){
    lcd.clear();
    push_button_count_value = 0;
    if(switch_value == 0){
      if(writeParameterConfigsTofile()){
        Serial.print("successfully wrote to file");
      }  
    }
  }
  if (switch_value == 0){
    return false;
  }
  return true;
}

bool isPushButtonPressed(){
  push_button_value = digitalRead(push_button_pin);
  if (push_button_value == 1){
    return true;
  }
  return false;
}

void get_index(){
  String html = "<!DOCTYPE html> <html>";
  html += "<head><meta http_equiv=\"refresh\" content=\"2\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head>";
  html += "<body> <h1>The Smart Pantry Sensor Module Dashboard</h1>";
  html += "<p>Welcome to the smart pantry dashboard</p>";
  html += "<div> <p>The temperature reading is: <strong>";
  html += temperature;
  html += "</strong> C. ";
  if(isTempWithinSpecifiedRange()){
    html += "Which is within the specified range.";
  } else {
    html += "Which is <strong>outside</strong> the specified range.";
  }
  html += "</p></div>";
  html += "<div> <p>The humidity reading is: <strong>";
  html += humidity;
  html += "</strong> % ";
  if(isHumWithinSpecifiedRange()){
    html += "Which is within the specified range.";
  } else {
    html += "Which is <strong>outside</strong> the specified range.";
  }
  html += "</p></div>";
  html += "<div><p>";
  html += "You have had <strong>";
  html += parameterConfigs["temperature"]["outOfRangeEvents"].as<String>();
  html += "</strong> out of range events from the temperature sensor.";
  html += "</p></div>";
  html += "<div><p>";
  html += "You have had <strong>";
  html += parameterConfigs["humidity"]["outOfRangeEvents"].as<String>();
  html += "</strong> out of range events from the humidity sensor.";
  html += "</p></div>";
  html += "<div><p>Test the warning buzzer:</p></div>";
  html += "<a href=\"/setBuzzerStatus?s=0\" target=\"_self\"\"\"><button>Turn Off </button></a>";
  html += "<a href=\"/setBuzzerStatus?s=1\" target=\"_self\"\"\"><button>Turn On </button></a>";
  html += "</body> </html>";
  
  server.send(200, "text/html", html);
}

void setBuzzerStatus(){
  int query_string = 0;
  if (server.arg("s") != ""){
    //parse the value from the query
    query_string = server.arg("s").toInt();
    // check the value and update the Buzzer component
    if (query_string == 1){
      tone(buzzer_pin, 1000);
      delay(1000);
    } else {
      noTone(buzzer_pin);
      delay(100);
    }
  }
  get_index();
}

void readTempHum(){
  //initialise a temporary variable, used to eliminate occasional false reads from our sensors.
  int temporary = 0;
  temporary = dht.readTemperature();
  // this is the range of the DHT11 sensor. 
  if(temporary >= minMeasurableTemp && temporary <= maxMeasurableTemp){
    temperature = temporary;
  }
  temporary = dht.readHumidity();
  if (temporary >= minMeasurableHum && temporary <= maxMeasurableHum){
    humidity = temporary;
  }  
}

void setLowTempParameter(){
  int prevLowTempParameter = parameterConfigs["temperature"]["lowParameter"];
  parameterConfigs["temperature"]["lowParameter"] = map(poteValue, 1023, 0, minMeasurableTemp, parameterConfigs["temperature"]["highParameter"]);
  if (parameterConfigs["temperature"]["lowParameter"] != prevLowTempParameter){
    lcd.clear();
  }
}

void setHighTempParameter(){
  int prevHighTempParameter = parameterConfigs["temperature"]["highParameter"];
  parameterConfigs["temperature"]["highParameter"] = map(poteValue, 1023, 0, parameterConfigs["temperature"]["lowParameter"], maxMeasurableTemp);
  if (parameterConfigs["temperature"]["highParameter"] != prevHighTempParameter){
    lcd.clear();
  }
}

void setLowHumParameter(){
  int prevLowHumParameter = parameterConfigs["humidity"]["lowParameter"];
  parameterConfigs["humidity"]["lowParameter"] = map(poteValue, 1023, 0, minMeasurableHum, parameterConfigs["humidity"]["highParameter"]);
  if (parameterConfigs["humidity"]["lowParameter"] != prevLowHumParameter){
    lcd.clear();
  }
}

void setHighHumParameter(){
  int prevHighHumParameter = parameterConfigs["humidity"]["highParameter"];
  parameterConfigs["humidity"]["highParameter"] = map(poteValue, 1023, 0, parameterConfigs["humidity"]["lowParameter"], maxMeasurableHum);
  if (parameterConfigs["humidity"]["highParameter"] != prevHighHumParameter){
    lcd.clear();
  }
}

void displayData(){
  String tempMessage = "Temp: ";
  tempMessage += temperature;
  tempMessage += " C";

  String humMessage = "Hum: ";
  humMessage += humidity;
  humMessage += " %";

  lcd.setCursor(0,0);
  lcd.print(tempMessage);
  lcd.setCursor(0,1);
  lcd.print(humMessage);
}

void displayWebserverIpAddress(){
  String ipAddress = WiFi.localIP().toString();
  lcd.setCursor(0,0);
  lcd.print("IP Address:");
  lcd.setCursor(0,1);
  lcd.print(ipAddress);
}

void displayParameterValues(){
  lcd.setCursor(0,0);
  lcd.print("Temp: ");
  lcd.print(parameterConfigs["temperature"]["lowParameter"].as<String>());
  lcd.print(" - ");
  lcd.print(parameterConfigs["temperature"]["highParameter"].as<String>());
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Hum: ");
  lcd.print(parameterConfigs["humidity"]["lowParameter"].as<String>());
  lcd.print(" - ");
  lcd.print(parameterConfigs["humidity"]["highParameter"].as<String>());
  lcd.print("%");
}

void displaySetLowTempParameterMessage(String message){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(parameterConfigs["temperature"]["lowParameter"].as<String>());
  lcd.print("C");
}

void displaySetHighTempParameterMessage(String message){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(parameterConfigs["temperature"]["highParameter"].as<String>());
  lcd.print("C");
}

void displaySetLowHumParameterMessage(String message){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(parameterConfigs["humidity"]["lowParameter"].as<String>());
  lcd.print("%");
}

void displaySetHighHumParameterMessage(String message){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(parameterConfigs["humidity"]["highParameter"].as<String>());
  lcd.print("%");
}


void displaySaveParametersMessage(){
  lcd.setCursor(0,0);
  lcd.print(save_parameters_message_line1);
  lcd.setCursor(0,1);
  lcd.print(save_parameters_message_line2);
}

void displayBootWelcomeMessage(){
  lcd.setCursor(0,0);
  lcd.print(welcome_message_line1);  
  lcd.setCursor(0,1);
  lcd.print(welcome_message_line2); 
  delay(5000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(welcome_message_line3);
  lcd.setCursor(0,1);
  lcd.print(welcome_message_line4);
  delay(5000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(welcome_message_line5);
  lcd.setCursor(0,1);
  lcd.print(welcome_message_line6);
  delay(5000);
  lcd.clear();
}

bool isTempWithinSpecifiedRange(){
  if(temperature > parameterConfigs["temperature"]["highParameter"] || temperature < parameterConfigs["temperature"]["lowParameter"]){
    return false;
  } else {
    return true;
  }
}

bool isHumWithinSpecifiedRange(){
  if(humidity > parameterConfigs["humidity"]["highParameter"] || humidity < parameterConfigs["humidity"]["lowParameter"]){
    return false;
  } else {
    return true;
  }
}

void trigBuzzerWhenOutsideSpecifiedRange(){
  if(!isTempWithinSpecifiedRange() && !tempAlarmSignaled){
    soundWarningBuzzer();
    parameterConfigs["temperature"]["outOfRangeEvents"] = parameterConfigs["temperature"]["outOfRangeEvents"].as<int>() + 1;
    tempAlarmSignaled = true;
  }
  if(tempAlarmSignaled && isTempWithinSpecifiedRange()){
    tempAlarmSignaled = false;
  }
  if(!isHumWithinSpecifiedRange() && !humAlarmSignaled){
    soundWarningBuzzer();
    parameterConfigs["humidity"]["outOfRangeEvents"] = parameterConfigs["humidity"]["outOfRangeEvents"].as<int>() + 1; 
    humAlarmSignaled = true;
  }
  if(humAlarmSignaled && isHumWithinSpecifiedRange()){
    humAlarmSignaled = false;
  }
}

void soundWarningBuzzer(){
  tone(buzzer_pin, 1000);  
  delay(2000);
  noTone(buzzer_pin);
}

void sendSensorJsonData(){
  // add JSON request data
  doc.clear();
  doc["content-type"] = "application/json";
  doc["status"] = 200;
  
  JsonObject tempDHT11 = doc.createNestedObject("temperature sensor");
  tempDHT11["sensorName"] = "DHT11";
  tempDHT11["sensorValue"] = temperature;
  tempDHT11["inRange"] = isTempWithinSpecifiedRange();
  tempDHT11["lowParameter"] = parameterConfigs["temperature"]["lowParameter"];
  tempDHT11["highParameter"] = parameterConfigs["temperature"]["highParameter"];
  tempDHT11["outOfRangeEvents"] = parameterConfigs["temperature"]["outOfRangeEvents"];
  JsonObject humDHT11 = doc.createNestedObject("humidity sensor");
  humDHT11["sensorName"] = "DHT11";
  humDHT11["sensorValue"] = humidity;
  humDHT11["inRange"] = isHumWithinSpecifiedRange();
  humDHT11["lowParameter"] = parameterConfigs["humidity"]["lowParameter"];
  humDHT11["highParameter"] = parameterConfigs["humidity"]["highParameter"];
  humDHT11["outOfRangeEvents"] = parameterConfigs["humidity"]["outOfRangeEvents"];

  String jsonStr;
  serializeJsonPretty(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
}

String loadParameterConfigsFromFile() {
  String result = "";
  File configsFile = LittleFS.open(parameter_configs_filename, "r");
  if (!configsFile) { 
    // failed the read operation return blank result
    Serial.print("Could not read " + parameter_configs_filename);
    return result;
  }
  while (configsFile.available()) {
    result += (char)configsFile.read();
  }
  configsFile.close();
  Serial.println("Got " + result + " from " + parameter_configs_filename);
  buildParametersFromJsonString(result);
  return result;
}

bool writeParameterConfigsTofile() {  
  File configsFile = LittleFS.open(parameter_configs_filename, "w");
  if (!configsFile) { 
    // failed to open the configsFile for writing
    Serial.println("writeParameterConfigsTofile:: Could not open parameter_configs_file to write");
    return false;
  }

  String configsStr;
  serializeJson(parameterConfigs, configsStr);
  int bytesWritten = configsFile.print(configsStr);
  if (bytesWritten == 0) { 
    // write operation failed on configsFile
    Serial.println("writeParameterConfigsTofile:: Could not write to parameter_configs_file");
    return false;
  }
  Serial.println("writen....");
  Serial.println(configsStr);    
  configsFile.close();
  return true;
}

void buildParametersFromJsonString(String jsonString){
  deserializeJson(parameterConfigs, jsonString);
}

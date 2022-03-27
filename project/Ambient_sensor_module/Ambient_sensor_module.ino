
////////////////////////// External libraries ////////////////////////////////////////////
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

////////////////////////// Initial values, global variables and sensor declarations //////

// instantiate and set the port for the web server
ESP8266WebServer server(80);

// set wifi credentials - these would need to be changed for the locally available network
const char* ssid = "FRITZ!Box 7590 XO";
const char* password = "96087252974805885212";

//Filename for the file that stores the long term data
const String parameter_configs_filename = "/parameterConfigs.txt";

//allocate the JSON document for responding to /json/pantry requests
DynamicJsonDocument doc(1024);
//allocate the JSON document for keeping track of the high, low, and out or range events
//for humidity and temperature. This JSON doc is written to and read from the Filesystem 
//to maintain data between power offs.
StaticJsonDocument<256> parameterConfigs;

//initialise the LCD screen
LiquidCrystal_I2C lcd(0x27, 16, 2);

//allocate the pins to the respective devices
const int potentiometer_pin = A0;
const int switch_pin = D0;
const int buzzer_pin = D5;
const int temp_hum_pin = D6;
const int pir_sensor_pin = D7;
const int push_button_pin = D8;

//initialise the DHT11 temperature and humidity sensor and its measurable ranges
DHT dht(temp_hum_pin, DHT11);
int minMeasurableTemp = 0;
int maxMeasurableTemp = 60;
int minMeasurableHum = 5; 
int maxMeasurableHum = 95;

//declare the global variables used to keep track of values from sensors and physical devices
int switch_value;
int pir_sensor_value;
int push_button_value;
int poteValue;
int push_button_count_value = 0;
int temperature = 0;
int humidity = 0;

//keep track of alarm signal states
bool tempAlarmSignaled = false;
bool humAlarmSignaled = false;

//variable used to keep track of which screen to display on the LCD
int display_screen = 0;

//message definitions for the various LCD screens
const String welcome_message_line1 = "Welcome to the";
const String welcome_message_line2 = "Smart Pantry.";
const String welcome_message_line3 = "Put this sensor";
const String welcome_message_line4 = "module where...";
const String welcome_message_line5 = "you store your ";
const String welcome_message_line6 = "perishables.";
const String setLowTempMessage1 = "Set low temp:";
const String setHighTempMessage1 = "Set high temp:";
const String setLowHumMessage1 = "Set low humid:";
const String setHighHumMessage1 = "Set high humid:";
const String save_parameters_message_line1 = "Go to monitor";
const String save_parameters_message_line2 = "mode to save.";


////////////////////////// Setup function ////////////////////////////////////////////
void setup() {
  //assign the pin modes to the respective device pins
  pinMode(switch_pin, INPUT);
  pinMode(potentiometer_pin, INPUT);
  pinMode(led_pin, OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(push_button_pin, INPUT);
  pinMode(pir_sensor_pin, INPUT);
  //ensure the buzzer is off
  digitalWrite(buzzer_pin, LOW);

  //initialise Serial communication
  Serial.begin(9600);
  delay(2000);

  //initialise the fileSystem
  LittleFS.begin();

  //read in from long-term memory the values for acceptable temperature and humidity ranges and
  //number of events encountered outside those ranges0
  String parameterConfigsStr = loadParameterConfigsFromFile();
  //if there is no existing file or a read problem was encountered initialise the ranges to the widest possible
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

  //initialise WiFi communication and attempt to connect to the local WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.println("Waiting to connect...");
  }

  // Print the board IP address to the Serial
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //define the handler functions for available server routes
  server.on("/", get_index);
  server.on("/setBuzzerStatus", setBuzzerStatus);
  server.on("/json/sensors", sendSensorJsonData);

  //initialise the server and print message to serial
  server.begin();
  Serial.println("Server listening...");

  //start the dht component reading
  dht.begin();

  //activate the LCD screen
  lcd.init();
  lcd.backlight();

  //display the welcome message to the user on the LCD
  displayBootWelcomeMessage();
}
////////////////////////// End of setup function ////////////////////////////////////////////

////////////////////////// Loop function ////////////////////////////////////////////////////
void loop() {
  //handle incoming http requests
  server.handleClient();

  //This keeps the server and serial monitor available 
  Serial.println("Server is running");

  //read the pir sensor value and turn on/off the LCD backlight and display accordingly
  //this reduces battery use by turning on the display only when movement nearby is sensed 
  pir_sensor_value = digitalRead(pir_sensor_pin);
  if(pir_sensor_value == 1){
    lcd.display();
    lcd.backlight();
  } else {
    lcd.noDisplay();
    lcd.noBacklight();
  }

  //read the current value of the potentiometer into its global variable
  poteValue = analogRead(potentiometer_pin);

  //is the position switch set to Set Mode or Monitor Mode
  if(!isSetModeActive()){
    //if in monitor mode
    //read the temperature and humidity into their global variables and record events
    readTempHum();
    //respond to any out of range values
    triggerOutsideSpecifiedRangeEvents();
    
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

    //check for button press events, just one press is registered per button position down-up
    //this prevents more than one button press event being detected per physical press of a button
    int prevPushButtonValue = push_button_value;
    //is the button currently down
    if(isPushButtonPressed()){
      //was the button not down in the last loop and is down now
      if(prevPushButtonValue != push_button_value && push_button_value == 1){
        //clear the lcd
        lcd.clear();
        //loop the count of the push button back to zero to enable screen cycling
        if(push_button_count_value == 4){
          push_button_count_value = 0;
        } else {
          push_button_count_value += 1;
        }
      }
    }
  }
}
////////////////////////// End of loop function /////////////////////////////////////////

//read the temperature and humidity values from the sensor into their respective global variables
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

////////////////////////// Mode and button functions ////////////////////////////////////

//decide which mode to display on the LCD (monitor mode/set mode) depending on the position of the switch
bool isSetModeActive(){
  bool prevSwitchValue = switch_value;
  switch_value = digitalRead(switch_pin);
  if(prevSwitchValue != switch_value){
    //clear the lcd screen when a switch event is registered
    lcd.clear();
    //reset this variable to return to the first screen next time the switch is positioned in Set Mode
    push_button_count_value = 0;
    if(switch_value == 0){
      //When switching from Set Mode to Monitor Mode save the new range parameters to file.
      if(writeParameterConfigsTofile()){
        Serial.print("successfully wrote parameterConfigs to file");
      }  
    }
  }
  //return bool for isSetModeActive by the position of the switch
  if (switch_value == 0){
    return false;
  }
  return true;
}

//check if the push button is currently being depressed
bool isPushButtonPressed(){
  push_button_value = digitalRead(push_button_pin);
  if (push_button_value == 1){
    return true;
  }
  return false;
}
////////////////////////// End of mode and button functions ///////////////////////

///////////////////////////////// Parameter threshold functions ///////////////////////////

//set the low temperature threshold via the potentiometer value
//possible set range is between the minimum measurable and the high temperature threshold
void setLowTempParameter(){
  int prevLowTempParameter = parameterConfigs["temperature"]["lowParameter"];
  parameterConfigs["temperature"]["lowParameter"] = map(poteValue, 1023, 0, minMeasurableTemp, parameterConfigs["temperature"]["highParameter"]);
  if (parameterConfigs["temperature"]["lowParameter"] != prevLowTempParameter){
    lcd.clear();
  }
}

//set the high temperature threshold via the potentiometer value
//possible set range is between the low temperature threshold and the maximum measurable
void setHighTempParameter(){
  int prevHighTempParameter = parameterConfigs["temperature"]["highParameter"];
  parameterConfigs["temperature"]["highParameter"] = map(poteValue, 1023, 0, parameterConfigs["temperature"]["lowParameter"], maxMeasurableTemp);
  if (parameterConfigs["temperature"]["highParameter"] != prevHighTempParameter){
    lcd.clear();
  }
}

//set the low humidity threshold via the potentiometer value
//possible set range is between the minimum measurable and the high humidity threshold
void setLowHumParameter(){
  int prevLowHumParameter = parameterConfigs["humidity"]["lowParameter"];
  parameterConfigs["humidity"]["lowParameter"] = map(poteValue, 1023, 0, minMeasurableHum, parameterConfigs["humidity"]["highParameter"]);
  if (parameterConfigs["humidity"]["lowParameter"] != prevLowHumParameter){
    lcd.clear();
  }
}

//set the high humidity threshold via the potentiometer value
//possible set range is between the low humidity threshold and the maximum measurable
void setHighHumParameter(){
  int prevHighHumParameter = parameterConfigs["humidity"]["highParameter"];
  parameterConfigs["humidity"]["highParameter"] = map(poteValue, 1023, 0, parameterConfigs["humidity"]["lowParameter"], maxMeasurableHum);
  if (parameterConfigs["humidity"]["highParameter"] != prevHighHumParameter){
    lcd.clear();
  }
}

//check if the current temperature value is within the specified range 
//returns true if in range, false if out of range
bool isTempWithinSpecifiedRange(){
  if(temperature > parameterConfigs["temperature"]["highParameter"] || temperature < parameterConfigs["temperature"]["lowParameter"]){
    return false;
  } else {
    return true;
  }
}

//check if the current humidity value is within the specified range 
//returns true if in range, false if out of range
bool isHumWithinSpecifiedRange(){
  if(humidity > parameterConfigs["humidity"]["highParameter"] || humidity < parameterConfigs["humidity"]["lowParameter"]){
    return false;
  } else {
    return true;
  }
}

//trigger the buzzer if either parameters are out of their specified range, record an "outOfRangeEvent"
//and adjust the 'signaled' boolean to prevent further alarms for this event
void triggerOutsideSpecifiedRangeEvents(){
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

//sound the warning sound on the buzzer
void soundWarningBuzzer(){
  tone(buzzer_pin, 1000);  
  delay(2000);
  noTone(buzzer_pin);
}
/////////////////////////////// End of parameter threshold functions ////////////////////////

////////////////////////////////////// LCD Display functions ///////////////////////////////
//display the current values for temperature and humidity as measured
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

//display the IP address of the module for reaching the web server
void displayWebserverIpAddress(){
  String ipAddress = WiFi.localIP().toString();
  lcd.setCursor(0,0);
  lcd.print("IP Address:");
  lcd.setCursor(0,1);
  lcd.print(ipAddress);
}

//display the current acceptable ranges for temperature and humidity
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

//display the screen to set the low temperature threshold
void displaySetLowTempParameterMessage(String message){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(parameterConfigs["temperature"]["lowParameter"].as<String>());
  lcd.print("C");
}

//display the screen to set the high temperature threshold
void displaySetHighTempParameterMessage(String message){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(parameterConfigs["temperature"]["highParameter"].as<String>());
  lcd.print("C");
}

//display the screen to set the low humidity threshold
void displaySetLowHumParameterMessage(String message){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(parameterConfigs["humidity"]["lowParameter"].as<String>());
  lcd.print("%");
}

//display the screen to set the high humidity threshold
void displaySetHighHumParameterMessage(String message){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(parameterConfigs["humidity"]["highParameter"].as<String>());
  lcd.print("%");
}

//display the screen to prompt user to save their set parameter ranges for temperature and humidity
void displaySaveParametersMessage(){
  lcd.setCursor(0,0);
  lcd.print(save_parameters_message_line1);
  lcd.setCursor(0,1);
  lcd.print(save_parameters_message_line2);
}

//display the welcome message - called in setup function 
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
////////////////////////////// End of LCD Display functions ///////////////////////////////

////////////////////////////////////// Filesystem functions ///////////////////////////////

//load the parameter config data into the JSON object from the filesystem,
//on success returns file contents as string
//on fail returns blank string
String loadParameterConfigsFromFile() {
  String result = "";
  //opent the file
  File configsFile = LittleFS.open(parameter_configs_filename, "r");
  if (!configsFile) { 
    // failed the read operation return blank result
    Serial.print("Could not read " + parameter_configs_filename);
    return result;
  }
  
  //read the contents of the file into the local variable
  while (configsFile.available()) {
    result += (char)configsFile.read();
  }

  //close the file
  configsFile.close();
  //print success message to Serial
  Serial.println("Got " + result + " from " + parameter_configs_filename);
  //build the JSON object from the file contents
  buildParametersFromJsonString(result);
  //return the file contents (as string)
  return result;
}

//write the parameter config data from the JSON object to the filesystem,
bool writeParameterConfigsTofile() {  
  //open the file for writing
  File configsFile = LittleFS.open(parameter_configs_filename, "w");
  if (!configsFile) { 
    // failed to open the configsFile for writing
    Serial.println("writeParameterConfigsTofile:: Could not open parameter_configs_file to write");
    return false;
  }

  //serialize the JSON object
  String configsStr;
  serializeJson(parameterConfigs, configsStr);
  //write the serialized JSON string into the file
  int bytesWritten = configsFile.print(configsStr);
  if (bytesWritten == 0) { 
    // write operation failed on configsFile
    Serial.println("writeParameterConfigsTofile:: Could not write to parameter_configs_file");
    return false;
  }

  //close the file when done
  configsFile.close();
  
  //print the success message to Serial
  Serial.println("writen....");
  Serial.println(configsStr);
  Serial.println("to file");
  
  return true;
}

void buildParametersFromJsonString(String jsonString){
  deserializeJson(parameterConfigs, jsonString);
}
////////////////////////////////////// End of filesystem functions /////////////////////

////////////////////////////////////// Web server routes ///////////////////////////////

//respond to requests for the main dashboard
void get_index(){
  String html = "<!DOCTYPE html> <html>";
  html += "<head><meta http_equiv=\"refresh\" content=\"2\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head>";
  html += "<body> <h1>The Smart Pantry Sensor Module Dashboard</h1>";
  html += "<p>Welcome to the smart pantry dashboard</p>";
  //display the temperature and humidity readings and if they are within specified ranges
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
  //display the number of out of range events occurred
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
  //provide buttons to test the warning buzzer on the module
  html += "<div><p>Test the warning buzzer:</p></div>";
  html += "<a href=\"/setBuzzerStatus?s=0\" target=\"_self\"\"\"><button>Turn Off </button></a>";
  html += "<a href=\"/setBuzzerStatus?s=1\" target=\"_self\"\"\"><button>Turn On </button></a>";
  html += "</body> </html>";

  //send the response
  server.send(200, "text/html", html);
}

void sendSensorJsonData(){
  // add JSON request data
  doc.clear();
  doc["content-type"] = "application/json";
  doc["status"] = 200;

  //build the temperature sensor data object
  JsonObject tempDHT11 = doc.createNestedObject("temperature sensor");
  tempDHT11["sensorName"] = "DHT11";
  tempDHT11["sensorValue"] = temperature;
  tempDHT11["inRange"] = isTempWithinSpecifiedRange();
  tempDHT11["lowParameter"] = parameterConfigs["temperature"]["lowParameter"];
  tempDHT11["highParameter"] = parameterConfigs["temperature"]["highParameter"];
  tempDHT11["outOfRangeEvents"] = parameterConfigs["temperature"]["outOfRangeEvents"];
  //build the humidity sensor data object
  JsonObject humDHT11 = doc.createNestedObject("humidity sensor");
  humDHT11["sensorName"] = "DHT11";
  humDHT11["sensorValue"] = humidity;
  humDHT11["inRange"] = isHumWithinSpecifiedRange();
  humDHT11["lowParameter"] = parameterConfigs["humidity"]["lowParameter"];
  humDHT11["highParameter"] = parameterConfigs["humidity"]["highParameter"];
  humDHT11["outOfRangeEvents"] = parameterConfigs["humidity"]["outOfRangeEvents"];

  //serialize the string to prettified JSON 
  String jsonStr;
  serializeJsonPretty(doc, jsonStr);

  //send the response
  server.send(200, "application/json", jsonStr);
}

//set the buzzer on or off in response to web dashboard request
//return the index page
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
////////////////////////////////////// End of web server routes /////////////////////

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// set the port for the web server
ESP8266WebServer server(80);
const char* ssid = "FRITZ!Box 7590 XO";
const char* password = "96087252974805885212";

//allocate the JSON document
//allows us to allocate memory to the document dynamically.
DynamicJsonDocument doc(1024);

const int switch_pin = D0;
int switch_value;

const int led_pin = D2;
const int pir_sensor_pin = D8;
int pir_sensor_value;
const int push_button_pin = D7;
int push_button_value;
int push_button_count_value = 0;

const int potentiometer_pin = A0;
int poteValue;

//initialise the buzzer pin
const int buzzer_pin = D5;
bool tempAlarmSignaled = false;
int countTempAlarm = 0;

//initialise the temperature and humidity pin
const int temp_hum_pin = D6;
bool humAlarmSignaled = false;
int countHumAlarm = 0;

//initialise the DHT11 component
DHT dht(temp_hum_pin, DHT11);

int minMeasurableTemp = 0;
int maxMeasurableTemp = 60;
int minMeasurableHum = 5; 
int maxMeasurableHum = 95;

//the low and high temperature thresholds set by the user (initialised to measurable ranges)
int lowSetTemp = minMeasurableTemp;
int highSetTemp = maxMeasurableTemp;
//the low and high humidity thresholds set by the user (initialised to measurable ranges)
int lowSetHum = minMeasurableHum;
int highSetHum = maxMeasurableHum;

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

  WiFi.begin(ssid, password);
  Serial.begin(9600);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.println("Waiting to connect...");
  }

  // Print the board IP address
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", get_index);
  server.on("/setBuzzerStatus", setBuzzerStatus);
  server.on("/json/sensors", getSensorJsonData);

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
      setLowSetTemp();
      displaySetParameterMessage(setLowTempMessage1, &lowSetTemp, "C");
    } else if(push_button_count_value == 1){
      setHighSetTemp();
      displaySetParameterMessage(setHighTempMessage1, &highSetTemp, "C");
    } else if(push_button_count_value == 2){
      setLowSetHum();
      displaySetParameterMessage(setLowHumMessage1, &lowSetHum, "%");
    } else if(push_button_count_value == 3) {
      setHighSetHum();
      displaySetParameterMessage(setHighHumMessage1, &highSetHum, "%");
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
//  Serial.print("temperature: ");
//  Serial.print(temperature);
//  Serial.print("\t");
//  Serial.print("humidity: ");
//  Serial.print(humidity);
//  Serial.print("\n");
}

void setLowSetTemp(){
  int prevLowSetTemp = lowSetTemp;
  lowSetTemp = map(poteValue, 1023, 0, minMeasurableTemp, highSetTemp);
  if (lowSetTemp != prevLowSetTemp){
    lcd.clear();
  }
}

void setHighSetTemp(){
  int prevHighSetTemp = highSetTemp;
  highSetTemp = map(poteValue, 1023, 0, lowSetTemp, maxMeasurableTemp);
  if (highSetTemp != prevHighSetTemp){
    lcd.clear();
  }
}

void setLowSetHum(){
  int prevLowSetHum = lowSetHum;
  lowSetHum = map(poteValue, 1023, 0, minMeasurableHum, highSetHum);
  if (lowSetHum != prevLowSetHum){
    lcd.clear();
  }
}

void setHighSetHum(){
  int prevHighSetHum = highSetHum;
  highSetHum = map(poteValue, 1023, 0, lowSetHum, maxMeasurableHum);
  if (highSetHum != prevHighSetHum){
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
  lcd.print(humMessage );
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
  lcd.print(lowSetTemp);
  lcd.print(" - ");
  lcd.print(highSetTemp);
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Hum: ");
  lcd.print(lowSetHum);
  lcd.print(" - ");
  lcd.print(highSetHum);
  lcd.print("%");
}

void displaySetParameterMessage(String message, int* valueToSet, String unit){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(*valueToSet);
  lcd.print(unit);
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
  if(temperature > highSetTemp || temperature < lowSetTemp){
    return false;
  } else {
    return true;
  }
}

bool isHumWithinSpecifiedRange(){
  if(humidity > highSetHum || humidity < lowSetHum){
    return false;
  } else {
    return true;
  }
}

void trigBuzzerWhenOutsideSpecifiedRange(){
  if(!isTempWithinSpecifiedRange() && !tempAlarmSignaled){
    soundWarningBuzzer();
    countTempAlarm++;
    tempAlarmSignaled = true;
  }
  if(tempAlarmSignaled && isTempWithinSpecifiedRange()){
    tempAlarmSignaled = false;
  }
  if(!isHumWithinSpecifiedRange() && !humAlarmSignaled){
    soundWarningBuzzer();
    countHumAlarm++;
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

void getSensorJsonData(){
  // add JSON request data
  doc.clear();
  doc["content-type"] = "application/json";
  doc["status"] = 200;
  
  JsonObject tempDHT11 = doc.createNestedObject("temperature sensor");
  tempDHT11["sensorName"] = "DHT11";
  tempDHT11["sensorValue"] = temperature;
  tempDHT11["inRange"] = isTempWithinSpecifiedRange();
  tempDHT11["lowParameter"] = lowSetTemp;
  tempDHT11["highParameter"] = highSetTemp;
  tempDHT11["outOfRangeEvents"] = countTempAlarm;
  JsonObject humDHT11 = doc.createNestedObject("humidity sensor");
  humDHT11["sensorName"] = "DHT11";
  humDHT11["sensorValue"] = humidity;
  humDHT11["inRange"] = isHumWithinSpecifiedRange();
  humDHT11["lowParameter"] = lowSetHum;
  humDHT11["highParameter"] = highSetHum;
  humDHT11["outOfRangeEvents"] = countHumAlarm;

  String jsonStr;
  serializeJsonPretty(doc, jsonStr);
  server.send(200, "application/json", jsonStr);
}

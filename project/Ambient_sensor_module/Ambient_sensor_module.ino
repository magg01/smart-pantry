#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>

// set the port for the web server
ESP8266WebServer server(80);
const char* ssid = "FRITZ!Box 7590 XO";
const char* password = "96087252974805885212";

const int switch_pin = D0;
int switch_value;

const int led_pin = D2;
const int push_button_pin = D7;
int push_button_value;
int push_button_count_value = 0;

const int potentiometer_pin = A0;
int poteValue;

//initialise the buzzer pin
const int buzzer_pin = D5;
bool signaled = false;

//initialise the temperature and humidity pin
const int temp_hum_pin = D6;

//initialise the DHT11 component
DHT dht(temp_hum_pin, DHT11);

int minMeasurableTemp = -20;
int maxMeasurableTemp = 60;
int minMeasurableHum = 0; 
int maxMeasurableHum = 100;

//the low and high temperature thresholds set by the user
int lowSetTemp;
int highSetTemp;
//the low and high humidity thresholds set by the user
int lowSetHum;
int highSetHum;

int minTemp = -2;
int maxTemp = 6;
int prefTemp = 3;

int temperature = 0;
int humidity = 0;

int displayScreen = 0;
const String welcomeMessageLine1 = "Welcome to the";
const String welcomeMessageLine2 = "Smart Fridge.";
const String reminderMessageLine1 = "Time to fill up";
const String reminderMessageLine2 = "the fridge.";
const String setLowTempMessage1 = "Set low temp:";
const String setHighTempMessage1 = "Set high temp:";
const String setLowHumMessage1 = "Set low humid:";
const String setHighHumMessage1 = "Set high humid:";

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  // put your setup code here, to run once:
  pinMode(switch_pin, INPUT);
  pinMode(potentiometer_pin, INPUT);
  pinMode(led_pin, OUTPUT);
  pinMode(buzzer_pin, OUTPUT);
  pinMode(push_button_pin, INPUT);
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

  server.begin();
  Serial.println("Server listening...");

  //start the dht component reading
  dht.begin();

  lcd.init();
  lcd.backlight();
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();

  // This keeps the server and serial monitor available 
  Serial.println("Server is running");

  if(!shouldSetValues()){   
    readTempHum();
    
    //Serial.println(fridgeTemperature());
    int temp = fridgeTemperature();

    trigBuzzerWhenAboveThreshold();
    
    poteValue = analogRead(potentiometer_pin);
    int displayValue = map(poteValue, 0, 1023, 0, 2);
    if(displayValue != displayScreen){
      lcd.clear();
      displayScreen = displayValue;
    }
    if(displayValue == 0){
      displayWelcomeMessage();
    } else if(displayValue == 1){
      displayData();  
    } else {
      if(signaled){
        displayReminder();
      }
      else {
        lcd.clear();
        lcd.print("LT: ");
        lcd.print(lowSetTemp);
        lcd.print("C, ");
        lcd.print("HT: ");
        lcd.print(highSetTemp);
        lcd.print("C");
        lcd.setCursor(0,1);
        lcd.print("LH: ");
        lcd.print(lowSetHum);
        lcd.print("%, ");
        lcd.print("HH: ");
        lcd.print(highSetHum);
        lcd.print("%");
      }
    }
  } else {
    poteValue = analogRead(potentiometer_pin);
    if(push_button_count_value == 0){
      int prevLowSetTemp = lowSetTemp;
      lowSetTemp = map(poteValue, 1023, 0, minMeasurableTemp, maxMeasurableTemp);
      if (lowSetTemp != prevLowSetTemp){
        lcd.clear();
      }
      displaySetParameterMessage(setLowTempMessage1, &lowSetTemp, "C");
    } else if(push_button_count_value == 1){
      int prevHighSetTemp = highSetTemp;
      highSetTemp = map(poteValue, 1023, 0, minMeasurableTemp, maxMeasurableTemp);
      if (highSetTemp != prevHighSetTemp){
        lcd.clear();
      }
      displaySetParameterMessage(setHighTempMessage1, &highSetTemp, "C");
    } else if(push_button_count_value == 2){
      int prevLowSetHum = lowSetHum;
      lowSetHum = map(poteValue, 1023, 0, minMeasurableHum, maxMeasurableHum);
      if (lowSetHum != prevLowSetHum){
        lcd.clear();
      }
      displaySetParameterMessage(setLowHumMessage1, &lowSetHum, "%");
    } else {
      int prevHighSetHum = highSetHum;
      highSetHum = map(poteValue, 1023, 0, minMeasurableHum, maxMeasurableHum);
      if (highSetHum != prevHighSetHum){
        lcd.clear();
      }
      displaySetParameterMessage(setHighHumMessage1, &highSetHum, "%");
    }

    if(isPushButtonPressed()){
      if(push_button_count_value == 4){
        push_button_count_value = 0;
      } else {
        push_button_count_value += 1;
      }
    }
    
//    digitalWrite(buzzer_pin, LOW);
//    poteValue = analogRead(potentiometer_pin);
//    int brightnessValue = map(poteValue, 0, 1023, 0, 255);
//    analogWrite(led_pin, brightnessValue);
    //Serial.print(poteValue + ", ");
    //Serial.println(brightnessValue);
  }
  delay(100);
}

bool shouldSetValues(){
  switch_value = digitalRead(switch_pin);
  if (switch_value == 0){
    return false;
  }
  return true;
}

bool isPushButtonPressed(){
  push_button_value = digitalRead(push_button_pin);
  if (push_button_value == 0){
    return false;
  }
  return true;
}

int fridgeTemperature(){
  poteValue = analogRead(potentiometer_pin);
  poteValue = map(poteValue, 0, 1023, minTemp, maxTemp);
  return poteValue;
}

void get_index(){
  String html = "<!DOCTYPE html> <html>";
  html += "<head><meta http_equiv=\"refresh\" content=\"2\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head>";
  html += "<body> <h1>The Smart Fridge Dashboard</h1>";
  html += "<p>Welcome to the smart fridge dashboard</p>";
  html += "<div><p><strong>The temperature preference is: ";
  html += prefTemp;
  html += " degrees.";
  html += "</strong></p>";
  html += "<div> <p> <strong> The temperature reading is: ";
  html += temperature;
  html += "</strong> degrees. </p>";
  html += "<div> <p> <strong> The humidity reading is: ";
  html += humidity;
  html += " % </strong> </p></div>";
  html += "<p> <strong>Buzzer component";
  html += "<a href=\"/setBuzzerStatus?s=0\" target=\"_blank\"\"\"><button>Turn Off </button></a>";
  html += "<a href=\"/setBuzzerStatus?s=1\" target=\"_blank\"\"\"><button>Turn On </button></a>";
  
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
}

void readTempHum(){
  //initialise a temporary variable, used to eliminate occasional false reads from our sensors.
  int temp = 0;
  temp = dht.readTemperature();
  // this is the range of the DHT11 sensor. 
  if(temp >= 0 && temp <=50){
    temperature = temp;
  }
  temp = dht.readHumidity();
  if (temp >= 0 && temp <= 100){
    humidity = temp;
  }  
  Serial.print("temperature: ");
  Serial.print(temperature);
  Serial.print("\t");
  Serial.print("humidity: ");
  Serial.print(humidity);
  Serial.print("\n");
}

void displayData(){
  String tmp = "Temp: ";
  tmp += temperature;
  tmp += " degrees";

  String hum = "Hum: ";
  hum += humidity;
  hum += " %";

  lcd.setCursor(0,0);
  lcd.print(tmp);
  lcd.setCursor(0,1);
  lcd.print(hum);
}

void countPushButtonPresses(){
  
}

void displayWelcomeMessage(){
  lcd.setCursor(0,0);
  lcd.print(welcomeMessageLine1);  
  lcd.setCursor(0,1);
  lcd.print(welcomeMessageLine2);  
}

void displayReminder(){
  lcd.setCursor(0,0);
  lcd.print(reminderMessageLine1);
  lcd.setCursor(0,1);
  lcd.print(reminderMessageLine2);
}

void displaySetLowTempMessage(){
  lcd.setCursor(0,0);
  lcd.print(setLowTempMessage1);
  lcd.setCursor(0,1);
  lcd.print(lowSetTemp);
  lcd.print("C");
}

void displaySetParameterMessage(String message, int* valueToSet, String unit){
  lcd.setCursor(0,0);
  lcd.print(message);
  lcd.setCursor(0,1);
  lcd.print(*valueToSet);
  lcd.print(unit);
}

void trigBuzzerWhenAboveThreshold(){
  if(temperature >= 30 && !signaled){
    tone(buzzer_pin, 1000);  
    delay(2000);
    noTone(buzzer_pin);
    signaled = true;
  }
  if(temperature < 30){
    signaled = false;
  }
}

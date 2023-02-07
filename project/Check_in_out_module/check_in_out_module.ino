////////////////////////// External libraries /////////////////////////////////////

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

/////////////////// Initial values, global variables and sensor declarations //////

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// instantiate and set the port for the web server
ESP8266WebServer server(80);

//define the IP address of the ambient sensor module for connecting to and reading the sensor data
//this would obviously be much better not hard-coded in as it is currenly very inflexible.
const char* host = "192.168.178.71";

// set wifi credentials - these would need to be changed for the locally available network
const char* ssid = "INSERT LOCAL WIFI SSID HERE";
const char* password = "INSERT LOCAL WIFI password HERE";

//define the NTP client for getting the time
//timezones are unimportant as time is only used to calculate number of days 
//i.e. 25 hour periods between two time points
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org"); //use the worldwide pool ntp
//global variable to hold the most recently retreived time point as epoch time
//i.e. seconds since the beginning of 1st Jan 1970
unsigned long epochTime;

//Filename for the file that stores the long term data
String foodstuffs_filename = "saved_foodstuffs.txt";

//initilalise the loadcell sensor on pins 14 and 12 (D5, D6)
HX711_ADC LoadCell(14,12);

//declare the WiFi and http client used for getting the sensor module ambient sensor data
WiFiClient client;
HTTPClient http;

//declare JSON doc used to contain the foodstuffs being tracked and their respective data
StaticJsonDocument<4000> foodstuffs;
//declare JSON doc used to respond to JSON REST API requests
StaticJsonDocument<512> doc;

//allocate the pins to the respective devices
const int potentiometer_pin = A0;
const int switch_pin = D0;
const int push_button2_pin = D7;
const int push_button1_pin = D8;

//declare the global variables used to keep track of values from sensors and physical devices
int switch_value;
int push_button1_value;
int push_button2_value;
int pote_value;
float current_loadcell_value;

//declare global variables for keeping track of the screens displayed on the OLED
int monitor_mode_screen_selection_value = 0;
int add_remove_mode_screen_selection_value = 0;
int num_screens_in_monitor_mode = 3;
int num_screens_in_add_remove_mode = 4;

//define the available foods for tracking on the device and the number of days they are good for.
//At the moment this is hardcoded but much better in the next iteration to allow this to be a 
//modifiable data structure so users can add or remove there own foods and adjust time lengths
const int num_available_foods = 11;
const String available_foods[num_available_foods][2] = {
  {"Apples", "6"}, {"Bananas", "4"}, {"Blueberry", "3"}, {"Brocolli", "6"}, {"Cauliflwr", "6"}, {"Cherries", "3"}, 
  {"Grapes", "5"}, {"Potatoes", "7"}, {"Onions", "14"}, {"Oranges", "6"}, {"Leftovers", "2"}
};

//global variable for tracking which foodstuff is currently being viewed or edited
int current_food;

//global variables used to contain data from the JSON object retreived from the ambient sensor module
//these could be eliminated and the JSON doc used directly but they act here as aliases to make the
//variable names shorter
int temp_sensor_value;
bool temp_sensor_in_range;
int temp_sensor_low_parameter;
int temp_sensor_high_parameter;
int temp_sensor_out_of_range_events;
int hum_sensor_value;
bool hum_sensor_in_range;
int hum_sensor_low_parameter;
int hum_sensor_high_parameter;
int hum_sensor_out_of_range_events;

///////// End of initial values, global variables and sensor declarations ////////////

////////////////////////// Setup function ////////////////////////////////////////////
void setup() {
  //assign the pin modes to the respective device pins
  pinMode(potentiometer_pin, INPUT);
  pinMode(push_button1_pin, INPUT);
  pinMode(push_button2_pin, INPUT);
  pinMode(switch_pin, INPUT);
  
  //initialise Serial port
  Serial.begin(9600);
  while (!Serial) continue;

  //initialise the load cell sensor
  LoadCell.begin();
  //allow two seconds for values to stabilise
  LoadCell.start(2000);
  //set specific calibration factor to my load cell setup
  LoadCell.setCalFactor(416); 

  //initialise the time client
  timeClient.begin();
  timeClient.setTimeOffset(3600); //GMT +1

  //initialise the fileSystem
  LittleFS.begin();
  //read in from long-term memory the values for all foodstuffs
  String result = loadFoodstuffsFromFile();
  //if there is no file or a read error then build a new JSON object with initial values
  if (result == ""){
    for(int i = 0; i < num_available_foods ; i++){
      JsonObject obj = foodstuffs.createNestedObject(available_foods[i][0]);
      obj["name"] = available_foods[i][0];
      obj["present"] = false;
      obj["timeEntered"] = NULL;
      obj["goodForDays"] = available_foods[i][1];
      obj["amountWasted[g]"] = NULL;
    }
  }

  //set the server routes
  server.on("/", get_index);
  server.on("/json/pantry", get_pantry_json);
  //initialise the web server
  server.begin();

  //initialise the OLED display
  spinUpOledDisplay();
  
  // Connect to the WiFi network
  WiFi.begin(ssid, password);
  delay(1000);
  // wait for successful connection
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.println("Waiting to connect...");
  }
  Serial.println("Connected!");

//  String output;
//  serializeJson(foodstuffs, output);
}
////////////////////////// End of setup function ////////////////////////////////////////////

////////////////////////// Loop function ////////////////////////////////////////////////////
void loop(){
  //get the current time from the ntp server
  timeClient.update();
  //set the current epoch time
  epochTime = timeClient.getEpochTime();

  //handle incoming webserver requests
  server.handleClient();
  
  // This keeps the serial monitor available 
  Serial.println("Server is running");

  //read the current potentiometer value
  pote_value = analogRead(potentiometer_pin);
  //set the current food by the potentiometer value
  current_food = map(pote_value, 1023, 0, 0, num_available_foods -1);

  //get the current measured weight from the load cell
  LoadCell.update();
  current_loadcell_value = LoadCell.getData();

  //if the position switch is set to Monitor mode
  if(isMonitorModeActive()){
    //show the default Monitor mode screen
    if(monitor_mode_screen_selection_value == 0){
      displayEatTodayItems();
      //check for button presses and change the screen if found
      if(checkForButton1Press()){
        cycleMonitorModeScreens();
      }
    //show the eat tomorrow screen
    } else if(monitor_mode_screen_selection_value == 1){
      displayEatTomorrowItems();
      //check for button presses and change the screen if found
      if(checkForButton1Press()){
        cycleMonitorModeScreens();
      }
    //show the ambient sensor data from the ambient sensor module
    } else if (monitor_mode_screen_selection_value == 2){
      //get the data from the ambient sensor through the ambient sensor REST API
      getAmbientSensorModuleDataJson();
      //set the internal conditions (aliases) with the retreived data
      setGlobalConditionsVariablesFromJson();
      //display the current ambient sensor data to the screen
      displayAmbientSensorModuleCurrentConditions();
      //delay 20 seconds before polling the server again while maintaining responsiveness
      delayWithResponsiveButtons(20);
    } 
  //if the position switch is set to AddRemove mode
  } else {
    //display the AddRemove function on the current food
    if(add_remove_mode_screen_selection_value == 0){
      displayAddRemoveToFromPantryScreen();
    //display the add waste screen for current foodstuff
    } else if (add_remove_mode_screen_selection_value == 1){
      displayAddWasteScreen();
    //display the reset waste screen for current foodstuff
    } else if (add_remove_mode_screen_selection_value == 2){
      displayResetWasteScreen();
    //display the reset waste screen for all foodstuffs
    } else if (add_remove_mode_screen_selection_value == 3){
      displayResetAllWasteScreen();
    }
    //check for button presses and cycle the screens if seen
    if(checkForButton1Press()){
      cycleAddRemoveModeScreens();
    }
  }  
}
////////////////////////// End of loop function /////////////////////////////////

////////////////////////// Mode and button functions ////////////////////////////////////

//check the switch position for the current mode and handle logic when switch is repositioned
bool isMonitorModeActive(){
  bool prevSwitchValue = switch_value;
  switch_value = digitalRead(switch_pin);
  if(prevSwitchValue != switch_value){
    display.clearDisplay();
    display.display();
    if(switch_value == 1 && monitor_mode_screen_selection_value == 2){
      displayLoadingStorageConditionsScreen();
    }
  }
  if (switch_value == 0){
    return false;
  }
  return true;
}

//returns true if the pushButton1 is currently depressed
bool isPushButton1Pressed(){
  push_button1_value = digitalRead(push_button1_pin);
  if (push_button1_value == 1){
    return true;
  }
  return false;
}

//returns true if the pushButton2 is currently depressed
bool isPushButton2Pressed(){
  push_button2_value = digitalRead(push_button2_pin);
  if (push_button2_value == 1){
    return true;
  }
  return false;
}

//checks for a single down-up press of the pushButton1
bool checkForButton1Press(){
  int prevPushButtonValue1 = push_button1_value;
  if(isPushButton1Pressed()){
    if(prevPushButtonValue1 != push_button1_value && push_button1_value == 1){
      return true;
    }
  }
  return false;
}

//checks for a single down-up press of the pushButton2
bool checkForButton2Press(){
  int prevPushButtonValue2 = push_button2_value;
  if(isPushButton2Pressed()){
    if(prevPushButtonValue2 != push_button2_value && push_button2_value == 1){
      return true;
    }
  }
  return false;
}
////////////////////////// End of mode and button functions ///////////////////////

////////////////////////// Utility functions //////////////////////////////////////

//delay the polling of the server for 10 seconds but check every 1/10th second if
//the user has changed the Switch position away from monitor mode, or pushed a
//button. Also keeps the server handling requests. Allows us not to poll the server
//not too often while maintaining responsiveness.
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

////////////////////// End of utility functions ///////////////////////////////////

//////////////////////////// Pantry functions /////////////////////////////////////

//switch the "present" bool of the current food to its opposite value and record the time entered.
void addOrRemoveCurrentFoodFromPantry(){
  foodstuffs[available_foods[current_food][0]]["present"] = !foodstuffs[available_foods[current_food][0]]["present"].as<bool>();
  foodstuffs[available_foods[current_food][0]]["timeEntered"] = epochTime;
}

//add waste value from the load cell to the current food waste value
void addWasteForCurrentFood(){
  foodstuffs[available_foods[current_food][0]]["amountWasted[g]"] = foodstuffs[available_foods[current_food][0]]["amountWasted[g]"].as<float>() + current_loadcell_value;
}

//reset all the waste values
void resetAllWasteValues(){
  for(int i = 0; i < num_available_foods; i++){
      foodstuffs[available_foods[i][0]]["amountWasted[g]"] = 0.0;    
    }
  writeToFoodstuffsfile();
  displayResetAllWasteValuesCompletedScreen();
}

//reset the amount of waste for the current food
void resetWasteOfCurrentFood(){
  foodstuffs[available_foods[current_food][0]]["amountWasted[g]"] = 0.0;  
}
////////////////////////// End of Pantry functions /////////////////////////////////////

//////////////////////// Time calculation functions ///////////////////////////////

//get the days remaining for a foodstuff before it goes bad as defined in the available foods array
int getDaysRemainingForFoodstuff(String foodstuffName){
  int daysSinceEntered = getDaysSinceEnteredForFoodstuff(foodstuffName);
  return foodstuffs[foodstuffName]["goodForDays"].as<int>() - daysSinceEntered;
}

//get the days since entry of a foodstuff
int getDaysSinceEnteredForFoodstuff(String foodstuffName){ 
  int secondsSinceEntered = epochTime - foodstuffs[foodstuffName]["timeEntered"].as<int>(); // + 86400 * 3 - 3; //simulate almost three days later
  return secondsSinceEntered / 60 / 60 / 24;
}
///////////////////// End of calculation functions ///////////////////////////

////////////////////////// HTTP client functions /////////////////////////////

//get the ambient sensor data from the ambient sensor module via an HTTP request to its REST api
void getAmbientSensorModuleDataJson(){
  http.useHTTP10(true);
  http.begin(client, "http://192.168.178.71/json/sensors");
  http.GET();
  deserializeJson(doc, http.getStream());
  Serial.println(doc["temperature sensor"].as<String>());
  Serial.println(doc["humidity sensor"].as<String>());
  http.end();
}

//set the global variables (aliases) for the retreived data from the ambient sensor module
void setGlobalConditionsVariablesFromJson(){
  temp_sensor_value = doc["temperature sensor"]["sensorValue"].as<int>();
  temp_sensor_in_range = doc["temperature sensor"]["inRange"].as<bool>();
  temp_sensor_low_parameter = doc["temperature sensor"]["lowParameter"].as<int>();
  temp_sensor_high_parameter = doc["temperature sensor"]["highParameter"].as<int>();
  temp_sensor_out_of_range_events = doc["temperature sensor"]["outOfRangeEvents"].as<int>();
  hum_sensor_value = doc["humidity sensor"]["sensorValue"].as<int>();
  hum_sensor_in_range = doc["humidity sensor"]["inRange"].as<bool>();
  hum_sensor_low_parameter = doc["humidity sensor"]["lowParameter"].as<int>();
  hum_sensor_high_parameter = doc["humidity sensor"]["highParameter"].as<int>();
  hum_sensor_out_of_range_events = doc["humidity sensor"]["outOfRangeEvents"].as<int>();
}
////////////////////// End of HTTP client functions //////////////////////////

////////////////////////////// Web server routes /////////////////////////////

//at the index return the dashboard
void get_index(){
  String html = "<!DOCTYPE html> <html>";
  html += "<head><meta http_equiv=\"refresh\" content=\"2\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head>";
  html += "<body> <h1>The Smart Pantry Check-in/out Module Dashboard</h1>";
  html += "<p>Welcome to the Smart Pantry dashboard</p>";
  html += "</br><h2>Pantry contents</h2>";
  //write a table of the current contents of the pantry
  html += "<table>";
  html += "<th style=\"border-bottom: 2px solid black;\">Foodstuff</th><th style=\"border-bottom: 2px solid black;\">Days in pantry</th><th style=\"border-bottom: 2px solid black;\">Days until spoilage</th>";
  int presentCount = 0;
  for(int i = 0; i < num_available_foods; i++){
    if(foodstuffs[available_foods[i][0]]["present"].as<bool>()){
      presentCount++;
      String daysSinceEntered = String(getDaysSinceEnteredForFoodstuff(available_foods[i][0]));
      String daysRemaining = String(getDaysRemainingForFoodstuff(available_foods[i][0]));
      html += "<tr>";
      html += "<td style=\"border-bottom: 1px solid black;\">" + available_foods[i][0] + "</td>";
      html += "<td style=\"border-bottom: 1px solid black;\">" + daysSinceEntered + "</td>";
      html += "<td style=\"border-bottom: 1px solid black;\">" + daysRemaining + "</td>";
      html += "</tr>";
    }
  }
  html += "</table>";
  if(presentCount == 0){
    html += "<p>The Pantry is empty!</p>";
  }
  html += "</br><h2>Waste records</h2>";
  //write a table of the cumulative waste of all foodstuffs
  html += "<table>";
  html += "<th style=\"border-bottom: 2px solid black;\">Foodstuff</th><th style=\"border-bottom: 2px solid black;\">Amount wasted (Kg)</th>";
  int wastedCount = 0;
  for(int i = 0; i < num_available_foods; i++){
    float amountWastedG = foodstuffs[available_foods[i][0]]["amountWasted[g]"].as<float>();
    if(amountWastedG > 0.00){
      String amountWastedKg = String(amountWastedG / 1000);
      wastedCount++;
      html += "<tr>";
      html += "<td style=\"border-bottom: 1px solid black;\">" + available_foods[i][0] + "</td>";
      html += "<td style=\"border-bottom: 1px solid black;\">" + amountWastedKg + "</td>";
      html += "</tr>";
    }
  }
  html += "</table>";
  if(wastedCount == 0){
    html += "<p>You haven't wasted any food yet. Well done!</p>";
  }
  html += "</body> </html>";
  server.send(200, "text/html", html);
}

//return the pantry contents and their data as prettified JSON
void get_pantry_json(){
  String jsonStr;  
  serializeJsonPretty(foodstuffs, jsonStr);    
  server.send(200, "application/json", jsonStr);
}

//////////////////////// End of web server routes /////////////////////////

///////////////////////// Filesystem functions ///////////////////////////////

//load the pantry data from file
String loadFoodstuffsFromFile() {
  String result = "";
  File foodstuffsFile = LittleFS.open(foodstuffs_filename, "r");
  if (!foodstuffsFile) { 
    // failed the read operation return blank result
    Serial.print("Could not read " + foodstuffs_filename);
    return result;
  }
  while (foodstuffsFile.available()) {
    result += (char)foodstuffsFile.read();
  }
  foodstuffsFile.close();
  Serial.print("Got " + result + " from " + foodstuffs_filename);
  buildFoodstuffsFromString(result);
  return result;
}

//build the JSON foodstuffs document from a string 
//i.e the loaded data from the pantry from the filesystem
void buildFoodstuffsFromString(String jsonString){
  deserializeJson(foodstuffs, jsonString);  
}

//write the current JSON foodstuffs document out to file for non-volatile storage
bool writeToFoodstuffsfile() {  
  File foodstuffsFile = LittleFS.open(foodstuffs_filename, "w");
  if (!foodstuffsFile) { 
    // failed to open the foodstuffsFile for writing
    return false;
  }

  String foodstuffsStr;
  serializeJson(foodstuffs, foodstuffsStr);
  int bytesWritten = foodstuffsFile.print(foodstuffsStr);
  if (bytesWritten == 0) { 
    // write operation failed on foodstuffsFile
    return false;
  }
   
  foodstuffsFile.close();
  return true;
}
//////////////////// End of filesystem functions ////////////////////////////

/////////////////////////// OLED Display functions //////////////////////////////

//perform initialisation of the OLED display and print the welcome message
void spinUpOledDisplay(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 OLED display failed to initialise");
  }
  delay(2000);
  //make sure the display is clear
  display.clearDisplay();
  //set the OLED text color, white here though will display on the modal as yellow for first two
  //lines (at text size 1) and blue underneath
  display.setTextColor(WHITE);
  displayWelcomeMessage();
}

//cycle through the screens in Monitor mode
void cycleMonitorModeScreens(){
  if(monitor_mode_screen_selection_value == num_screens_in_monitor_mode - 1){
    monitor_mode_screen_selection_value = 0;
  } else {
    monitor_mode_screen_selection_value++;
  }
  if(monitor_mode_screen_selection_value == 2){
    displayLoadingStorageConditionsScreen();
  }
}

//cycle through the screens in AddRemove mode
void cycleAddRemoveModeScreens(){
  if(add_remove_mode_screen_selection_value == num_screens_in_add_remove_mode - 1){
    add_remove_mode_screen_selection_value = 0;
  } else {
    add_remove_mode_screen_selection_value++;
  }
}

//display the welcome message to the OLED screen
void displayWelcomeMessage(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  // Display welcome message
  display.println("Welcome to");
  display.println("Smart");
  display.println("Pantry!");
  display.display();
}

        ///// Monitor mode screens ////

//display the current ambient sensor data
void displayAmbientSensorModuleCurrentConditions(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Storage conditions");
  display.setCursor(0,16);
  display.print("T: ");
  display.print(temp_sensor_value);
  display.print("C");
  if(temp_sensor_in_range){
    display.println(" - okay!");
  } else {
    if(temp_sensor_value < temp_sensor_low_parameter){
      display.println(" - too low!");
    } else {
      display.println(" - too high!");  
    }
  }
  display.setCursor(0,32);
  display.print("H: ");
  display.print(hum_sensor_value);
  display.print("%");
    if(hum_sensor_in_range){
    display.println(" - okay!");
  } else {
    if(hum_sensor_value < hum_sensor_low_parameter){
      display.println(" - too low!");
    } else {
      display.println(" - too high!");  
    }
  }
  display.display();
}

//display the screen with items to eat today
void displayEatTodayItems(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println("Eat today:");
  display.setTextSize(1);
  for(int i = 0; i < num_available_foods; i++){
    if(foodstuffs[available_foods[i][0]]["present"]){
      if(getDaysRemainingForFoodstuff(available_foods[i][0]) <= 0){
        display.print(foodstuffs[available_foods[i][0]]["name"].as<String>());
        display.print(", ");
      }
    }
  }
  display.display();
}

//display the screen with items to eat by tomorrow
void displayEatTomorrowItems(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("Eat by tomorrow:");
  display.setCursor(0,16);
  for(int i = 0; i < num_available_foods; i++){
    if(foodstuffs[available_foods[i][0]]["present"]){
      if(getDaysRemainingForFoodstuff(available_foods[i][0]) == 1){
        display.print(foodstuffs[available_foods[i][0]]["name"].as<String>());
        display.print(", ");
      }
    }
  }
  display.display();
}

//display the screen while waiting for data from the ambient sensor module
void displayLoadingStorageConditionsScreen(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("Storage conditions"); 
  display.setCursor(0, 16);
  display.println("loading...");
  display.display(); 
}
        ///// End of Monitor mode screens ////

          ///// AddRemove mode screens ////

//display the screen to view and add or remove a foodstuff from the pantry
void displayAddRemoveToFromPantryScreen(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(foodstuffs[available_foods[current_food][0]]["name"].as<String>());
  display.setTextSize(1);
  display.println("");
  if(foodstuffs[available_foods[current_food][0]]["present"].as<bool>()){
    display.print("Days kept: ");
    display.println(getDaysSinceEnteredForFoodstuff(available_foods[current_food][0]));
    display.println("");
    display.print("Eat within: ");
    display.print(getDaysRemainingForFoodstuff(available_foods[current_food][0]));
    display.print(" days");
  }
  if(foodstuffs[available_foods[current_food][0]]["present"]){
    display.println("");
    display.println("");
    display.println("   Remove from pantry");  
  } else {
    display.println("");
    display.println("");
    display.println("");
    display.println("");
    display.println("        Add to pantry");  
  }
  display.display();
  if(checkForButton2Press()){
    addOrRemoveCurrentFoodFromPantry();
    //write the changes to the saved foodstuffs file
    writeToFoodstuffsfile();
  }
}

//display the add waste screen for the current food to the OLED screen
void displayAddWasteScreen(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(foodstuffs[available_foods[current_food][0]]["name"].as<String>());
  display.setTextSize(1);
  display.println("");
  display.print("Wasted[Kg]: ");
  display.println(foodstuffs[available_foods[current_food][0]]["amountWasted[g]"].as<float>() / 1000);
  display.println("");
  display.print("Scales[g]: ");
  display.print(current_loadcell_value);
  display.println("");
  display.println("");
  display.println("            Add waste");
  display.display();
  if(checkForButton2Press()){
    addWasteForCurrentFood();  
    writeToFoodstuffsfile();
  }
}

//display the reset waste screen for the current food to the OLED screen
void displayResetWasteScreen(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(foodstuffs[available_foods[current_food][0]]["name"].as<String>());
  display.setTextSize(1);
  display.println("");
  display.print("Wasted[Kg]: ");
  display.println(foodstuffs[available_foods[current_food][0]]["amountWasted[g]"].as<float>() / 1000);
  display.println();
  display.println("");
  display.println("");
  display.println("     Reset waste to 0");
  display.display();
  if(checkForButton2Press()){
    resetWasteOfCurrentFood();
    writeToFoodstuffsfile();
  }
}

//display the reset all waste screen to the OLED screen
void displayResetAllWasteScreen(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println("Reset all");
  display.setTextSize(1);
  display.println("");
  display.print("Reset the waste levels for all foods?");
  display.println("");
  display.println("");
  display.println("");
  display.println(" Reset all waste to 0");
  display.display();
  if(checkForButton2Press()){
    resetAllWasteValues();
  }
}

//display the success screen for resetting all waste values
void displayResetAllWasteValuesCompletedScreen(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println("Reset all");
  display.setTextSize(1);
  display.println("");
  display.print("Reset completed!");
  display.display();
  delay(2000);
  cycleAddRemoveModeScreens();
}
            ///// End of AddRemove mode screens ////

//////////////////// End of OLED Display functions ///////////////////////

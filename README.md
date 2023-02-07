# Smart Pantry
 A smart pantry system to help consumers reduce their food waste. Allows the user to monitor storage conditions, track perishable item expiry dates and record waste levels. Each node presents a rudimentary web-based dashboard served to the local network for data summary.

## Introduction
Household food waste is a key area for improvement in terms of reducing global greenhouse emissions and sustainable living. This project represents a prototype version of a smart pantry system to track storage conditions, expiry dates of food and waste levels generated with the aim of materially reducing household food waste.

## Implementation
The system is composed of two Esp8266 12-E microcontroller nodes; an ambient sensor module - designed to be placed in the pantry storage area, and a check in/out module - to be placed visibly in the kitchen.

Arduino IDE sketches, Fritzing board diagrams and photos are provided for each node to be reconstructed and improved upon.

### Ambient sensor module
Monitors humidity and temperature and warns the user about out of range events to ensure their food is stored in optimum conditions for longevity.

<img src="https://user-images.githubusercontent.com/34540708/217272734-53dd2bbd-714c-4f1d-af0c-332e5b607b6b.png" width="300" height="200">  <img src="https://user-images.githubusercontent.com/34540708/217272328-79518606-b6bf-43ca-b3f3-f01d6473ebc0.png" width="300" height="200">

### Check in/out module
Foodstuffs can be checked in to the system whereupon their spoilage time is tracked and reminders served to the user about when they should be eaten through the integrated OLED screen.

<img src="https://user-images.githubusercontent.com/34540708/217270662-209c542f-03d6-4a27-a7a1-0c0ced2d9f81.png" width="200" height="200">  <img src="https://user-images.githubusercontent.com/34540708/217270688-c6300f3e-0355-4519-a7cf-0ed4a876aae6.png" width="200" height="200">

Waste levels can be input by weighing waste using the connected load cell and tracked over time to allow users to record and review their waste levels and adjust their purchasing or usage behaviours.

<img src="https://user-images.githubusercontent.com/34540708/217270583-70ab4195-0d31-452f-ae4d-a8749c168199.png" width="200" height="200">


## Suggested next steps
Improve the web dashboards to provide time series data - for example graphed waste levels.

Implement MQTT based messaging between the nodes.

Improve the robustness of the code in terms of error handling.

Provide more options for inputting additional foodstuffs and modifying their spoilage times.

Link the quality of ambient storage conditions to the spoilage rate of foodstuffs. i.e. shorten expeted spoilage times when conditions are outside of normal or optimum ranges.

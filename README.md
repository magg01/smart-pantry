# smart-pantry
 A smart pantry system to help consumers reduce their food waste. Allows the user to monitor storage conditions, track perishable item expiry dates and record waste levels. Each node presents a rudimentary web-based dashboard served to the local network for data summary.

## Introduction
Household food waste is a key area for improvement in terms of reducing global greenhouse emissions and sustainable living. This project represents a prototype version of a smart pantry system to track storage conditions, expiry dates of food and waste levels generated with the aim of materially reducing household food waste.

## Implementation
The system is composed of two Esp8266 12-E microcontroller nodes; an ambient sensor module - designed to be placed in the pantry storage area, and a check in/out module - to be placed visibly in the kitchen.

Arduino IDE sketches, Fritzing board diagrams and photos are provided for each node to be reconstructed and improved upon.

### Ambient sensor module
Monitors humidity and temperature and warns the user about out of range events to ensure their food is stored in optimum conditions for longevity.

### Check in/out module
Foodstuffs can be checked in to the system whereupon their spoilage time is tracked and reminders served to the user about when they should be eaten through the integrated OLED screen.

Waste levels can be input by weighing waste using the connected load cell and tracked over time to allow users to record and review their waste levels and adjust their purchasing or usage behaviours.

## Suggested next steps
Improve the web dashboards to provide time series data - for example graphed waste levels.

Implement MQTT based messaging between the nodes.

Improve the robustness of the code in terms of error handling.

Provide more options for inputting additional foodstuffs and modifying their spoilage times.

Link the quality of ambient storage conditions to the spoilage rate of foodstuffs. i.e. shorten expeted spoilage times when conditions are outside of normal or optimum ranges.

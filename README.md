# rfid-attendance-system
Attendance system verifier using RFID, MQTT and ESP-IDF framework for an ESP-WROOM-32 devboard.

## Firmware
The RC522 module collects the UID from the PICC and publishes to a topic which a server is subscribed. The server checks its database for the UID and sends a response to the ESP32, which is a subscriber to a response topic. According to the topic's data, a green or red LED blinks and a certain frequency is played in a piezo buzzer.

## Server
The server is using a microservice architecture with Docker containers and Docker compose. The following services are present in the server directory: broker, database, interface and subscriber. The broker is Mosquitto, Redis as a database and Python scripts for the interface and subscriber services.   
   
To run the services, use ```docker compose up --build``` in the **server** directory.

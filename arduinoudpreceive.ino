#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Servo.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// UDP settings
WiFiUDP UDP;
unsigned int localUdpPort = 8888;  // The port to listen on
char incomingPacket[255];          // Buffer for incoming packets

// Hardware pins
const int LED_PIN = D4;            // Built-in LED on most NodeMCU boards (active LOW)
const int SERVO_PIN = D2;          // Pin for servo control
const int MOTOR_PIN = D1;          // Pin for motor control (PWM)

// Status variables
bool ledStatus = false;
int servoAngle = 0;
int motorSpeed = 0;

// Hardware objects
Servo myServo;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("\n\nESP8266 UDP JSON Controller");
  
  // Initialize hardware
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Turn LED off initially (active LOW)
  
  myServo.attach(SERVO_PIN);
  myServo.write(servoAngle);
  
  pinMode(MOTOR_PIN, OUTPUT);
  analogWrite(MOTOR_PIN, motorSpeed);
  
  // Connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Start listening for UDP packets
  Serial.printf("Starting UDP server on port %d\n", localUdpPort);
  UDP.begin(localUdpPort);
}

void loop() {
  // Check if there's data available to read
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    // Read the packet into the buffer
    int len = UDP.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0;  // Null terminate the string
    }
    
    Serial.printf("Received %d bytes from %s, port %d\n", 
                 packetSize, UDP.remoteIP().toString().c_str(), UDP.remotePort());
    Serial.printf("Packet contents: %s\n", incomingPacket);
    
    // Process the received JSON data
    processJsonCommand(incomingPacket);
  }
}

void processJsonCommand(char* jsonString) {
  // Allocate a dynamic JsonDocument
  DynamicJsonDocument doc(1024);
  
  // Parse the JSON data
  DeserializationError error = deserializeJson(doc, jsonString);
  
  // Check for parsing errors
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    sendResponse("error", "JSON parsing failed");
    return;
  }
  
  // Extract the command
  const char* command = doc["command"];
  if (!command) {
    sendResponse("error", "No command specified");
    return;
  }
  
  // Process different commands
  String cmdStr = String(command);
  
  if (cmdStr == "led") {
    // Handle LED command
    if (doc.containsKey("value")) {
      int value = doc["value"];
      ledStatus = value > 0;
      digitalWrite(LED_PIN, !ledStatus);  // Invert because LED is active LOW
      Serial.printf("LED set to %s\n", ledStatus ? "ON" : "OFF");
      sendResponse("success", "LED state updated");
    } else {
      sendResponse("error", "LED value not specified");
    }
  } 
  else if (cmdStr == "servo") {
    // Handle servo command
    if (doc.containsKey("angle")) {
      servoAngle = constrain(doc["angle"], 0, 180);
      myServo.write(servoAngle);
      Serial.printf("Servo angle set to %d degrees\n", servoAngle);
      sendResponse("success", "Servo angle updated");
    } else {
      sendResponse("error", "Servo angle not specified");
    }
  } 
  else if (cmdStr == "motor") {
    // Handle motor command
    if (doc.containsKey("speed")) {
      motorSpeed = constrain(doc["speed"], 0, 255);
      analogWrite(MOTOR_PIN, motorSpeed);
      Serial.printf("Motor speed set to %d\n", motorSpeed);
      sendResponse("success", "Motor speed updated");
    } else {
      sendResponse("error", "Motor speed not specified");
    }
  } 
  else if (cmdStr == "get_status") {
    // Handle status request
    sendStatusResponse();
  } 
  else {
    // Unknown command
    Serial.printf("Unknown command: %s\n", command);
    sendResponse("error", "Unknown command");
  }
}

void sendResponse(const char* status, const char* message) {
  // Create a JSON response
  DynamicJsonDocument response(256);
  response["status"] = status;
  response["message"] = message;
  
  // Serialize to string
  String jsonResponse;
  serializeJson(response, jsonResponse);
  
  // Send via UDP
  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
  UDP.write(jsonResponse.c_str());
  UDP.endPacket();
}

void sendStatusResponse() {
  // Create a detailed status response
  DynamicJsonDocument response(512);
  response["status"] = "success";
  response["message"] = "Current status";
  
  // Add device status
  JsonObject data = response.createNestedObject("data");
  data["device"] = "ESP8266";
  data["ip"] = WiFi.localIP().toString();
  data["uptime_ms"] = millis();
  data["wifi_strength"] = WiFi.RSSI();
  
  // Add component status
  JsonObject components = data.createNestedObject("components");
  components["led"] = ledStatus ? "on" : "off";
  components["servo_angle"] = servoAngle;
  components["motor_speed"] = motorSpeed;
  
  // Serialize to string
  String jsonResponse;
  serializeJson(response, jsonResponse);
  
  // Send via UDP
  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
  UDP.write(jsonResponse.c_str());
  UDP.endPacket();
  
  Serial.println("Status sent");
}
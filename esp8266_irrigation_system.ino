#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Servo.h>

// WiFi Credentials
const char* ssid = "SOFI";
const char* password = "12345678";

// Server URL - CHANGE THIS TO YOUR RENDER.COM URL AFTER DEPLOYMENT
// Example: "https://your-app-name.onrender.com"
const char* serverUrl = "http://192.168.1.100:5000";  // Change to your Render URL

// Pin Definitions
#define SENSOR1_PIN A0      // Analog pin for moisture sensor 1
#define SENSOR2_PIN D0      // Digital pin for moisture sensor 2 (if using digital, or use analog multiplexer)
#define PUMP_PIN D1         // Relay pin for pump control
#define SERVO_PIN D2        // PWM pin for servo motor

// Note: ESP8266 has only one analog pin (A0)
// For second sensor, you can use:
// 1. Digital moisture sensor on D0
// 2. Analog multiplexer (CD4051)
// 3. Second ESP8266
// This code assumes sensor2 is connected via analog multiplexer or digital output

// Servo object
Servo servoMotor;

// Global variables
int sensor1Value = 0;
int sensor2Value = 0;
String pumpCommand = "OFF";
String servoCommand = "IDLE";

// Servo state variables
int servoAngle = 0;
bool servoRotating = false;
int rotationDirection = 1;  // 1 for forward, -1 for backward
unsigned long lastServoUpdate = 0;
const int servoUpdateInterval = 20;  // Update servo every 20ms for smooth rotation

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastServerUpdate = 0;
unsigned long lastCommandPoll = 0;
const unsigned long sensorInterval = 2000;    // Read sensors every 2 seconds
const unsigned long serverInterval = 5000;    // Send data to server every 5 seconds
const unsigned long commandInterval = 3000;   // Poll commands every 3 seconds

WiFiClient wifiClient;

void setup() {
  Serial.begin(115200);
  delay(10);
  
  Serial.println("\n\n=================================");
  Serial.println("Smart Tree Irrigation System");
  Serial.println("ESP8266 Controller");
  Serial.println("=================================\n");
  
  // Initialize pins
  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);  // Pump OFF initially
  
  // Initialize servo
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(90);  // Center position
  delay(500);
  
  // Connect to WiFi
  connectToWiFi();
  
  Serial.println("System initialized successfully!");
  Serial.println("Starting main loop...\n");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    connectToWiFi();
  }
  
  // Read sensors
  if (currentMillis - lastSensorRead >= sensorInterval) {
    lastSensorRead = currentMillis;
    readSensors();
  }
  
  // Send sensor data to server
  if (currentMillis - lastServerUpdate >= serverInterval) {
    lastServerUpdate = currentMillis;
    sendSensorData();
  }
  
  // Poll commands from server
  if (currentMillis - lastCommandPoll >= commandInterval) {
    lastCommandPoll = currentMillis;
    getCommandsFromServer();
  }
  
  // Execute commands
  executeCommands();
  
  // Handle servo rotation if in STATE2
  if (servoRotating && (currentMillis - lastServoUpdate >= servoUpdateInterval)) {
    lastServoUpdate = currentMillis;
    updateServoRotation();
  }
  
  delay(10);  // Small delay to prevent WDT reset
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm\n");
  } else {
    Serial.println("\nFailed to connect to WiFi!");
    Serial.println("Please check your credentials and try again.");
    // Retry after delay
    delay(5000);
    connectToWiFi();
  }
}

void readSensors() {
  // Read moisture sensor 1 (Analog)
  int rawValue1 = analogRead(SENSOR1_PIN);
  // Convert to percentage (0-1023 -> 0-100%)
  // Note: Calibrate these values based on your sensor
  // Typically: dry = high value, wet = low value
  sensor1Value = map(rawValue1, 1023, 0, 0, 100);
  sensor1Value = constrain(sensor1Value, 0, 100);
  
  // Read moisture sensor 2
  // If using digital sensor, read digital value
  // If using analog multiplexer, implement multiplexer reading here
  // For demonstration, using a digital read (modify as needed)
  int rawValue2 = digitalRead(SENSOR2_PIN);
  sensor2Value = rawValue2 * 50;  // Simple conversion, modify based on your setup
  
  // If you have analog multiplexer for sensor 2, use this approach:
  // sensor2Value = readAnalogMultiplexer(channel);
  
  Serial.println("--- Sensor Readings ---");
  Serial.print("Sensor 1 (Field 1): ");
  Serial.print(sensor1Value);
  Serial.println("%");
  Serial.print("Sensor 2 (Field 2): ");
  Serial.print(sensor2Value);
  Serial.println("%");
  Serial.println();
}

void sendSensorData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send data: WiFi not connected");
    return;
  }
  
  HTTPClient http;
  String url = String(serverUrl) + "/api/sensor_data";
  
  http.begin(wifiClient, url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["sensor1"] = sensor1Value;
  doc["sensor2"] = sensor2Value;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("Sending sensor data to server...");
  Serial.println("URL: " + url);
  Serial.println("Payload: " + jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Server response code: ");
    Serial.println(httpResponseCode);
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.print("Error sending data. Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Error: " + http.errorToString(httpResponseCode));
  }
  
  http.end();
  Serial.println();
}

void getCommandsFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot get commands: WiFi not connected");
    return;
  }
  
  HTTPClient http;
  String url = String(serverUrl) + "/api/esp_commands";
  
  http.begin(wifiClient, url);
  
  Serial.println("Polling commands from server...");
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Commands received: ");
    Serial.println(response);
    
    // Parse JSON response
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      pumpCommand = doc["pump"].as<String>();
      servoCommand = doc["servo"].as<String>();
      
      Serial.print("Pump: ");
      Serial.print(pumpCommand);
      Serial.print(" | Servo: ");
      Serial.println(servoCommand);
    } else {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("Error getting commands. Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
  Serial.println();
}

void executeCommands() {
  // Control pump
  if (pumpCommand == "ON") {
    digitalWrite(PUMP_PIN, HIGH);  // Turn pump ON
  } else {
    digitalWrite(PUMP_PIN, LOW);   // Turn pump OFF
  }
  
  // Control servo
  if (servoCommand == "STATE1") {
    // State 1: Move to 180 degrees (right) and stay still
    servoRotating = false;
    servoMotor.write(180);
  } 
  else if (servoCommand == "STATE2") {
    // State 2: Continuous rotation between 0-160 degrees
    servoRotating = true;
  } 
  else {
    // IDLE: Move to center position (90 degrees)
    servoRotating = false;
    servoMotor.write(90);
  }
}

void updateServoRotation() {
  // Rotate servo between 0 and 160 degrees
  servoAngle += rotationDirection * 2;  // Increment by 2 degrees
  
  if (servoAngle >= 160) {
    servoAngle = 160;
    rotationDirection = -1;  // Reverse direction
  } else if (servoAngle <= 0) {
    servoAngle = 0;
    rotationDirection = 1;  // Forward direction
  }
  
  servoMotor.write(servoAngle);
}

// Optional: Function to read from analog multiplexer (if using CD4051 or similar)
/*
int readAnalogMultiplexer(int channel) {
  // Example for CD4051 8-channel multiplexer
  // Connect S0, S1, S2 to digital pins (e.g., D5, D6, D7)
  // Connect signal output to A0
  
  #define MUX_S0 D5
  #define MUX_S1 D6
  #define MUX_S2 D7
  
  digitalWrite(MUX_S0, bitRead(channel, 0));
  digitalWrite(MUX_S1, bitRead(channel, 1));
  digitalWrite(MUX_S2, bitRead(channel, 2));
  
  delay(10);  // Allow signal to settle
  
  int rawValue = analogRead(A0);
  int percentage = map(rawValue, 1023, 0, 0, 100);
  return constrain(percentage, 0, 100);
}
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Servo.h>

// ========================================
// CONFIGURATION - Modify these values
// ========================================

// WiFi Credentials - Multiple networks (will try in order)
const char* ssid = "TLE 512";
const char* password = "TLE_Cse-19";

const char* ssid2 = "SOFI";
const char* password2 = "12345678";

// Server URL - Your Render.com deployment
const char* serverUrl = "https://iot-smart-tree-irrigation-and-monitoring.onrender.com";

// Pin Definitions
#define SENSOR1_PIN A0      // Analog pin for moisture sensor 1 (Field 1)
#define SENSOR2_PIN D0      // Digital pin for moisture sensor 2 (Field 2)
#define PUMP_PIN D1         // Relay pin for water pump control
#define SERVO_PIN D2        // PWM pin for servo motor control
#define FIRE_RELAY_PIN D3   // Relay pin for fire pump control
#define FLAME_SENSOR_PIN D4 // Digital pin for flame sensor input
#define BUZZER_PIN D5       // Digital pin for buzzer alarm output

// Sensor Calibration (adjust based on your sensors)
#define SENSOR_DRY_VALUE 1023    // Analog reading when sensor is dry
#define SENSOR_WET_VALUE 0       // Analog reading when sensor is wet

// Timing Configuration (milliseconds)
#define SENSOR_READ_INTERVAL 2000      // Read sensors every 2 seconds
#define SERVER_UPDATE_INTERVAL 5000    // Send data to server every 5 seconds
#define COMMAND_POLL_INTERVAL 3000     // Poll commands from server every 3 seconds
#define SERVO_UPDATE_INTERVAL 5        // Update servo position every 5ms (maximum speed)

// Servo Configuration
#define SERVO_MIN_ANGLE 0       // Minimum rotation angle
#define SERVO_MAX_ANGLE 180     // Maximum rotation angle (full 180 degrees)
#define SERVO_FIELD1_ANGLE 180  // Static angle for Field 1 (STATE1)
#define SERVO_IDLE_ANGLE 90     // Center/idle position
#define SERVO_ROTATION_STEP 10  // Degrees to move per update (maximum speed for 360° servo)

// ========================================
// GLOBAL VARIABLES
// ========================================

// Servo object
Servo servoMotor;

// WiFi client for HTTPS
WiFiClientSecure wifiClient;

// Sensor data
int sensor1Value = 0;        // Analog sensor percentage (0-100%)
bool sensor2Dry = false;     // Digital DO sensor: true=Dry, false=Wet
bool fireDetected = false;

// Command states
String pumpCommand = "OFF";
String servoCommand = "IDLE";
String buzzerCommand = "OFF";

// Servo rotation state
int servoAngle = SERVO_IDLE_ANGLE;
bool servoRotating = false;
int rotationDirection = 1;  // 1 = forward, -1 = backward

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastServerUpdate = 0;
unsigned long lastCommandPoll = 0;
unsigned long lastServoUpdate = 0;

// Connection retry counter
int wifiReconnectAttempts = 0;
const int MAX_WIFI_RECONNECT = 3;

// ========================================
// SETUP FUNCTION
// ========================================

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\n===========================================");
  Serial.println("  Smart Tree Irrigation System");
  Serial.println("  ESP8266 IoT Controller v1.0");
  Serial.println("===========================================\n");
  
  // Initialize pins
  pinMode(SENSOR1_PIN, INPUT);
  pinMode(SENSOR2_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(FIRE_RELAY_PIN, OUTPUT);
  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  digitalWrite(PUMP_PIN, LOW);        // Ensure irrigation pump is OFF at startup
  digitalWrite(FIRE_RELAY_PIN, LOW);  // Ensure fire pump is OFF at startup
  digitalWrite(BUZZER_PIN, LOW);      // Ensure buzzer is OFF at startup
  
  Serial.println("[INIT] Pins configured");
  
  // Initialize servo motor
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(SERVO_IDLE_ANGLE);
  delay(500);
  Serial.println("[INIT] Servo motor initialized at idle position");
  
  // Configure WiFi client for HTTPS (disable certificate validation for simplicity)
  wifiClient.setInsecure();
  
  // Connect to WiFi
  connectToWiFi();
  
  Serial.println("\n[READY] System initialized successfully!");
  Serial.println("[INFO] Starting main control loop...\n");
  Serial.println("===========================================\n");
}

// ========================================
// MAIN LOOP
// ========================================

void loop() {
  unsigned long currentMillis = millis();
  
  // Ensure WiFi connection is maintained
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WIFI] Connection lost! Attempting to reconnect...");
    wifiReconnectAttempts++;
    
    if (wifiReconnectAttempts <= MAX_WIFI_RECONNECT) {
      connectToWiFi();
    } else {
      Serial.println("[ERROR] Max reconnection attempts reached. Restarting ESP8266...");
      delay(1000);
      ESP.restart();
    }
  } else {
    wifiReconnectAttempts = 0;  // Reset counter when connected
  }
  
  // Read sensor data at defined interval
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = currentMillis;
    readSensors();
  }
  
  // Send sensor data to server at defined interval
  if (currentMillis - lastServerUpdate >= SERVER_UPDATE_INTERVAL) {
    lastServerUpdate = currentMillis;
    sendSensorData();
  }
  
  // Poll commands from server at defined interval
  if (currentMillis - lastCommandPoll >= COMMAND_POLL_INTERVAL) {
    lastCommandPoll = currentMillis;
    getCommandsFromServer();
  }
  
  // FIRE EMERGENCY OVERRIDE - Takes precedence over all other commands
  if (fireDetected) {
    handleFireEmergency();
  } else {
    // Normal operation - execute received commands (pump, servo, and buzzer control)
    executeCommands();
    // Ensure fire relay is OFF when no fire
    digitalWrite(FIRE_RELAY_PIN, LOW);
  }
  
  // Handle continuous servo rotation for STATE2 with proper timing
  if (servoRotating && (currentMillis - lastServoUpdate >= SERVO_UPDATE_INTERVAL)) {
    lastServoUpdate = currentMillis;
    updateServoRotation();
  }
  
  // Small yield to prevent watchdog timer reset
  yield();
}

// ========================================
// WIFI CONNECTION FUNCTION
// ========================================

void connectToWiFi() {
  Serial.println("[WIFI] Attempting to connect to available networks...");
  
  // Try first network
  Serial.print("[WIFI] Trying network 1: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] ✓ Connected successfully!");
    Serial.print("[WIFI] Network: ");
    Serial.println(ssid);
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WIFI] Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("[WIFI] MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.println();
    return;
  }
  
  // Try second network if first failed
  Serial.println("\n[WIFI] First network failed. Trying network 2...");
  Serial.print("[WIFI] Trying network 2: ");
  Serial.println(ssid2);
  
  WiFi.begin(ssid2, password2);
  
  attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] ✓ Connected successfully!");
    Serial.print("[WIFI] Network: ");
    Serial.println(ssid2);
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WIFI] Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("[WIFI] MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.println();
  } else {
    Serial.println("\n[WIFI] ✗ Both networks failed!");
    Serial.println("[ERROR] Check WiFi credentials and signal strength");
    Serial.println("[INFO] Retrying in 5 seconds...\n");
    delay(5000);
    connectToWiFi();
  }
}

// ========================================
// SENSOR READING FUNCTION
// ========================================

void readSensors() {
  // Read moisture sensor 1 (Analog - Field 1) - Average of 5 readings for stability
  long sum1 = 0;
  for (int i = 0; i < 5; i++) {
    sum1 += analogRead(SENSOR1_PIN);
    delay(10);  // Small delay between readings
  }
  int rawValue1 = sum1 / 5;
  sensor1Value = map(rawValue1, SENSOR_DRY_VALUE, SENSOR_WET_VALUE, 0, 100);
  sensor1Value = constrain(sensor1Value, 0, 100);
  
  // Read moisture sensor 2 (Digital DO sensor - Field 2)
  // Digital sensor: HIGH = Dry soil, LOW = Wet soil
  // Average multiple readings for stability
  int highCount = 0;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(SENSOR2_PIN) == HIGH) {
      highCount++;
    }
    delay(5);
  }
  
  // Determine dry/wet state: if more than 5 out of 10 readings are HIGH, soil is dry
  sensor2Dry = (highCount > 5);
  
  // Read flame sensor (Digital - Active LOW: LOW = Fire detected)
  fireDetected = (digitalRead(FLAME_SENSOR_PIN) == LOW);
  
  // Display sensor readings with raw values for debugging
  Serial.println("┌─────────────────────────────────────┐");
  Serial.println("│       SENSOR READINGS               │");
  Serial.println("├─────────────────────────────────────┤");
  Serial.print("│ Field 1 (Analog): ");
  Serial.print(sensor1Value);
  Serial.print("% (Raw: ");
  Serial.print(rawValue1);
  Serial.println(")     │");
  Serial.print("│ Field 2 (DO Sensor): ");
  Serial.print(sensor2Dry ? "DRY" : "WET");
  Serial.print(" (HIGH: ");
  Serial.print(highCount);
  Serial.println("/10)  │");
  Serial.print("│ Fire Detected: ");
  Serial.print(fireDetected ? "YES ⚠️" : "NO ✓");
  Serial.println("      │");
  Serial.println("└─────────────────────────────────────┘\n");
}

// ========================================
// SEND DATA TO SERVER
// ========================================

void sendSensorData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] Cannot send data - WiFi not connected\n");
    return;
  }
  
  HTTPClient http;
  String url = String(serverUrl) + "/api/sensor_data";
  
  http.begin(wifiClient, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);  // 10 second timeout
  
  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["sensor1"] = sensor1Value;
  doc["sensor2_dry"] = sensor2Dry;
  doc["fire_detected"] = fireDetected;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("[HTTP] Sending sensor data...");
  Serial.print("[DATA] ");
  Serial.println(jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    Serial.print("[HTTP] ✓ Response code: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.print("[SERVER] ");
      Serial.println(response);
    }
  } else {
    Serial.print("[HTTP] ✗ Error: ");
    Serial.println(http.errorToString(httpResponseCode));
  }
  
  http.end();
  Serial.println();
}

// ========================================
// GET COMMANDS FROM SERVER
// ========================================

void getCommandsFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] Cannot get commands - WiFi not connected\n");
    return;
  }
  
  HTTPClient http;
  String url = String(serverUrl) + "/api/esp_commands";
  
  http.begin(wifiClient, url);
  http.setTimeout(10000);  // 10 second timeout
  
  Serial.println("[HTTP] Polling commands from server...");
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.print("[HTTP] ✓ Response code: ");
    Serial.println(httpResponseCode);
    
    if (httpResponseCode == 200) {
      String response = http.getString();
      
      // Parse JSON response
      StaticJsonDocument<300> doc;
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        String newPumpCommand = doc["pump"].as<String>();
        String newServoCommand = doc["servo"].as<String>();
        String newBuzzerCommand = doc["buzzer"].as<String>();
        
        // Only log if commands changed
        if (newPumpCommand != pumpCommand || newServoCommand != servoCommand || newBuzzerCommand != buzzerCommand) {
          Serial.println("[COMMAND] New commands received:");
          Serial.print("  → Pump: ");
          Serial.println(newPumpCommand);
          Serial.print("  → Servo: ");
          Serial.println(newServoCommand);
          Serial.print("  → Buzzer: ");
          Serial.println(newBuzzerCommand);
        }
        
        pumpCommand = newPumpCommand;
        servoCommand = newServoCommand;
        buzzerCommand = newBuzzerCommand;
      } else {
        Serial.print("[ERROR] JSON parsing failed: ");
        Serial.println(error.c_str());
      }
    }
  } else {
    Serial.print("[HTTP] ✗ Error: ");
    Serial.println(http.errorToString(httpResponseCode));
  }
  
  http.end();
  Serial.println();
}

// ========================================
// EXECUTE COMMANDS (PUMP & SERVO)
// ========================================

void executeCommands() {
  // Control water pump via relay
  if (pumpCommand == "ON") {
    digitalWrite(PUMP_PIN, HIGH);  // Relay ON → Pump ON
  } else {
    digitalWrite(PUMP_PIN, LOW);   // Relay OFF → Pump OFF
  }
  
  // Control servo motor position/rotation
  if (servoCommand == "STATE1") {
    // STATE1: Field 1 irrigation - Static position at 180°
    servoRotating = false;
    servoMotor.write(SERVO_FIELD1_ANGLE);
    
  } else if (servoCommand == "STATE2") {
    // STATE2: Field 2 irrigation - Continuous sweeping 0-180° back and forth
    servoRotating = true;
    
  } else {
    // IDLE: No irrigation - Center position
    servoRotating = false;
    servoMotor.write(SERVO_IDLE_ANGLE);
  }
  
  // Control buzzer alarm (manual or fire emergency)
  if (buzzerCommand == "ON") {
    digitalWrite(BUZZER_PIN, HIGH);  // Buzzer ON
  } else {
    digitalWrite(BUZZER_PIN, LOW);   // Buzzer OFF
  }
}

// ========================================
// SERVO ROTATION HANDLER (STATE2)
// ========================================

void updateServoRotation() {
  // Move servo continuously
  servoAngle += rotationDirection * SERVO_ROTATION_STEP;
  
  // Check if reached or exceeded boundaries and reverse direction
  if (servoAngle >= SERVO_MAX_ANGLE) {
    servoAngle = SERVO_MAX_ANGLE;  // Ensure it reaches exactly 180
    rotationDirection = -1;         // Reverse to go back
  } else if (servoAngle <= SERVO_MIN_ANGLE) {
    servoAngle = SERVO_MIN_ANGLE;  // Ensure it reaches exactly 0
    rotationDirection = 1;          // Reverse to go forward
  }
  
  servoMotor.write(servoAngle);
}

// ========================================
// FIRE EMERGENCY HANDLER
// ========================================

void handleFireEmergency() {
  // FIRE DETECTED - Emergency protocol activated!
  Serial.println("\n🚨🚨🚨 FIRE EMERGENCY DETECTED! 🚨🚨🚨");
  
  // 1. Turn ON fire pump relay (D3)
  digitalWrite(FIRE_RELAY_PIN, HIGH);
  Serial.println("[FIRE] Fire pump relay ACTIVATED");
  
  // 2. Turn ON buzzer alarm (D5)
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("[FIRE] Buzzer alarm ACTIVATED");
  
  // 3. Set servo to STATE2 (rotating) for wide coverage
  servoCommand = "STATE2";
  servoRotating = true;
  Serial.println("[FIRE] Servo set to STATE2 (rotating)");
  
  // 4. Also turn ON main irrigation pump for maximum water output
  digitalWrite(PUMP_PIN, HIGH);
  Serial.println("[FIRE] Main pump ACTIVATED");
  
  Serial.println("[FIRE] All fire suppression systems ACTIVE\n");
}

// ========================================
// OPTIONAL: ANALOG MULTIPLEXER SUPPORT
// ========================================
// Uncomment and configure if using CD4051 or similar multiplexer
// for reading multiple analog sensors with single A0 pin

/*
#define MUX_S0 D5
#define MUX_S1 D6
#define MUX_S2 D7

void setupMultiplexer() {
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
}

int readAnalogMultiplexer(int channel) {
  // Set multiplexer channel (0-7)
  digitalWrite(MUX_S0, bitRead(channel, 0));
  digitalWrite(MUX_S1, bitRead(channel, 1));
  digitalWrite(MUX_S2, bitRead(channel, 2));
  
  delayMicroseconds(100);  // Allow signal to settle
  
  int rawValue = analogRead(A0);
  int percentage = map(rawValue, SENSOR_DRY_VALUE, SENSOR_WET_VALUE, 0, 100);
  return constrain(percentage, 0, 100);
}
*/

# 🌳 IoT Smart Tree Irrigation and Monitoring System

A complete IoT-based irrigation system with automatic and manual control modes, real-time monitoring, and web-based interface.

## 📋 Features

### Hardware Control
- **Dual Moisture Sensors**: Monitor two separate fields
  - Sensor 1: Continuous irrigation field (static servo)
  - Sensor 2: Rotating irrigation field (dynamic servo)
- **Smart Pump Control**: Automatic or manual operation
- **Servo Motor**: Two operational states
  - State 1: 180° right (static position)
  - State 2: Rotating between 0-160°

### Software Features
- **Web-based Dashboard**: Real-time monitoring and control
- **Two Control Modes**:
  - **Auto Mode**: Intelligent irrigation based on moisture levels
  - **Manual Mode**: Direct control of pump and servo
- **Priority System**: Field 1 always has priority over Field 2
- **Real-time Data**: Live sensor readings and system status

## 🛠️ Hardware Requirements

- ESP8266 (NodeMCU or Wemos D1 Mini)
- 2x Moisture Sensors (analog/digital)
- 1x Water Pump
- 1x Relay Module (for pump control)
- 1x Servo Motor (for water flow direction)
- Power supply (5V for ESP8266, appropriate voltage for pump)
- Jumper wires

### Pin Connections (ESP8266)

```
Sensor 1 (Analog)    -> A0
Sensor 2 (Digital)   -> D0
Pump Relay           -> D1
Servo Motor          -> D2
```

**Note**: ESP8266 has only one analog pin. For a second analog sensor, use:
1. Digital moisture sensor, or
2. Analog multiplexer (CD4051), or
3. Second ESP8266

## 💻 Software Requirements

- Python 3.8+
- Arduino IDE (for ESP8266 programming)
- ESP8266 Board Package
- Required Arduino Libraries:
  - ESP8266WiFi
  - ESP8266HTTPClient
  - ArduinoJson
  - Servo

## 🚀 Installation & Setup

### 1. Python Server Setup (Local Testing)

```bash
# Clone or navigate to the project directory
cd Iot_smart_Tree_irrigation_and-monitoring_system

# Install dependencies
pip install -r requirements.txt

# Run the server
python app.py
```

The server will start at `http://localhost:5000`

### 2. Deploy to Render.com

1. **Create a GitHub Repository**:
   ```bash
   git init
   git add .
   git commit -m "Initial commit"
   git branch -M main
   git remote add origin <your-github-repo-url>
   git push -u origin main
   ```

2. **Deploy on Render.com**:
   - Go to [render.com](https://render.com) and sign up/login
   - Click "New" → "Web Service"
   - Connect your GitHub repository
   - Configure:
     - **Name**: Your app name (e.g., `smart-tree-irrigation`)
     - **Environment**: Python 3
     - **Build Command**: `pip install -r requirements.txt`
     - **Start Command**: `gunicorn app:app`
     - **Instance Type**: Free
   - Click "Create Web Service"
   - Wait for deployment (5-10 minutes)
   - Note your app URL: `https://your-app-name.onrender.com`

### 3. ESP8266 Setup

1. **Install Arduino IDE** and ESP8266 board support:
   - Open Arduino IDE
   - Go to File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     http://arduino.esp8266.com/stable/package_esp8266com_index.json
     ```
   - Go to Tools → Board → Board Manager
   - Search for "ESP8266" and install

2. **Install Required Libraries**:
   - Tools → Manage Libraries
   - Install: ArduinoJson (by Benoit Blanchon)
   - Servo library is built-in

3. **Configure the Code**:
   - Open `esp8266_irrigation_system.ino`
   - Update WiFi credentials (already set):
     ```cpp
     const char* ssid = "SOFI";
     const char* password = "12345678";
     ```
   - Update server URL to your Render.com URL:
     ```cpp
     const char* serverUrl = "https://your-app-name.onrender.com";
     ```

4. **Upload to ESP8266**:
   - Connect ESP8266 via USB
   - Tools → Board → Select your ESP8266 board
   - Tools → Port → Select the correct COM port
   - Click Upload button
   - Open Serial Monitor (115200 baud) to view status

## 📱 Using the System

### Web Interface

Access the dashboard at:
- Local: `http://localhost:5000`
- Production: `https://your-app-name.onrender.com`

### Control Buttons

**Mode Control** (4 buttons):
1. **Start Manual Mode** - Activate manual control
2. **Stop Manual Mode** - Deactivate manual control
3. **Start Auto Mode** - Activate automatic irrigation
4. **Stop Auto Mode** - Deactivate automatic mode

**Manual Controls** (4 buttons - only work in Manual Mode):
1. **Pump ON** - Turn water pump on
2. **Pump OFF** - Turn water pump off
3. **State 1 (180° Static)** - Set servo to Field 1 position
4. **State 2 (Rotating)** - Set servo to Field 2 rotating mode

### Automatic Mode Logic

When Auto Mode is active:

1. **Field 1 Priority**: Always checked first
   - If moisture < 30%: Start pump, servo to 180° (State 1)
   - Continue until moisture ≥ 70%
   
2. **Field 2**: Only operates when Field 1 is satisfied
   - If moisture < 30%: Start pump, servo rotating (State 2)
   - Servo continuously rotates between 0-160° for even distribution
   - Continue until moisture ≥ 70%

3. **Never Simultaneous**: Only one field irrigated at a time

## 🔧 Configuration

### Moisture Thresholds

Edit in `app.py`:
```python
MOISTURE_THRESHOLD_LOW = 30   # Start irrigation
MOISTURE_THRESHOLD_HIGH = 70  # Stop irrigation
```

### Sensor Calibration

Edit in `esp8266_irrigation_system.ino`:
```cpp
// Adjust map() function based on your sensor readings
sensor1Value = map(rawValue1, 1023, 0, 0, 100);
```

### Timing Intervals

Edit in `esp8266_irrigation_system.ino`:
```cpp
const unsigned long sensorInterval = 2000;    // Read sensors every 2s
const unsigned long serverInterval = 5000;    // Send data every 5s
const unsigned long commandInterval = 3000;   // Poll commands every 3s
```

## 📊 API Endpoints

### GET Endpoints
- `GET /` - Web interface
- `GET /api/status` - Get system status
- `GET /api/esp_commands` - ESP8266 polls for commands

### POST Endpoints
- `POST /api/sensor_data` - Receive sensor data
- `POST /api/pump/on` - Turn pump on
- `POST /api/pump/off` - Turn pump off
- `POST /api/servo/state1` - Set servo to State 1
- `POST /api/servo/state2` - Set servo to State 2
- `POST /api/mode/manual/start` - Start manual mode
- `POST /api/mode/manual/stop` - Stop manual mode
- `POST /api/mode/auto/start` - Start auto mode
- `POST /api/mode/auto/stop` - Stop auto mode

## 🐛 Troubleshooting

### ESP8266 Won't Connect to WiFi
- Check WiFi credentials
- Ensure 2.4GHz network (ESP8266 doesn't support 5GHz)
- Check signal strength

### Sensor Readings Are Wrong
- Calibrate sensors (adjust map() function)
- Check sensor connections
- Verify power supply

### Server Not Responding
- Check Render.com deployment status
- Verify ESP8266 has correct server URL
- Check CORS settings if accessing from different domain

### Pump/Servo Not Working
- Check relay connections
- Verify power supply is adequate
- Check pin assignments
- Test with Serial Monitor output

## 📝 File Structure

```
Iot_smart_Tree_irrigation_and-monitoring_system/
├── app.py                          # Flask server
├── requirements.txt                # Python dependencies
├── esp8266_irrigation_system.ino   # ESP8266 code
├── templates/
│   └── index.html                  # Web UI
└── README.md                       # This file
```

## 🔐 Security Notes

- Change default WiFi credentials in production
- Use HTTPS for production deployment (Render.com provides this)
- Consider adding authentication for web interface
- Keep API endpoints secured

## 📄 License

This project is open source and available for educational and personal use.

## 👨‍💻 Support

For issues or questions:
1. Check the Serial Monitor output on ESP8266
2. Check Render.com logs for server issues
3. Verify all connections and configurations

## 🎯 Future Enhancements

- [ ] Add user authentication
- [ ] SMS/Email notifications
- [ ] Data logging and analytics
- [ ] Weather API integration
- [ ] Mobile app (React Native)
- [ ] Multiple field support
- [ ] Soil pH monitoring
- [ ] Fertilizer control

---

**Created for IoT Smart Tree Irrigation System** 🌳💧

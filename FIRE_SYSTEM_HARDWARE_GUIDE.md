# 🔥 Fire Detection System - Hardware Connection Guide

## Overview
Your system now includes a complete fire detection and suppression system with automatic emergency response.

---

## 📋 Required Hardware Components

### Fire Detection Components:
1. **JQC-3FF-S-Z Relay Module** (for fire pump control)
   - 1-channel relay module
   - Operating voltage: 5V DC
   - Contact rating: 10A 250VAC / 10A 30VDC

2. **Flame Sensor Module**
   - Digital flame/IR sensor
   - Detection angle: ~60 degrees
   - Detection distance: 80cm - 100cm
   - Output: Digital (LOW when flame detected)

3. **Buzzer Alarm**
   - Active buzzer module (5V)
   - Sound level: 85dB or higher
   - Continuous tone type

4. **Fire Pump** (connected to relay)
   - Water pump for fire suppression
   - Must match relay rating (max 10A)

---

## 🔌 Pin Connections on ESP8266 (NodeMCU)

### Complete Pin Assignment:

| Component              | ESP8266 Pin | Pin Label | Function                    |
|------------------------|-------------|-----------|------------------------------|
| Moisture Sensor 1      | A0          | A0        | Analog moisture reading      |
| Moisture Sensor 2      | GPIO16      | D0        | Digital moisture reading     |
| **Irrigation Pump Relay** | GPIO5    | **D1**    | Main irrigation pump control |
| Servo Motor            | GPIO4       | D2        | Servo motor PWM signal       |
| **FIRE PUMP RELAY**    | GPIO0       | **D3**    | ⚠️ Fire pump control (NEW)  |
| **FLAME SENSOR**       | GPIO2       | **D4**    | ⚠️ Fire detection input (NEW) |
| **BUZZER ALARM**       | GPIO14      | **D5**    | ⚠️ Alarm sound output (NEW) |

---

## 🔧 Detailed Wiring Instructions

### 1️⃣ Fire Pump Relay (D3 - GPIO0)

**JQC-3FF-S-Z Relay Module:**

```
ESP8266 D3 (GPIO0)  →  Relay Module IN (Signal Input)
ESP8266 GND         →  Relay Module GND
ESP8266 5V/VIN      →  Relay Module VCC (5V)

Fire Pump Connection:
Power Source (+)    →  Relay COM (Common)
Relay NO (Normally Open) → Fire Pump (+)
Fire Pump (-)       →  Power Source (-)
```

**Important Notes:**
- Use **NO (Normally Open)** terminal - pump is OFF when no signal
- Ensure pump current is within relay rating (10A max)
- For higher current pumps, use this relay to trigger a contactor
- D3 (GPIO0) HIGH = Relay ON = Pump ON

---

### 2️⃣ Flame Sensor (D4 - GPIO2)

**Flame Sensor Module Wiring:**

```
Flame Sensor VCC  →  ESP8266 3.3V or 5V
Flame Sensor GND  →  ESP8266 GND
Flame Sensor D0   →  ESP8266 D4 (GPIO2)
```

**Sensor Behavior:**
- **No Fire:** D0 output = HIGH (3.3V/5V)
- **Fire Detected:** D0 output = LOW (0V) ⚠️

**Sensitivity Adjustment:**
- Rotate the potentiometer on sensor module
- Clockwise = More sensitive (detects from farther)
- Counter-clockwise = Less sensitive
- Test with a lighter to verify detection distance

**Mounting Tips:**
- Mount sensor pointing toward area to monitor
- Keep sensor away from direct sunlight (can cause false triggers)
- Recommended height: 1-2 meters above ground
- Clear line of sight to monitored area

---

### 3️⃣ Buzzer Alarm (D5 - GPIO14)

**Active Buzzer Module Wiring:**

```
Buzzer (+) or VCC  →  ESP8266 D5 (GPIO14)
Buzzer (-) or GND  →  ESP8266 GND
```

**For Louder Alarm (Optional):**
If you need louder alarm, add a transistor driver:

```
ESP8266 D5 → 1kΩ Resistor → NPN Transistor Base (e.g., 2N2222)
Transistor Emitter → GND
Transistor Collector → Buzzer (-)
Buzzer (+) → 5V
```

**Buzzer Behavior:**
- D5 HIGH = Buzzer ON (alarm sounding)
- D5 LOW = Buzzer OFF (silent)

---

## ⚡ Power Supply Considerations

### Power Requirements:
- ESP8266: ~80mA (170mA peak during WiFi)
- Servo Motor: 100-500mA (depending on load)
- Relays (2x): ~70mA each
- Buzzer: ~30mA
- Sensors: ~20mA total

**Total: ~400-800mA**

### Recommended Setup:
1. **Option 1: USB Power (5V 2A)**
   - Use quality 5V 2A USB adapter
   - Good for testing and light loads
   - NodeMCU has onboard voltage regulator

2. **Option 2: External 5V Power Supply**
   - Use 5V 3A DC power supply
   - Connect to VIN and GND pins
   - More stable for heavier loads

3. **Pump Power:**
   - **DO NOT** power pumps directly from ESP8266
   - Use separate 12V/24V power supply for pumps
   - Only relay control signal comes from ESP8266

---

## 🚨 Fire Emergency Operation

### Automatic Fire Response Sequence:

When flame sensor detects fire (D4 goes LOW):

1. **Immediate Actions (within milliseconds):**
   - ✅ Fire pump relay activates (D3 HIGH)
   - ✅ Main irrigation pump activates (D1 HIGH)
   - ✅ Buzzer alarm sounds (D5 HIGH)
   - ✅ Servo switches to STATE2 (rotating 0-180°)

2. **System Override:**
   - 🔥 Fire mode **overrides all other modes** (manual/auto)
   - 🔥 Cannot be turned off manually while fire detected
   - 🔥 UI shows prominent red fire alert modal
   - 🔥 Mode displays as "FIRE_EMERGENCY"

3. **Automatic Recovery:**
   - When flame sensor no longer detects fire (D4 goes HIGH)
   - All fire systems automatically turn OFF
   - System returns to previous mode (manual/auto)
   - Normal irrigation operation resumes

---

## 🧪 Testing Procedure

### Initial Testing (IMPORTANT!):

1. **Individual Component Test:**
   ```
   - Power on ESP8266 without connecting pumps
   - Open Serial Monitor (115200 baud)
   - Watch sensor readings
   ```

2. **Flame Sensor Test:**
   ```
   - Use a lighter 30-50cm away from sensor
   - Serial monitor should show: "Fire Detected: YES ⚠️"
   - Status should change when lighter is removed
   ```

3. **Relay Test:**
   ```
   - Listen for relay "click" sound when fire detected
   - Check relay LED indicator (should light up)
   - Verify NO terminal has continuity to COM when fire detected
   ```

4. **Buzzer Test:**
   ```
   - Buzzer should sound immediately when fire detected
   - Sound should stop when fire cleared
   - Check volume is adequate for your needs
   ```

5. **Full System Test:**
   ```
   - Connect fire pump to relay
   - Trigger flame sensor
   - Verify:
     ✓ Fire pump turns ON
     ✓ Irrigation pump turns ON
     ✓ Buzzer sounds
     ✓ Servo starts rotating
     ✓ Web UI shows fire alert
   ```

---

## 🛡️ Safety Guidelines

### ⚠️ IMPORTANT SAFETY WARNINGS:

1. **Electrical Safety:**
   - ⚡ Never work on circuit while powered
   - ⚡ Check all connections before applying power
   - ⚡ Use appropriate wire gauge for pump current
   - ⚡ Install fuse protection on pump circuits

2. **Water Safety:**
   - 💧 Keep ESP8266 and relays in waterproof enclosure
   - 💧 Use IP65+ rated junction boxes
   - 💧 Seal all cable entries
   - 💧 Test for leaks before permanent installation

3. **Fire System Testing:**
   - 🔥 Test fire system weekly
   - 🔥 Ensure adequate water supply for fire pump
   - 🔥 Check sensor alignment and cleanliness
   - 🔥 Verify pumps can actually reach fire area

4. **False Alarm Prevention:**
   - Adjust sensor sensitivity properly
   - Shield sensor from direct sunlight
   - Keep sensor lens clean
   - Consider adding 2-3 second confirmation delay in code

---

## 📊 System Features Summary

### Fire Detection System Capabilities:

✅ **Automatic Fire Detection**
- Flame sensor monitors 24/7
- ~80cm detection range
- 60° field of view

✅ **Emergency Response**
- Activates within milliseconds
- Dual pump system (irrigation + fire pump)
- Rotating servo for wide water coverage
- Loud buzzer alarm

✅ **Smart Override**
- Overrides manual/auto irrigation modes
- Cannot be manually disabled during fire
- Automatic return to normal when fire cleared

✅ **Web UI Integration**
- Real-time fire status display
- Prominent fire emergency alert modal
- Shows fire pump and buzzer status
- Visible fire alarm on all devices

✅ **Data Logging**
- Fire events sent to server
- Timestamp of fire detection
- Duration of emergency mode

---

## 🔍 Troubleshooting

### Problem: Flame sensor always shows fire detected
**Solution:**
- Adjust sensitivity potentiometer (turn counter-clockwise)
- Check if sensor is facing bright light source
- Verify sensor VCC is 3.3V or 5V (not both)

### Problem: Buzzer doesn't sound
**Solution:**
- Verify buzzer polarity (+ to D5, - to GND)
- Check if active buzzer (has internal oscillator)
- Test buzzer directly with 5V to confirm working
- Consider transistor driver for more current

### Problem: Fire pump relay doesn't activate
**Solution:**
- Check D3 connection to relay IN pin
- Verify relay has 5V power
- Test relay manually by connecting IN to 5V
- Check relay LED indicator

### Problem: False fire alarms
**Solution:**
- Reduce sensor sensitivity
- Shield sensor from sunlight
- Clean sensor lens
- Move sensor away from heat sources
- Add 2-3 second confirmation delay in code

---

## 📸 Connection Diagram Summary

```
┌─────────────────────────────────────────────────────┐
│                    ESP8266 NodeMCU                  │
│                                                     │
│  A0  ← Moisture Sensor 1 (Analog)                 │
│  D0  ← Moisture Sensor 2 (Digital)                │
│  D1  → Irrigation Pump Relay                       │
│  D2  → Servo Motor (PWM)                           │
│  D3  → 🔥 Fire Pump Relay (NEW)                   │
│  D4  ← 🔥 Flame Sensor (NEW)                      │
│  D5  → 🔥 Buzzer Alarm (NEW)                      │
│                                                     │
│  VIN ← 5V Power Supply                             │
│  GND ← Ground (Common)                             │
└─────────────────────────────────────────────────────┘
```

---

## 📞 Support

If you encounter any issues:
1. Check Serial Monitor output for debug info
2. Verify all pin connections match this guide
3. Test each component individually
4. Check power supply voltage and current
5. Review ESP8266 code comments for details

---

## ✅ Hardware Checklist

Before powering on, verify:

- [ ] All ground connections are common
- [ ] Power supply can handle total current
- [ ] Relay contacts rated for pump load
- [ ] Flame sensor sensitivity adjusted
- [ ] Buzzer polarity correct
- [ ] Servo has separate power if needed
- [ ] Water supply connected to both pumps
- [ ] ESP8266 and relays in waterproof box
- [ ] Fire sensor has clear view of area
- [ ] All wire connections are secure
- [ ] Fuse protection on pump circuits
- [ ] Test button or lighter ready for testing

---

**⚠️ FINAL NOTE:** This is a supplementary fire detection system. It should NOT replace proper fire safety equipment, fire extinguishers, smoke detectors, or professional fire suppression systems. Always follow local fire safety codes and regulations.

**🚀 You're now ready to connect the fire detection hardware!**

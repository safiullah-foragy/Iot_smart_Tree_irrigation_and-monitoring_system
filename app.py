from flask import Flask, jsonify, request, render_template
from flask_cors import CORS
from datetime import datetime, timedelta
import threading
import time

app = Flask(__name__)
CORS(app)

# Global state variables
system_state = {
    'sensor1_moisture': 0,
    'sensor2_dry': False,  # DO sensor: True=Dry, False=Wet
    'pump_status': 'OFF',
    'servo_state': 'IDLE',  # IDLE, STATE1 (180 deg), STATE2 (rotating)
    'control_mode': 'MANUAL',  # MANUAL or AUTO
    'last_update': datetime.now().isoformat(),
    'last_esp_update': datetime.now(),  # Track ESP8266 last communication
    'esp_connected': True,  # Connection status
    'auto_mode_active': False,
    'manual_mode_active': True,
    'field1_active': False,
    'field2_active': False,
    'fire_detected': False,  # Fire detection status from sensor
    'fire_relay_status': 'OFF',  # Fire pump relay status
    'buzzer_status': 'OFF',  # Buzzer alarm status
    'manual_alarm_active': False,  # Manual alarm button state
    'fire_emergency_mode': 'OFF',  # OFF, AUTO, MANUAL
    'fire_mode_active': False  # True when fire emergency is active (either auto or manual)
}

# ESP8266 connection timeout (seconds)
ESP_TIMEOUT = 15  # Consider offline if no data for 15 seconds

# Thresholds for auto mode
MOISTURE_THRESHOLD_LOW = 30   # Below this, irrigation needed (emergency)
MOISTURE_THRESHOLD_HIGH = 80  # Above this, irrigation sufficient (stop)

# Lock for thread-safe operations
state_lock = threading.Lock()

@app.route('/')
def index():
    """Serve the main web UI"""
    return render_template('index.html')

@app.route('/api/status', methods=['GET'])
def get_status():
    """Get current system status"""
    with state_lock:
        # Check if ESP8266 is still connected
        time_since_update = (datetime.now() - system_state['last_esp_update']).total_seconds()
        system_state['esp_connected'] = time_since_update < ESP_TIMEOUT
        
        # Create response with connection status
        response = system_state.copy()
        response['last_esp_update'] = system_state['last_esp_update'].isoformat()
        response['seconds_since_update'] = int(time_since_update)
        
        return jsonify(response)

@app.route('/api/sensor_data', methods=['POST'])
def receive_sensor_data():
    """Receive sensor data from ESP8266"""
    try:
        data = request.get_json()
        
        with state_lock:
            system_state['sensor1_moisture'] = data.get('sensor1', 0)
            system_state['sensor2_dry'] = data.get('sensor2_dry', False)  # Boolean: True=Dry, False=Wet
            system_state['fire_detected'] = data.get('fire_detected', False)
            system_state['last_update'] = datetime.now().isoformat()
            system_state['last_esp_update'] = datetime.now()  # Update ESP connection timestamp
            system_state['esp_connected'] = True  # Mark as connected
        
        # Check fire emergency conditions
        if system_state['fire_emergency_mode'] == 'AUTO' and system_state['fire_detected']:
            # AUTO mode: Fire detected by sensor - activate fire emergency
            activate_fire_emergency()
        elif system_state['fire_emergency_mode'] == 'AUTO' and not system_state['fire_detected']:
            # AUTO mode: Fire cleared - deactivate fire emergency
            if system_state['fire_mode_active']:
                deactivate_fire_emergency()
        elif system_state['fire_emergency_mode'] == 'MANUAL':
            # MANUAL mode: Keep fire emergency active regardless of sensor
            activate_fire_emergency()
        elif system_state['fire_emergency_mode'] == 'OFF':
            # Fire mode OFF: Deactivate if active
            if system_state['fire_mode_active']:
                deactivate_fire_emergency()
            # Run normal irrigation if auto mode active
            if system_state['auto_mode_active']:
                auto_control_logic()
        
        return jsonify({'status': 'success', 'message': 'Sensor data received'})
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)}), 400

@app.route('/api/pump/on', methods=['POST'])
def pump_on():
    """Manually turn pump ON (only in manual mode)"""
    with state_lock:
        if system_state['manual_mode_active']:
            system_state['pump_status'] = 'ON'
            return jsonify({'status': 'success', 'message': 'Pump turned ON'})
        else:
            return jsonify({'status': 'error', 'message': 'Manual mode is not active'}), 403

@app.route('/api/pump/off', methods=['POST'])
def pump_off():
    """Manually turn pump OFF (only in manual mode)"""
    with state_lock:
        if system_state['manual_mode_active']:
            system_state['pump_status'] = 'OFF'
            system_state['servo_state'] = 'IDLE'
            return jsonify({'status': 'success', 'message': 'Pump turned OFF'})
        else:
            return jsonify({'status': 'error', 'message': 'Manual mode is not active'}), 403

@app.route('/api/servo/state1', methods=['POST'])
def servo_state1():
    """Set servo to State 1 - 180 degrees right (only in manual mode)"""
    with state_lock:
        if system_state['manual_mode_active']:
            system_state['servo_state'] = 'STATE1'
            return jsonify({'status': 'success', 'message': 'Servo set to State 1 (180° right)'})
        else:
            return jsonify({'status': 'error', 'message': 'Manual mode is not active'}), 403

@app.route('/api/servo/state2', methods=['POST'])
def servo_state2():
    """Set servo to State 2 - Rotating 0-160 degrees (only in manual mode)"""
    with state_lock:
        if system_state['manual_mode_active']:
            system_state['servo_state'] = 'STATE2'
            return jsonify({'status': 'success', 'message': 'Servo set to State 2 (rotating)'})
        else:
            return jsonify({'status': 'error', 'message': 'Manual mode is not active'}), 403

@app.route('/api/servo/idle', methods=['POST'])
def servo_idle():
    """Set servo to Idle position (only in manual mode)"""
    with state_lock:
        if system_state['manual_mode_active']:
            system_state['servo_state'] = 'IDLE'
            return jsonify({'status': 'success', 'message': 'Servo set to Idle (90° center)'})
        else:
            return jsonify({'status': 'error', 'message': 'Manual mode is not active'}), 403

@app.route('/api/servo/active', methods=['POST'])
def servo_active():
    """Activate servo (same as State 1 - only in manual mode)"""
    with state_lock:
        if system_state['manual_mode_active']:
            system_state['servo_state'] = 'STATE1'
            return jsonify({'status': 'success', 'message': 'Servo activated (180° right)'})
        else:
            return jsonify({'status': 'error', 'message': 'Manual mode is not active'}), 403

@app.route('/api/mode/manual/start', methods=['POST'])
def start_manual_mode():
    """Activate manual control mode"""
    with state_lock:
        if not system_state['auto_mode_active']:
            system_state['manual_mode_active'] = True
            system_state['control_mode'] = 'MANUAL'
            return jsonify({'status': 'success', 'message': 'Manual mode activated'})
        else:
            return jsonify({'status': 'error', 'message': 'Please deactivate auto mode first'}), 403

@app.route('/api/mode/manual/stop', methods=['POST'])
def stop_manual_mode():
    """Deactivate manual control mode"""
    with state_lock:
        system_state['manual_mode_active'] = False
        system_state['pump_status'] = 'OFF'
        system_state['servo_state'] = 'IDLE'
        return jsonify({'status': 'success', 'message': 'Manual mode deactivated'})

@app.route('/api/mode/auto/start', methods=['POST'])
def start_auto_mode():
    """Activate automatic control mode"""
    with state_lock:
        if not system_state['manual_mode_active']:
            system_state['auto_mode_active'] = True
            system_state['control_mode'] = 'AUTO'
            return jsonify({'status': 'success', 'message': 'Auto mode activated'})
        else:
            return jsonify({'status': 'error', 'message': 'Please deactivate manual mode first'}), 403

@app.route('/api/mode/auto/stop', methods=['POST'])
def stop_auto_mode():
    """Deactivate automatic control mode"""
    with state_lock:
        system_state['auto_mode_active'] = False
        system_state['pump_status'] = 'OFF'
        system_state['servo_state'] = 'IDLE'
        system_state['field1_active'] = False
        system_state['field2_active'] = False
        return jsonify({'status': 'success', 'message': 'Auto mode deactivated'})

def auto_control_logic():
    """
    Automatic control logic for irrigation system.
    - Field 1 (A0 analog sensor): Below 50% start irrigation with rotating servo (10-180°), above 50% stop
    - Field 2 (DO digital sensor): If DRY start irrigation with static servo (0°), if WET stop
    - Field 1 has priority when both need irrigation
    """
    with state_lock:
        sensor1 = system_state['sensor1_moisture']
        sensor2_is_dry = system_state['sensor2_dry']
        
        # Check which sensors need irrigation
        sensor1_needs_water = sensor1 < 50  # Below 50% needs irrigation
        
        # Priority logic: Field 1 (A0) takes priority over Field 2 (DO)
        if sensor1_needs_water:
            # Field 1: A0 sensor below 50% - Pump ON + Servo rotating 10-180°
            system_state['pump_status'] = 'ON'
            system_state['servo_state'] = 'STATE1'
            system_state['field1_active'] = True
            system_state['field2_active'] = False
            
        elif sensor2_is_dry:
            # Field 2: DO sensor DRY - Pump ON + Servo static at 0°
            system_state['pump_status'] = 'ON'
            system_state['servo_state'] = 'STATE2'
            system_state['field1_active'] = False
            system_state['field2_active'] = True
            
        elif sensor1 >= 50 and not sensor2_is_dry:
            # Both conditions satisfied: A0 >= 50% AND DO is WET - Everything OFF
            system_state['pump_status'] = 'OFF'
            system_state['servo_state'] = 'IDLE'
            system_state['field1_active'] = False
            system_state['field2_active'] = False
            
        elif system_state['field1_active'] and sensor1 >= 50:
            # Field 1 reached 50% - Turn OFF
            system_state['pump_status'] = 'OFF'
            system_state['servo_state'] = 'IDLE'
            system_state['field1_active'] = False
            
        elif system_state['field2_active'] and not sensor2_is_dry:
            # Field 2 is now WET - Turn OFF
            system_state['pump_status'] = 'OFF'
            system_state['servo_state'] = 'IDLE'
            system_state['field2_active'] = False

def activate_fire_emergency():
    """
    Activate fire emergency mode.
    Stops all irrigation activities and activates fire suppression.
    """
    with state_lock:
        # Mark fire mode as active
        system_state['fire_mode_active'] = True
        
        # Stop all irrigation activities
        system_state['field1_active'] = False
        system_state['field2_active'] = False
        
        # Activate fire suppression systems
        system_state['fire_relay_status'] = 'ON'  # Fire pump ON
        system_state['pump_status'] = 'ON'  # Main pump also ON for water
        system_state['servo_state'] = 'STATE2'  # Servo rotating for wide coverage
        system_state['buzzer_status'] = 'ON'  # Buzzer alarm ON
        system_state['control_mode'] = 'FIRE_EMERGENCY'  # Override mode

def deactivate_fire_emergency():
    """Deactivate fire emergency and return to normal operation"""
    with state_lock:
        # Mark fire mode as inactive
        system_state['fire_mode_active'] = False
        system_state['fire_emergency_mode'] = 'OFF'
        
        # Turn off all fire systems
        system_state['fire_relay_status'] = 'OFF'
        system_state['pump_status'] = 'OFF'
        system_state['servo_state'] = 'IDLE'
        system_state['buzzer_status'] = 'OFF'
        
        # Return to previous mode
        if system_state['manual_mode_active']:
            system_state['control_mode'] = 'MANUAL'
        elif system_state['auto_mode_active']:
            system_state['control_mode'] = 'AUTO'
        else:
            system_state['control_mode'] = 'MANUAL'

@app.route('/api/fire/mode/auto', methods=['POST'])
def fire_mode_auto():
    """Activate AUTO fire emergency mode - responds to fire sensor"""
    with state_lock:
        system_state['fire_emergency_mode'] = 'AUTO'
        # If fire already detected, activate immediately
        if system_state['fire_detected']:
            activate_fire_emergency()
        return jsonify({'status': 'success', 'message': 'Fire emergency AUTO mode activated'})

@app.route('/api/fire/mode/manual', methods=['POST'])
def fire_mode_manual():
    """Activate MANUAL fire emergency mode - always active"""
    with state_lock:
        system_state['fire_emergency_mode'] = 'MANUAL'
        activate_fire_emergency()
        return jsonify({'status': 'success', 'message': 'Fire emergency MANUAL mode activated'})

@app.route('/api/fire/mode/off', methods=['POST'])
def fire_mode_off():
    """Deactivate fire emergency mode"""
    with state_lock:
        system_state['fire_emergency_mode'] = 'OFF'
        if system_state['fire_mode_active']:
            deactivate_fire_emergency()
        return jsonify({'status': 'success', 'message': 'Fire emergency mode deactivated'})

@app.route('/api/alarm/on', methods=['POST'])
def alarm_on():
    """Manually turn alarm ON"""
    with state_lock:
        system_state['manual_alarm_active'] = True
        system_state['buzzer_status'] = 'ON'
        return jsonify({'status': 'success', 'message': 'Alarm activated'})

@app.route('/api/alarm/off', methods=['POST'])
def alarm_off():
    """Manually turn alarm OFF (only if no fire)"""
    with state_lock:
        if system_state['fire_detected']:
            return jsonify({'status': 'error', 'message': 'Cannot disable alarm during fire emergency'}), 403
        
        system_state['manual_alarm_active'] = False
        system_state['buzzer_status'] = 'OFF'
        return jsonify({'status': 'success', 'message': 'Alarm deactivated'})

@app.route('/api/esp_commands', methods=['GET'])
def get_esp_commands():
    """ESP8266 polls this endpoint to get commands"""
    with state_lock:
        # Update ESP connection timestamp when it polls for commands
        system_state['last_esp_update'] = datetime.now()
        system_state['esp_connected'] = True
        
        commands = {
            'pump': system_state['pump_status'],
            'servo': system_state['servo_state'],
            'buzzer': system_state['buzzer_status'],
            'timestamp': datetime.now().isoformat()
        }
    return jsonify(commands)

if __name__ == '__main__':
    # For local testing
    app.run(host='0.0.0.0', port=5000, debug=True)

from flask import Flask, jsonify, request, render_template
from flask_cors import CORS
from datetime import datetime
import threading
import time

app = Flask(__name__)
CORS(app)

# Global state variables
system_state = {
    'sensor1_moisture': 0,
    'sensor2_moisture': 0,
    'pump_status': 'OFF',
    'servo_state': 'IDLE',  # IDLE, STATE1 (180 deg), STATE2 (rotating)
    'control_mode': 'MANUAL',  # MANUAL or AUTO
    'last_update': datetime.now().isoformat(),
    'auto_mode_active': False,
    'manual_mode_active': True,
    'field1_active': False,
    'field2_active': False
}

# Thresholds for auto mode
MOISTURE_THRESHOLD_LOW = 30  # Below this, irrigation needed
MOISTURE_THRESHOLD_HIGH = 70  # Above this, irrigation sufficient

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
        return jsonify(system_state)

@app.route('/api/sensor_data', methods=['POST'])
def receive_sensor_data():
    """Receive sensor data from ESP8266"""
    try:
        data = request.get_json()
        
        with state_lock:
            system_state['sensor1_moisture'] = data.get('sensor1', 0)
            system_state['sensor2_moisture'] = data.get('sensor2', 0)
            system_state['last_update'] = datetime.now().isoformat()
        
        # If auto mode is active, process the logic
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
    Priority: Sensor 1 (Field 1) -> Sensor 2 (Field 2)
    """
    with state_lock:
        sensor1 = system_state['sensor1_moisture']
        sensor2 = system_state['sensor2_moisture']
        
        # Field 1 (Sensor 1) - Continuous irrigation with static servo
        if sensor1 < MOISTURE_THRESHOLD_LOW and not system_state['field2_active']:
            # Start irrigation for Field 1
            system_state['pump_status'] = 'ON'
            system_state['servo_state'] = 'STATE1'  # 180 degrees right
            system_state['field1_active'] = True
        elif sensor1 >= MOISTURE_THRESHOLD_HIGH and system_state['field1_active']:
            # Stop irrigation for Field 1
            system_state['pump_status'] = 'OFF'
            system_state['servo_state'] = 'IDLE'
            system_state['field1_active'] = False
        
        # Field 2 (Sensor 2) - Rotating irrigation
        # Only if Field 1 is not active
        if not system_state['field1_active']:
            if sensor2 < MOISTURE_THRESHOLD_LOW:
                # Start irrigation for Field 2
                system_state['pump_status'] = 'ON'
                system_state['servo_state'] = 'STATE2'  # Rotating
                system_state['field2_active'] = True
            elif sensor2 >= MOISTURE_THRESHOLD_HIGH and system_state['field2_active']:
                # Stop irrigation for Field 2
                system_state['pump_status'] = 'OFF'
                system_state['servo_state'] = 'IDLE'
                system_state['field2_active'] = False

@app.route('/api/esp_commands', methods=['GET'])
def get_esp_commands():
    """ESP8266 polls this endpoint to get commands"""
    with state_lock:
        commands = {
            'pump': system_state['pump_status'],
            'servo': system_state['servo_state'],
            'timestamp': datetime.now().isoformat()
        }
    return jsonify(commands)

if __name__ == '__main__':
    # For local testing
    app.run(host='0.0.0.0', port=5000, debug=True)

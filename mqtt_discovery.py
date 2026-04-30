import paho.mqtt.client as mqtt
import json
import time
import threading

# ================== CONFIG ==================
BROKER = "192.168.0.100"
PORT = 1883

connected = False

HEARTBEAT_INTERVAL = 30

# ================== MQTT CLIENT ==================
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.reconnect_delay_set(min_delay=1, max_delay=10)

# ================== CALLBACK ==================
def on_connect(client, userdata, flags, rc, properties=None):
    global connected
    if rc == 0:
        print("[MQTT] Connected")
        connected = True
    else:
        print("[MQTT] Connect failed:", rc)

def on_disconnect(client, userdata, rc, properties=None):
    global connected
    print("[MQTT] Disconnected:", rc)
    connected = False

client.on_connect = on_connect
client.on_disconnect = on_disconnect

# ================== SAFE PUBLISH ==================
def safe_publish(topic, payload, retain=True):
    if not client.is_connected():
        return False

    try:
        result = client.publish(topic, payload, retain=retain)
        return result.rc == mqtt.MQTT_ERR_SUCCESS
    except Exception as e:
        print("[MQTT] Publish error:", e)
        return False

def publish_config(topic, payload):
    safe_publish(topic, json.dumps(payload), retain=True)

# ================== DEVICE INFO ==================
device_espC = {
    "identifiers": ["espC"],
    "name": "ESP C",
    "model": "ESP8266",
    "manufacturer": "Custom"
}

device_espD = {
    "identifiers": ["espD"],
    "name": "ESP D",
    "model": "ESP8266",
    "manufacturer": "Custom"
}

# ================== ESP C ==================
def setup_espC():

    for i in range(1, 4):
        publish_config(
            f"homeassistant/switch/espC_relay{i}/config",
            {
                "name": f"Relay {i}",
                "command_topic": f"espC/relay{i}/set",
                "state_topic": f"espC/relay{i}/state",
                "payload_on": "ON",
                "payload_off": "OFF",
                "unique_id": f"espC_relay{i}",
                "device": device_espC
            }
        )

    publish_config(
        "homeassistant/sensor/espC_temp/config",
        {
            "name": "Temperature",
            "state_topic": "espC/temp",
            "unit_of_measurement": "°C",
            "unique_id": "espC_temp",
            "device": device_espC
        }
    )

    publish_config(
        "homeassistant/sensor/espC_hum/config",
        {
            "name": "Humidity",
            "state_topic": "espC/hum",
            "unit_of_measurement": "%",
            "unique_id": "espC_hum",
            "device": device_espC
        }
    )

    publish_config(
        "homeassistant/switch/espC_lcd_backlight/config",
        {
            "name": "LCD Backlight",
            "command_topic": "espC/lcd/backlight/set",
            "state_topic": "espC/lcd/backlight/state",
            "payload_on": "ON",
            "payload_off": "OFF",
            "unique_id": "espC_lcd_backlight",
            "icon": "mdi:television",
            "device": device_espC
        }
    )

    publish_config(
        "homeassistant/sensor/espC_heartbeat/config",
        {
            "name": "ESP C Heartbeat",
            "state_topic": "espC/heartbeat",
            "unique_id": "espC_heartbeat",
            "device": device_espC,
            "icon": "mdi:heart-pulse"
        }
    )

# ================== ESP D ==================
def setup_espD():

    for i in range(1, 3):
        publish_config(
            f"homeassistant/switch/espD_relay{i}/config",
            {
                "name": f"Relay {i}",
                "command_topic": f"espD/relay{i}/set",
                "state_topic": f"espD/relay{i}/state",
                "payload_on": "ON",
                "payload_off": "OFF",
                "unique_id": f"espD_relay{i}",
                "device": device_espD
            }
        )

    for i in range(1, 3):
        publish_config(
            f"homeassistant/switch/espD_servo{i}/config",
            {
                "name": f"Servo {i}",
                "command_topic": f"espD/servo{i}/set",
                "state_topic": f"espD/servo{i}/state",
                "payload_on": "ON",
                "payload_off": "OFF",
                "unique_id": f"espD_servo{i}",
                "device": device_espD
            }
        )

    publish_config(
        "homeassistant/sensor/espD_temp/config",
        {
            "name": "Temperature",
            "state_topic": "espD/temp",
            "unit_of_measurement": "°C",
            "device_class": "temperature",
            "unique_id": "espD_temp",
            "device": device_espD
        }
    )
    
    publish_config(
        "homeassistant/sensor/espD_hum/config",
        {
            "name": "Humidity",
            "state_topic": "espD/hum",
            "unit_of_measurement": "%",
            "device_class": "humidity",
            "unique_id": "espD_hum",
            "device": device_espD
        }
    )

    publish_config(
        "homeassistant/binary_sensor/espD_motion/config",
        {
            "name": "Motion",
            "state_topic": "espD/motion",
            "payload_on": "1",
            "payload_off": "0",
            "device_class": "motion",
            "unique_id": "espD_motion",
            "device": device_espD
        }
    )

    publish_config(
        "homeassistant/binary_sensor/espD_light/config",
        {
            "name": "Light",
            "state_topic": "espD/light/state",
            "payload_on": "ON",
            "payload_off": "OFF",
            "device_class": "light",
            "unique_id": "espD_light",
            "device": device_espD
        }
    )

    publish_config(
        "homeassistant/sensor/espD_time_of_day/config",
        {
            "name": "Time Of Day",
            "state_topic": "espD/time_of_day",
            "unique_id": "espD_time_of_day",
            "icon": "mdi:clock-outline",
            "device": device_espD
        }
    )

    publish_config(
        "homeassistant/sensor/espD_heartbeat/config",
        {
            "name": "ESP D Heartbeat",
            "state_topic": "espD/heartbeat",
            "unique_id": "espD_heartbeat",
            "device": device_espD,
            "icon": "mdi:heart-pulse"
        }
    )

# ================== HEARTBEAT LOOP ==================
def heartbeat_loop():
    while True:
        if connected:
            ts = str(int(time.time()))

            safe_publish("espC/heartbeat", ts, True)
            safe_publish("espD/heartbeat", ts, True)
            safe_publish("system/mqtt/status", "online", True)

        time.sleep(HEARTBEAT_INTERVAL)

# ================== START ==================
client.connect(BROKER, PORT, 60)
client.loop_start()

while not connected:
    time.sleep(0.1)

setup_espC()
setup_espD()

print("[SYSTEM] MQTT Discovery Ready")

threading.Thread(target=heartbeat_loop, daemon=True).start()

# ================== MAIN LOOP ==================
while True:
    if not client.is_connected():
        try:
            client.reconnect()
        except Exception as e:
            print("[MQTT] reconnect failed:", e)

    time.sleep(5)
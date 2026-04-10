import paho.mqtt.client as mqtt
import json
import time

BROKER = "localhost"
PORT = 1883

client = mqtt.Client()

# ================== CONNECT ==================
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT")

client.on_connect = on_connect
client.connect(BROKER, PORT, 60)

# ================== HELPER ==================
def publish_config(topic, payload):
    client.publish(topic, json.dumps(payload), retain=True)

# ================== ESP C ==================
def setup_espC():
    # Relay 1,2,3
    for i in range(1, 4):
        topic = f"homeassistant/switch/espC_relay{i}/config"
        payload = {
            "name": f"ESP C Relay {i}",
            "command_topic": f"espC/relay{i}/set",
            "state_topic": f"espC/relay{i}/state",
            "payload_on": "ON",
            "payload_off": "OFF",
            "unique_id": f"espC_relay{i}"
        }
        publish_config(topic, payload)

    # Temperature
    publish_config(
        "homeassistant/sensor/espC_temp/config",
        {
            "name": "ESP C Temperature",
            "state_topic": "espC/temp",
            "unit_of_measurement": "°C",
            "unique_id": "espC_temp"
        }
    )

    # Humidity
    publish_config(
        "homeassistant/sensor/espC_hum/config",
        {
            "name": "ESP C Humidity",
            "state_topic": "espC/hum",
            "unit_of_measurement": "%",
            "unique_id": "espC_hum"
        }
    )

# ================== ESP D ==================
def setup_espD():
    # Relay 1,2
    for i in range(1, 3):
        topic = f"homeassistant/switch/espD_relay{i}/config"
        payload = {
            "name": f"ESP D Relay {i}",
            "command_topic": f"espD/relay{i}/set",
            "state_topic": f"espD/relay{i}/state",
            "payload_on": "ON",
            "payload_off": "OFF",
            "unique_id": f"espD_relay{i}"
        }
        publish_config(topic, payload)

    # Servo 1,2 (treat as switch)
    for i in range(1, 3):
        topic = f"homeassistant/switch/espD_servo{i}/config"
        payload = {
            "name": f"ESP D Servo {i}",
            "command_topic": f"espD/servo{i}/set",
            "state_topic": f"espD/servo{i}/state",
            "payload_on": "ON",
            "payload_off": "OFF",
            "unique_id": f"espD_servo{i}"
        }
        publish_config(topic, payload)

    # Motion sensor
    publish_config(
        "homeassistant/binary_sensor/espD_motion/config",
        {
            "name": "ESP D Motion",
            "state_topic": "espD/motion",
            "payload_on": "1",
            "payload_off": "0",
            "device_class": "motion",
            "unique_id": "espD_motion"
        }
    )

    # Light state (read-only)
    publish_config(
        "homeassistant/binary_sensor/espD_light/config",
        {
            "name": "ESP D Light",
            "state_topic": "espD/light/state",
            "payload_on": "ON",
            "payload_off": "OFF",
            "device_class": "light",
            "unique_id": "espD_light"
        }
    )

# ================== MAIN ==================
client.loop_start()

time.sleep(1)

setup_espC()
setup_espD()

print("MQTT Discovery config sent!")

while True:
    time.sleep(10)
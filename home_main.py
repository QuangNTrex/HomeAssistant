import paho.mqtt.client as mqtt
import json
import time

BROKER = "localhost"
PORT = 1883

client = mqtt.Client()

connected = False

# ================== CONNECT ==================
def on_connect(client, userdata, flags, rc):
    global connected
    if rc == 0:
        print("Connected to MQTT")
        connected = True
    else:
        print("MQTT connect failed:", rc)

client.on_connect = on_connect
client.connect(BROKER, PORT, 60)

# ================== HELPER ==================
def publish_config(topic, payload):
    print("Publishing:", topic)
    client.publish(topic, json.dumps(payload), retain=True)

# ================== DEVICE ==================
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

# ================== ESP D ==================
def setup_espD():
    print("=== SETUP ESP D ===")
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

# ================== MAIN ==================
client.loop_start()

# chờ connect thật sự
while not connected:
    time.sleep(0.1)

setup_espC()
setup_espD()

print("MQTT Discovery config sent!")

while True:
    time.sleep(10)
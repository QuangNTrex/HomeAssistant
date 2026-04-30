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

# ================== HEARTBEAT CONFIG ==================
def heartbeat(device):
    return {
        "state_topic": f"{device}/heartbeat",
        "name": "Heartbeat",
        "unique_id": f"{device}_heartbeat",
        "device": device_espC if device == "espC" else device_espD,
        "icon": "mdi:heart-pulse"
    }

# ================== ESP C ==================
def setup_espC():

    # RELAY
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

    # TEMP
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

    # HUM
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

    # LCD BACKLIGHT
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

    # HEARTBEAT (IMPORTANT)
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

    # HEARTBEAT
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


# ================== MAIN ==================
client.loop_start()

while not connected:
    time.sleep(0.1)

setup_espC()
setup_espD()

print("MQTT Discovery config sent!")

while True:
    if not client.is_connected():
        try:
            client.reconnect()
        except:
            pass

    time.sleep(5)
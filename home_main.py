import paho.mqtt.client as mqtt
import json
import time

BROKER = "192.168.0.100"

client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    print("Connected")
    client.subscribe("home/esp_c/#")

    # ===== CREATE ENTITIES =====

    # Relay 1
    client.publish("homeassistant/switch/esp_c_relay1/config", json.dumps({
        "name": "Relay 1",
        "command_topic": "home/esp_c/relay1/set",
        "state_topic": "home/esp_c/relay1/state",
        "payload_on": "ON",
        "payload_off": "OFF",
        "unique_id": "esp_c_relay1"
    }), retain=True)

    # Relay 2
    client.publish("homeassistant/switch/esp_c_relay2/config", json.dumps({
        "name": "Relay 2",
        "command_topic": "home/esp_c/relay2/set",
        "state_topic": "home/esp_c/relay2/state",
        "payload_on": "ON",
        "payload_off": "OFF",
        "unique_id": "esp_c_relay2"
    }), retain=True)

    # Relay 3
    client.publish("homeassistant/switch/esp_c_relay3/config", json.dumps({
        "name": "Relay 3",
        "command_topic": "home/esp_c/relay3/set",
        "state_topic": "home/esp_c/relay3/state",
        "payload_on": "ON",
        "payload_off": "OFF",
        "unique_id": "esp_c_relay3"
    }), retain=True)

    # Temperature
    client.publish("homeassistant/sensor/esp_c_temp/config", json.dumps({
        "name": "Temperature",
        "state_topic": "home/esp_c/dht/temperature",
        "unit_of_measurement": "°C",
        "unique_id": "esp_c_temp"
    }), retain=True)

    # Humidity
    client.publish("homeassistant/sensor/esp_c_hum/config", json.dumps({
        "name": "Humidity",
        "state_topic": "home/esp_c/dht/humidity",
        "unit_of_measurement": "%",
        "unique_id": "esp_c_hum"
    }), retain=True)

def on_message(client, userdata, msg):
    print(msg.topic, msg.payload.decode())

client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, 1883, 60)
client.loop_forever()
import paho.mqtt.client as mqtt
import time

BROKER = "192.168.0.100"
PORT = 1883

# Using Callback API Version 2 of paho-mqtt
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("[MQTT] Connected to broker successfully!")
        topics = [
            "espD/relay1/set",
            "espD/relay2/set",
            "espD/servo1/set",
            "espD/servo2/set",
            "espD/fan/set"
        ]
        for topic in topics:
            print(f"[MQTT] Clearing retained message on: {topic}")
            # Publishing None/empty payload with retain=True deletes the retained message
            client.publish(topic, payload=None, retain=True)
        print("[MQTT] All target topics cleared!")
    else:
        print(f"[MQTT] Connection failed with code {rc}")

client.on_connect = on_connect
print(f"[SYSTEM] Connecting to broker {BROKER}:{PORT}...")
client.connect(BROKER, PORT, 60)
client.loop_start()

# Wait 2 seconds to complete connection and publish the clear commands
time.sleep(2)

client.loop_stop()
client.disconnect()
print("[SYSTEM] Script finished.")

import paho.mqtt.client as mqtt

client = mqtt.Client()
client.connect("localhost", 1883, 60)

client.loop_start()

# Xóa toàn bộ config discovery
client.publish("homeassistant/#", "", retain=True)

print("Cleared MQTT discovery")
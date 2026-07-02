# realtime_inference.py

import json
import joblib
import numpy as np
import paho.mqtt.client as mqtt

# MQTT CONFIGURATION
BROKER = "localhost"
PORT = 1883

# Topics
SENSOR_TOPIC = "esp32/sensor_data"
ACTUATION_TOPIC = "esp32/actuation_command"

# Load best model
model = joblib.load("best_model.pkl")

print("Best ML Model Loaded Successfully")


# MQTT CONNECT CALLBACK
def on_connect(client, userdata, flags, rc):

    print("Connected to MQTT Broker")

    client.subscribe(SENSOR_TOPIC)

    print(f"Subscribed to topic: {SENSOR_TOPIC}")


# MQTT MESSAGE CALLBACK
def on_message(client, userdata, msg):

    try:

        payload = msg.payload.decode()

        print("\nReceived Sensor Data:")
        print(payload)

        data = json.loads(payload)

        moisture = data["moisture"]
        temperature = data["temperature"]
        humidity = data["humidity"]

        # Prepare input for ML model
        X = np.array([[moisture, temperature, humidity]])

        # Predict stress level
        prediction = model.predict(X)[0]

        print(f"Predicted Level: {prediction}")

        # -----------------------------
        # LEVEL 0
        # -----------------------------
        if prediction == 0:

            command = {
                "level": 0,
                "label": "Low",
                "buzzer": "off",
                "led": "green"
            }

        # -----------------------------
        # LEVEL 1
        # -----------------------------
        elif prediction == 1:

            command = {
                "level": 1,
                "label": "Medium",
                "buzzer": "beep",
                "led": "blue"
            }

        # -----------------------------
        # LEVEL 2
        # -----------------------------
        else:

            command = {
                "level": 2,
                "label": "High",
                "buzzer": "continuous",
                "led": "red"
            }

        # Convert to JSON
        command_json = json.dumps(command)

        # Publish MQTT downlink
        client.publish(ACTUATION_TOPIC, command_json)

        print("\nPublished Command:")
        print(command_json)

    except Exception as e:

        print("Error:", e)


# CREATE MQTT CLIENT
client = mqtt.Client()

client.on_connect = on_connect
client.on_message = on_message

# CONNECT TO BROKER
client.connect(BROKER, PORT, 60)

print("\nWaiting for sensor data...\n")

# START LOOP
client.loop_forever()
import json
import csv
import os

from datetime import datetime

import paho.mqtt.client as mqtt

BROKER = "localhost"
PORT = 1883

TOPIC = "esp32/sensor_data"

CSV_FILE = "sensor_data.csv"


if not os.path.exists(CSV_FILE):

    with open(CSV_FILE, "w", newline="") as file:

        writer = csv.writer(file)

        writer.writerow([
            "timestamp",
            "temperature",
            "humidity",
            "moisture"
        ])


def on_connect(client, userdata, flags, reason_code, properties=None):

    print("Connected to MQTT Broker")

    client.subscribe(TOPIC)

    print(f"Subscribed to {TOPIC}")


def on_message(client, userdata, msg):

    try:

        data = json.loads(
            msg.payload.decode()
        )

        timestamp = datetime.now().strftime(
            "%Y-%m-%d %H:%M:%S"
        )

        with open(CSV_FILE, "a", newline="") as file:

            writer = csv.writer(file)

            writer.writerow([
                timestamp,
                data["temperature"],
                data["humidity"],
                data["moisture"]
            ])

        print(
            f"{timestamp} | "
            f"T={data['temperature']} "
            f"H={data['humidity']} "
            f"M={data['moisture']}"
        )

    except Exception as e:

        print("Error:", e)


client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2
)

client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT)

client.loop_forever()

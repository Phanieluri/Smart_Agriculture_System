import json
import csv
import sys
import os
from datetime import datetime

import paho.mqtt.client as mqtt

BROKER = "localhost"
PORT = 1883
TOPIC = "esp32/sensor_data"

MAX_RECORDS = 350

if len(sys.argv) != 2:
    print("\nUsage:")
    print("python mqtt_dataset_logger.py 0")
    print("python mqtt_dataset_logger.py 1")
    print("python mqtt_dataset_logger.py 2\n")
    sys.exit(1)

LEVEL = sys.argv[1]

if LEVEL not in ["0", "1", "2"]:
    print("Level must be 0, 1 or 2")
    sys.exit(1)

CSV_FILE = f"level{LEVEL}.csv"


def get_record_count():

    if not os.path.exists(CSV_FILE):
        return 0

    with open(CSV_FILE, "r") as f:
        return max(sum(1 for _ in f) - 1, 0)


def create_file_if_needed():

    if not os.path.exists(CSV_FILE):

        with open(CSV_FILE, "w", newline="") as f:

            writer = csv.writer(f)

            writer.writerow([
                "temperature",
                "humidity",
                "moisture"
            ])


create_file_if_needed()


def on_connect(client, userdata, flags, reason_code, properties=None):

    print("Connected to MQTT Broker")
    client.subscribe(TOPIC)

    print(f"Collecting data for Level {LEVEL}")
    print(f"Saving to {CSV_FILE}")


def on_message(client, userdata, msg):

    try:

        count = get_record_count()

        if count >= MAX_RECORDS:

            print(f"\n{CSV_FILE} reached {MAX_RECORDS} rows")
            print("Stopping collection...\n")

            client.disconnect()
            return

        data = json.loads(msg.payload.decode())

        with open(CSV_FILE, "a", newline="") as f:

            writer = csv.writer(f)

            writer.writerow([
                data["temperature"],
                data["humidity"],
                data["moisture"]
            ])

        print(
            f"[{count+1}/{MAX_RECORDS}] "
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

import binascii
from datetime import datetime

import paho.mqtt.client as mqtt
import redis


tags_db = redis.Redis(
    host="database", db=0, port=6379, charset="utf-8", decode_responses=True
)

attendance_db = redis.Redis(
    host="database", db=1, port=6379, charset="utf-8", decode_responses=True
)

is_connected = tags_db.ping()

if is_connected:
    print("Subscriber connected to database!")


def on_connect(client, userdata, flags, rc):
    print(f"\nConnected with result code {rc}\n", flush=True)
    client.subscribe("/topic/tag_uid")

def check_on_database(key):
    value = tags_db.get(key)
    return True if value is not None else False


def update_attendance(key):
    time = str(datetime.now())
    attendance_list = attendance_db.get(key)
    if attendance_list is None:
        attendance_list = time
    else:
        attendance_list += f"\n{time}"
    attendance_db.set(key, attendance_list)


def check_PICC_UID(picc_uid):
    result = tags_db.get(picc_uid)
    if result is not None:
        print(f"{picc_uid} found in Redis")
        client.publish("/topic/db_response", "1")
        update_attendance(result)
    else:
        print(f"Key {picc_uid} not found in Redis.")
        client.publish("/topic/db_response", "0")


def on_message(client, userdata, msg):
    topic = msg.topic
    # message = binascii.hexlify(
    #     msg.payload
    # ).decode()  # Converte hexadecimal enviado do ESP32 para string
    message = msg.payload.decode()
    valid = check_on_database(message)
    output = {"Topic": topic, "Message": message, "Valid": valid}
    print(f"Message received! {output}", flush=True)
    check_PICC_UID(message)


if __name__ == "__main__":
    client = mqtt.Client()

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect("broker", 1883, 60)  # connect to broker container

    client.loop_forever()

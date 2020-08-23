import paho.mqtt.client as mqtt
from data_access import DataAccess
from sensor_msg_parser import SensorMessageParser
from sensor_aggregator import SensorAggregator
from datetime import datetime
import weather_access
import json

""" MQTT Parameters """
SUBSCRIBER_ID = "apc-iot:subx1"
SERVER_ADDR = "fd00::1"
MQTT_PORT = 1883
KEEP_ALIVE_TIME = 60
TOP_LEVEL_TOPIC = "apc-iot"
SUBTOPICS = ["113", "056"]
QOS_LEVEL = 0
client = mqtt.Client(SUBSCRIBER_ID, protocol=mqtt.MQTTv31)
""" Database Parameters """
DB_NAME = "apc-iot"
DB_ADDR = "localhost"
DB_PORT = 27017
DB_COLLECTION = "apc_data"
dataLogic = DataAccess(DB_ADDR, DB_PORT, DB_COLLECTION)
""" Weather API Parameters """
api_key = weather_access.get_api_key()
LOCATION_ID = "1729525"
""" State Machine Variables """
state = 0
MAX_STATES = len(SUBTOPICS)
messages = []


# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    for subtopic in SUBTOPICS:
        client.subscribe(TOP_LEVEL_TOPIC + "/" + subtopic + '/#', QOS_LEVEL)


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, message):
    print("Received PUBLISH")
    global state, messages
    if state < MAX_STATES - 1:
        print("Current State: " + str(state))
        print("TOPIC: " + message.topic)
        print("Message(Raw): " + str(message.payload))
        messages.append(message.payload)
        print("Appended message to message list.")
        state += 1

    elif state == MAX_STATES - 1:
        print("Current State: " + str(state))
        print("TOPIC: " + message.topic)
        print("Message(Raw): " + str(message.payload))
        messages.append(message.payload)
        print("Appended message to message list.")
        if dataLogic.is_connection_active:
            document = {}
            try:
                parser = SensorMessageParser(messages)
                parser.parse_data()

                document["collectors_info"] = []
                document["collectors_data"] = []
                for msg in parser.sensor_msgs:
                    document["collectors_info"].append(msg.collector_info)
                    document["collectors_data"].append(msg.collector_data)
            except json.JSONDecodeError:
                print("ERROR - malformed payload data.")
                return

            # add weather field
            weather = weather_access.get_weather_with_id(api_key, LOCATION_ID)
            if weather is not None:
                weather_params = {}

                for key, val in weather.items():
                    if key == 'weather':
                        weather_conditions = []
                        for weather_info in weather['weather']:
                            weather_conditions.append(weather_info['main'])
                        weather_params['api_weather'] = weather_conditions
                    elif key == 'wind':
                        weather_params['api_wind'] = val

                # merge the api weather data with the current data we have
                document.update(weather_params)
            else:
                print("Unable to access weather API")

            # add the date field
            document['date'] = datetime.now()

            print("Message(Formatted): " + str(document))
            dataLogic.insert_document(document)
        else:
            print("Database is not active")

        state = 0
        messages = []


# debug logger
def on_log(client, userdata, level, buf):
    print(buf)


def main():
    client.on_log = on_log
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(SERVER_ADDR, MQTT_PORT, KEEP_ALIVE_TIME)
    dataLogic.connect_db(DB_NAME)

    # Blocking call that processes network traffic, dispatches callbacks and
    # handles reconnecting.
    # Other loop*() functions are available that give a threaded interface and a
    # manual interface.
    client.loop_forever()


if __name__ == "__main__":
    main()

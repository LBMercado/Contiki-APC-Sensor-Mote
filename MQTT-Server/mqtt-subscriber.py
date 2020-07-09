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
SUBTOPICS = ["mote1"]
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
LOCATION = "Manila,ph"
""" State Machine Variables """
state = 0
MAX_STATES = len(SUBTOPICS)
curr_subtopic_index = 0
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
    global state, curr_subtopic_index, messages
    if state < MAX_STATES - 1 and SUBTOPICS[curr_subtopic_index] in message.topic:
        print("Current State: " + str(state))
        print("TOPIC: " + message.topic)
        print("Message(Raw): " + str(message.payload))
        messages.append(message.payload)
        print("Appended message to message list.")
        if curr_subtopic_index + 1 <= len(SUBTOPICS):
            curr_subtopic_index += 1
            state += 1
        else:
            curr_subtopic_index = 0
            state = 0

    elif state == MAX_STATES - 1 and SUBTOPICS[curr_subtopic_index] in message.topic:
        print("Current State: " + str(state))
        print("TOPIC: " + message.topic)
        print("Message(Raw): " + str(message.payload))
        messages.append(message.payload)
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
            weather = weather_access.get_weather(api_key, LOCATION)
            if weather is not None:
                weatherList = []
                for weatherInfo in weather:
                    weatherList.append(weatherInfo['main'])
                document['weather'] = weatherList
            else:
                print("Unable to access weather API")
                document['weather'] = []

            # add the date field
            document['date'] = datetime.now()

            print("Message(Formatted): " + str(document))
            dataLogic.insert_document(document)
        else:
            print("Database is not active")

        curr_subtopic_index = 0
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

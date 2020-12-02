import paho.mqtt.client as mqtt
from configparser import ConfigParser
from data_access import DataAccess
from sensor_msg_parser import SensorMessageParser
from datetime import datetime
import weather_access
import json
from time import time

def main():
    """ MQTT Parameters """
    global SUBSCRIBER_ID, SERVER_ADDRESS, MQTT_PORT, KEEP_ALIVE_TIME, TOP_LEVEL_TOPIC, SUBTOPICS, QOS_LEVEL, \
        TIMEOUT_DURATION
    global DB_NAME, DB_ADDRESS, DB_PORT, DB_MOTEDATA_COLL, DB_PREDICTIONS_COLL
    global API_KEY, LOCATION_ID
    global client, dataLogic
    global state, MAX_STATES, messages, topics_received, timeout_timer

    config = ConfigParser()
    config.read('config.ini')
    # mqtt configuration
    SUBSCRIBER_ID = config['mqtt']['subscriber_id']
    SERVER_ADDRESS = config['mqtt']['server_address']
    MQTT_PORT = int(config['mqtt']['mqtt_port'])
    KEEP_ALIVE_TIME = int(config['mqtt']['keep_alive_time'])
    TOP_LEVEL_TOPIC = config['mqtt']['top_level_topic']
    SUBTOPICS = config['mqtt']['subtopics'].split(',')
    SUBTOPICS = [x.strip(' ') for x in SUBTOPICS]  # trim whitespace
    QOS_LEVEL = int(config['mqtt']['qos_level'])
    TIMEOUT_DURATION = int(config['mqtt']['timeout_duration'])
    # mongodb configuration
    DB_NAME = config['mongodb']['db_name']
    DB_ADDRESS = config['mongodb']['db_address']
    DB_PORT = int(config['mongodb']['db_port'])
    DB_MOTEDATA_COLL = config['mongodb']['db_motedata_collection']
    DB_PREDICTIONS_COLL = config['mongodb']['db_prediction_collection']

    # weather api configuration
    API_KEY = config['openweathermap']['api_key']
    LOCATION_ID = config['openweathermap']['location_id']

    client = mqtt.Client(SUBSCRIBER_ID, protocol=mqtt.MQTTv31)
    client.on_log = on_log
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(SERVER_ADDRESS, MQTT_PORT, KEEP_ALIVE_TIME)
    dataLogic = DataAccess(DB_ADDRESS, int(DB_PORT), DB_NAME)

    state = 0
    MAX_STATES = len(SUBTOPICS)
    messages = []
    topics_received = []

    # Blocking call that processes network traffic, dispatches callbacks and
    # handles reconnecting.
    # Other loop*() functions are available that give a threaded interface and a
    # manual interface.
    client.loop_forever()


# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    for subtopic in SUBTOPICS:
        client.subscribe("{}/{}/evt/status/fmt/json".format(TOP_LEVEL_TOPIC, subtopic), QOS_LEVEL)
        # reset mote timers to synchronize mqtt publication
        client.publish("{}/{}/cmd/timer-reset/fmt/json".format(TOP_LEVEL_TOPIC, subtopic), "1")


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, message):
    print("Received PUBLISH")
    global state, messages, topics_received, timeout_timer
    if state == 0:
        timeout_timer = time()
    else:
        if time() - timeout_timer >= TIMEOUT_DURATION:
            print('Warning! Timeout duration reached in between messages, sending reset timer request to motes.')
            for subtopic in SUBTOPICS:
                # reset mote timers to synchronize mqtt publication
                client.publish("{}/{}/cmd/timer-reset/fmt/json".format(TOP_LEVEL_TOPIC, subtopic), "1")
            state = 0
            messages = []
            topics_received = []
        else:
            timeout_timer = time()
    if state < MAX_STATES - 1:
        print("Current State: " + str(state))
        print("TOPIC: " + message.topic)
        if message.topic not in topics_received:
            topics_received.append(message.topic)
            print("Message(Raw): " + str(message.payload))
            messages.append(message.payload)
            print("Appended message to message list.")
        else:
            print('Warning! Duplicate topic received, ignoring.')
        state += 1

    elif state == MAX_STATES - 1:
        print("Current State: " + str(state))
        print("TOPIC: " + message.topic)
        if message.topic not in topics_received:
            print("Message(Raw): " + str(message.payload))
            messages.append(message.payload)
            print("Appended message to message list.")
        else:
            print('Warning! Duplicate topic received, ignoring.')

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
            weather = weather_access.get_weather_with_id(API_KEY, LOCATION_ID)
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
            dataLogic.insert_document(document, DB_MOTEDATA_COLL)
        else:
            print("Database is not active")

        state = 0
        messages = []
        topics_received = []


# debug logger
def on_log(client, userdata, level, buf):
    print(buf)


if __name__ == "__main__":
    main()

import paho.mqtt.client as mqtt
from data_access import DataAccess
from sensor_msg_parser import SensorMessage
from datetime import datetime
import weather_access
import json

""" MQTT Parameters """
SUBSCRIBER_ID = "apc-iot:subx1"
SERVER_ADDR = "fd00::1"
MQTT_PORT = 1883
KEEP_ALIVE_TIME = 60
TOPIC = "apc-iot/#"
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

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(TOPIC, QOS_LEVEL)


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, message):
    print("Received PUBLISH")
    print("TOPIC: " + message.topic)
    print("Message(Raw): " + str(message.payload))
    if dataLogic.is_connection_active:
        document = {}
        try:
            parser = SensorMessage(message.payload)
            parser.parse_data()

            document['sink'] = parser.sink
            document['collectors'] = parser.collectors

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

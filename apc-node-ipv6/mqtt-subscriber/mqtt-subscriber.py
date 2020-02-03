import paho.mqtt.client as mqtt
import data_access
import weather_access
import json

""" MQTT Parameters """
subscriber_id = "apc-iot:subx1"
address = "fd00::1"
mqtt_port = 1883
keep_alive_time = 60
topic = "apc-iot/#"
qos_level = 0
client = mqtt.Client(subscriber_id, protocol=mqtt.MQTTv31)
""" Database Parameters """
db_name = "apc-iot"
db_address = "localhost"
db_port = 27017
record = "apc_data"
data = data_access.DataAccess(db_address, db_port)
""" Weather API Parameters """
api_key = weather_access.get_api_key()
location = "Manila,ph"

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(topic, qos_level)


# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, message):
    print("Received PUBLISH")
    # python is incorrectly parsing string, must set right decoder
    print(message.topic.decode('utf-8') + " " + str(message.payload).decode('utf-8'))
    if data.is_connection_active:
        doc = json.loads(str(message.payload))
        
        weather = weather_access.get_weather(api_key, location)
        if weather is not None:
            weatherList = []
            for weatherInfo in weather:
                weatherList.append(weatherInfo['main'])
            doc['weather'] = weatherList;
        else:
            doc['weather'] = [];
        data.insert_document(record, doc)
    else:
        print("Database is not active")

# debug logger
def on_log(client, userdata, level, buf):
    print(buf)

def main():
    client.on_log = on_log
    client.on_connect = on_connect
    client.on_message = on_message
    
    client.connect(address, mqtt_port, keep_alive_time)
    data.connect_db(db_name)
    
    # Blocking call that processes network traffic, dispatches callbacks and
    # handles reconnecting.
    # Other loop*() functions are available that give a threaded interface and a
    # manual interface.
    client.loop_forever()


if __name__ == "__main__":
    main()

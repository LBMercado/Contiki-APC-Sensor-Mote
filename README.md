# Contiki-APC-Sensor-Mote
This makes use of the __*Contiki OS and Zoul Firefly Platform*__.
The codes are designed to work in the Zoul Firefly Platform, I cannot guarantee that they will still work on a different platform.

The following codes written are used to program the motes to read from the sensors, and communicate them from the collector motes to the sink mote.

## Two implementations are available here:
* the **rime** implementation is a non-ip based network used for testing purposes with no connectivity outside its local network.
* the **ipv6** implementation provides connectivity outside its local network through a *border router* ~~which is also the sink mote.~~
	- the border router is typically connected through a USB interface on a computer or the Raspberry Pi in this case.

## There are three types of motes in this mote network:
1. APC-Sensor-Node -> Collector Mote
2. APC-Sink-Node -> Sink Mote ~~& Border Router(Not Implemented Yet)~~
3. Border Router

### Sensors Used
* DHT22 (Temperature and Humidity)
* GP2Y1014AU0F (PM2.5)
* ~~MQ7 (Carbon Monoxide)~~
* MQ131 (Ozone)
* ~~MQ135 (Nitrogen Oxides/Carbon Dioxide)~~
* MICS4514 (Nitrogen Dioxide & Carbon Monoxide)

*quantifiable phenomenon are measured*

**This implementation is valid for only one mote network, if using multiple mote networks, a server is needed and is not covered here.**

# Procedure to make this work
	* Create an 'apc-node' folder in the Contiki/examples directory.
		* Move the sink and sensor node folders into this 'apc-node' folder.
		* make necessary changes to the project.conf file to fit your device's needs
	* Move the files within 'place-in-dev-folder' inside the 'platform/zoul/dev' folder in Contiki
	* Replace also the dht22.c file there with the one here since there is a bug that prevents the original one from working properly, this file fixes that bug. [1]

## Compiling and Uploading Code Using Contiki and Zoul Firefly (general procedure)

	1. Create the C file with the program logic. (Measure sensor, broadcast etc.)
	2. Create the make file for compilation.
	3. Enter the command: "make TARGET=zoul BOARD=firefly savetarget"
	4. Enter the command: "make motelist"
		a. This will show the device port which will be required for uploading the code
	5. Enter the command: "make <c-file-name>.upload PORT=<device-port-number>"
	6. To monitor the output of the device in terminal, enter the command: "make login PORT=<device-port-number>"

## Compiling and Running Border Router

	1. Make a copy or run the RPL Border Router example from the contiki examples. (found at examples/ipv6/rpl-border-router)
	2. Enter the following command to run the border router. Connect to the appropriate USB port in your computer and make necessary changes to the PORT and IPv6 address.
	"make border-router.upload PORT=/dev/ttyUSB0 && make connect-router PREFIX="-s /dev/ttyUSB0 fd00::1/64"
	3. At this point, the RPL border router should be running. You should be able to see the router address in the terminal. You can view the neighbors of the router by pasting its router address in a browser.

You can make use of the mqtt subscriber to make the published data persistent. The subscriber will store the published data locally using MongoDB.

## Running the MQTT Server/Subscriber

	1. Make sure that Mosquitto and MongoDB are properly set up and working
	2. Add a config.ini with a section [openweathermap] and key, value pair api_key=~your~api~key~here~
	3. Make necessary changes in mqtt-subscriber variables (mosquitto and mongodb config, ip addresses etc.)
	4. Run mqtt-subscriber.py

## Software Requirements
* Mosquitto - for MQTT functionality
* MongoDB (64-bit) - for storing sensor data persistently
* Paho MQTT - module dependency
* PyMongo - module dependency
* Python - to run the mqtt subscriber script

Make sure to update the git repo for contiki to avoid any other issues
(cd into contiki and pull the latest release using the command "git pull")

[1] For DHT22, bug in code prevents it from working properly make modifications as follows:
The contiki-ng code for dht22 was working and when i compared their c source codes, I found 2 changes that when done in contiki code, it started giving nice output.
	1. initialing last_state = 0xff in dht22.c
	2. commenting out the assignment statement last_state = GPIO_READ_PIN(DHT22_PORT_BASE, DHT22_PIN_MASK); in dht22_read() function of dht22.c

From <https://github.com/contiki-os/contiki/issues/2495> 
reference for the correct code

For the actual circuit of the sensor node, refer to the diagram below
<a href="https://drive.google.com/uc?export=view&id=1zQjA_jTVKHifyyPVH9N4CGrHCIEI7_Gn"><img src="https://drive.google.com/uc?export=view&id=1zQjA_jTVKHifyyPVH9N4CGrHCIEI7_Gn" style="width: 500px; max-width: 100%; height: auto" title="Click for the larger version."/></a>

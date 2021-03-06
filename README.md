# Contiki-APC-Sensor-Mote
![](misc/imgs/Sensor_Node_Setup.jpg)
This makes use of the [_**Contiki OS**_](https://github.com/contiki-os/contiki) and [_**Zoul Firefly Platform**_](https://github.com/Zolertia/Resources/wiki/Firefly).
The codes are designed to work in the Zoul Firefly Platform, I cannot guarantee that they will still work on a different platform. Further, the python scripts were ran and tested in a Linux OS, so there might be incompatibilities when run on different OS.

The following codes written are used to program the motes to read from the sensors, and communicate them from the collector motes to a sink in the border router.

## Update (2021/01/10)
This repository is no longer maintained, use at your own risk.

## Block Diagram
![](misc/imgs/BlockDiagramV2.png)


## Two implementations are available here:
* the **rime** implementation is a non-ip based network used for testing purposes with no connectivity outside its local network.
* the **ipv6** implementation provides connectivity outside its local network through a *border router*
	- the border router is typically connected through a USB interface on a computer or the Raspberry Pi in this case.

## There are two types of motes in this mote network:
1. APC-Sensor-Node -> Collector Mote
2. Border Router

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

## Running the MQTT Server/Subscriber (MQTT-Server)
	1. Make sure that Mosquitto and MongoDB are properly set up and working
	2. Rename distribution config.ini to actual .ini file; add your openweather API key there.
	3. Make necessary changes in the configuration (mosquitto and mongodb config, ip addresses etc.)
	4. Run mqtt-subscriber.py

## Running the CSV Converter for Stored Sensor Values in MongoDB (MQTT-Server)
	1. Make sure that MongoDB is properly set up and working
	2. Look into csv_convert.py and change desired sensor columns or configuration values via the config.ini file.
	3. Run csv_convert.py
	4. The .CSV file should be outputted in the directory where the csv_convert.py is.

## Running the API for the web application
[Refer here for the creation/naming convention of the persistent models](https://github.com/LBMercado/stacked-generalization-ensemble-learning-for-air-pollutant-concentration-prediction.git)

	1. Make sure that MongoDB  is properly set up and working and the models are properly formatted and stored in the folder where the script is in
	2. Make necessary changes to the config.ini file, especially the 'apc_model' section
	3. Run apc_monitor_api.py to start the API.

## Software Requirements
* Mosquitto - for MQTT functionality
* MongoDB (64-bit) - for storing sensor data persistently
* Python - to run the mqtt subscriber script
### Python Module Dependencies
* Paho MQTT - python module dependency for MQTT functionality
* Flask - python module dependency for API functionality
* PyMongo - python module dependency for MongoDB access
* Sci-kit Learn - to run the prediction model ([Check here to find out how to create the model](https://github.com/LBMercado/stacked-generalization-ensemble-learning-for-air-pollutant-concentration-prediction.git))
* Numpy, Pandas - alongside Sci-kit Learn
* Joblib - model persistence

Make sure to update the git repo for contiki to avoid any other issues
(cd into contiki and pull the latest release using the command "git pull")

[1] For DHT22, bug in code prevents it from working properly make modifications as follows:
The contiki-ng code for dht22 was working and when i compared their c source codes, I found 2 changes that when done in contiki code, it started giving nice output.
	1. initialing last_state = 0xff in dht22.c
	2. commenting out the assignment statement last_state = GPIO_READ_PIN(DHT22_PORT_BASE, DHT22_PIN_MASK); in dht22_read() function of dht22.c

From <https://github.com/contiki-os/contiki/issues/2495> 
reference for the correct code

For the actual circuit of the sensor node, refer to the diagram below
# Sensor Node Schematic Diagram
![](misc/imgs/Schem_Sensor_Node.png)
# Power Supply Schematic Diagram
![](misc/imgs/Schem_Power_Supply.png)

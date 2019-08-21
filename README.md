# Contiki-APC-Sensor-Mote
A generic mote that can either act as a master (server) or a slave (sensor node). This makes use of the Contiki OS and Zoul Firefly Platform.

The server node here makes use of the RIME communication stack.
This is valid for one server implementation, a separate server is required to aggregate all of the server's collected data from their slave nodes.

(use the dht22-fixed.c instead of the one pulled from contiki official repo) (see below as to why)

Compiling and Uploading Code Using Contiki and Zoul Firefly (General Procedure)

	1. Create the C file with the program logic. (Measure sensor, broadcast etc.)
	2. Create the make file for compilation.
	3. Enter the command: "make TARGET=zoul BOARD=firefly savetarget"
	4. Enter the command: "make motelist"
		a. This will show the device port which will be required for uploading the code
	5. Enter the command: "make <c-file-name>.upload PORT=<device-port-number>"
	6. To monitor the output of the device in terminal, enter the command: "make login PORT=<device-port-number>"

Make sure to update the git repo for contiki to avoid any other issues
(cd into contiki and pull the latest release using the command "git pull")

For DHT22, bug in code prevents it from working properly make modifications as follows:
The contiki-ng code for dht22 was working and when i compared their c source codes, I found 2 changes that when done in contiki code, it started giving nice output.
	1. initialing last_state = 0xff in dht22.c
	2. commenting out the assignment statement last_state = GPIO_READ_PIN(DHT22_PORT_BASE, DHT22_PIN_MASK); in dht22_read() function of dht22.c

From <https://github.com/contiki-os/contiki/issues/2495> 
reference for correct code
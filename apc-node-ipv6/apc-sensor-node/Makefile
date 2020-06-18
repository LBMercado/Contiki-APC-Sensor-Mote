DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_TARGET_SOURCEFILES+= dht22.c pm25-sensor.c air-quality-sensor.c anemometer-sensor.c shared-sensors.c

CONTIKI_PROJECT = apc-sensor-node
all: $(CONTIKI_PROJECT)

CONTIKI = ../../..
CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
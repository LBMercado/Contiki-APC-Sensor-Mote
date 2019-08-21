DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_TARGET_SOURCEFILES+= dht22.c adc-sensors.c

CONTIKI_PROJECT = apc-sensor-node
all: $(CONTIKI_PROJECT)

CONTIKI = ../..
CONTIKI_WITH_RIME = 1
include $(CONTIKI)/Makefile.include

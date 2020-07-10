DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_TARGET_SOURCEFILES+= dht22.c pm25-sensor.c air-quality-sensor.c anemometer-sensor.c shared-sensors.c

CONTIKI_PROJECT = apc-sensor-node
all: $(CONTIKI_PROJECT)

CFLAGS += -ffunction-sections
LDFLAGS += -Wl,--gc-sections,--undefined=_reset_vector__,--undefined=InterruptVectors,--undefined=_copy_data_init__,--undefined=_clear_bss_init__,--undefined=_end_of_init__
APPS += mqtt
CONTIKI = ../../..
CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include

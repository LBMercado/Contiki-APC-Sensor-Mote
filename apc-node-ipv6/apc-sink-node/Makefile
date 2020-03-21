DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

CONTIKI_PROJECT = apc-sink-node
PROJECT_SOURCEFILES += mqtt-sink-handler.c
all: $(CONTIKI_PROJECT)
CONTIKI = ../../..

APPS += mqtt

CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include

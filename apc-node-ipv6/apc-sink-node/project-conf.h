#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*----------------------------------------------------------------*/
/*---------------------- SINK CONFIGURATION ----------------------*/
/*----------------------------------------------------------------*/
#define MAX_SENSOR_NODES_CONF           3
// request every 10 minutes
#define SENSOR_REQUEST_INTERVAL_CONF    600

/*----------------------------------------------------------------*/
/*------------------------IP-CONFIGURATION------------------------*/
/*----------------------------------------------------------------*/
#define APC_SINK_ADDRESS_CONF           "fd00::212:4B00:1932:E37A"
#define MQTT_CONF_BROKER_IP_ADDR        "fd00::1"
#define MQTT_CONF_STATUS_LED            LEDS_WHITE
#define UDP_COLLECT_PORT                2001
#define UDP_SINK_PORT                   2002
#define UIP_CONF_SINK_64_BIT            1
#define UIP_CONF_SINK_16_BIT            2
#define UIP_CONF_SINK_LL_DERIVED        3
#define UIP_CONF_SINK_MODE              UIP_CONF_SINK_64_BIT
/*----------------------------------------------------------------*/
/*This code was taken from rpl-collect example found in example/ipv6*/
/*----------------------------------------------------------------*/
#ifndef WITH_NON_STORING
#define WITH_NON_STORING 0 /* Set this to run with non-storing mode */
#endif /* WITH_NON_STORING */

#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#undef UIP_CONF_MAX_ROUTES

#ifdef TEST_MORE_ROUTES
/* configure number of neighbors and routes */
#define NBR_TABLE_CONF_MAX_NEIGHBORS     10
#define UIP_CONF_MAX_ROUTES   30
#else
/* configure number of neighbors and routes */
#define NBR_TABLE_CONF_MAX_NEIGHBORS     10
#define UIP_CONF_MAX_ROUTES   10
#endif /* TEST_MORE_ROUTES */

#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nullrdc_driver
#undef NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_CONF_802154_AUTOACK       1

/* Define as minutes */
#define RPL_CONF_DEFAULT_LIFETIME_UNIT   60

/* 10 minutes lifetime of routes */
#define RPL_CONF_DEFAULT_LIFETIME        10

#define RPL_CONF_DEFAULT_ROUTE_INFINITE_LIFETIME 1

#if WITH_NON_STORING
#undef RPL_NS_CONF_LINK_NUM
#define RPL_NS_CONF_LINK_NUM 40 /* Number of links maintained at the root. Can be set to 0 at non-root nodes. */
#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES 0 /* No need for routes */
#undef RPL_CONF_MOP
#define RPL_CONF_MOP RPL_MOP_NON_STORING /* Mode of operation*/
#endif /* WITH_NON_STORING */
/*----------------------------------------------------------------*/
/*This code was taken from rpl-collect example found in example/ipv6*/
/*----------------------------------------------------------------*/
#endif

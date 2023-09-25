#ifndef SOMFY_CONFIG
#define SOMFY_CONFIG

#include <Arduino.h>

#define RING_BUFFER_SIZE 100

const char*         wifi_ssid       = "Fritzi";
const char*         wifi_password   = "45714154412553682462";

const char*         mqtt_server     = "homeassistant.local";
const unsigned int  mqtt_port       = 1883;
const char*         mqtt_user       = "mqtt-user";
const char*         mqtt_password   = "sweethome";
const char*         mqtt_id         = "esp8266-somfy-remote";

const char*         status_topic    = "home/somfy/status"; // Online / offline
const char*         log_topic       = "home/somfy/log"; // Last RING_BUFFER_SIZE commands
const char*         error_topic     = "home/somfy/error";  
String              discovery_topic = "homeassistant/discover/cover"; // Commands ack "id: 0x184623, cmd: u"

#endif
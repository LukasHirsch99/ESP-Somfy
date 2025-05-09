#ifndef SOMFY_CONFIG
#define SOMFY_CONFIG

#define TRANSMIT_PIN D4

#define RING_BUFFER_SIZE 100
#define TCP_PORT 42069

#ifdef DEFINE_WIFI_CREDS
const char *wifi_ssid = "Fritzi";
const char *wifi_password = "45714154412553682462";
#else
extern const char *wifi_ssid;
extern const char *wifi_password;
#endif

#endif

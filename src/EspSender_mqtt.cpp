#include "config.h"
#include <Controllers.h>
#include <SomfyProtocol.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <WiFiUdp.h>

#pragma pack(push, 1)

typedef enum {
  GET_COVERS = 1,
  COVER_CMD = 2,
  ADD_COVER = 3,
  REN_COVER = 4,
  CUSTOM_CMD = 5
} OpCode;

typedef struct {
  unsigned short magicNumber; // 2 byte
  unsigned short opcode;      // 2 byte
  unsigned int remoteId;      // 4 byte
} ReqHeader;

typedef struct {
  byte command;
} ReqCoverCmd;

typedef struct {
  unsigned long rollingCode; // 4 byte
  byte nameLen;              // 1 bytes
  char name[1];              // dynamic length
} ReqAddCover;

typedef struct {
  byte nameLen; // 1 bytes
  char name[1]; // dynamic length
} ReqRenameCover;

typedef struct {
  unsigned long rollingCode;
  byte command;
  unsigned short frameRepeat;
} ReqCustomCmd;

#pragma pack(pop)

enum LogLevel { INFO, ERROR };

String ringBuffer[RING_BUFFER_SIZE];
int rbidx = -1;

String callback(char *data);
void receivedCallback(char *topic, byte *payload, unsigned int length);
void mqttconnect();
String log(LogLevel level, String msg);
String getLog();
String getTimeString();
String handleGetAllCovers();
String handleCoverCommand(ReqHeader *h);
String handleAddCover(ReqHeader *h);
String handleRenameCover(ReqHeader *h);
String handleCustomCommand(ReqHeader *h);

WiFiClient wifiClient;

// Define NTP Client to get time
WiFiUDP ntpUDP;

// 2h offset for summertime
NTPClient timeClient(ntpUDP, "pool.ntp.org", 2 * 3600);

WiFiServer server(TCP_PORT); // TCP server on port 1234

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Somfy");
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.setHostname("somfy-remote");
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Print the IP address
  Serial.println("IP-Address: " + WiFi.localIP().toString());

  if (MDNS.begin("somfy-remote")) {
    Serial.println("mDNS responder started as somfy-remote.local");
  }

  server.begin(); // start TCP server
  MDNS.addService("somfy-socks", "tcp", TCP_PORT);
  Serial.printf("Server started on %s:%d\n", WiFi.localIP().toString().c_str(),
                TCP_PORT);

  // NTPClient
  timeClient.begin();

  pinMode(transmitPin, OUTPUT);
  digitalWrite(transmitPin, HIGH);
  setupControllers();
  Serial.println(controllersToString());
}

void loop() {
  MDNS.update();
  WiFiClient client = server.accept(); // check for incoming clients

  if (client) {
    Serial.println("Client connected");
    char buffer[128];

    while (client.connected()) {
      if (client.available()) {
        // String data = client.readString();
        client.read(buffer, 128);
        client.println(callback(buffer));
      }
    }

    client.stop();
    Serial.println("Client disconnected");
  }
}

String callback(char *data) {
  Serial.print("Received data: ");
  Serial.println(data);

  ReqHeader *h = (ReqHeader *)data;

  if (h->magicNumber != 0xAFFE) {
    return log(ERROR, "invalid message format");
  }

  // Serial.printf("magicNumber: 0x%x\n", p->magicNumber);
  // Serial.printf("opcode: %d\n", p->opcode);
  // Serial.printf("remoteId: %d\n", p->remoteId);
  // Serial.printf("arg1: %lu\n", p->arg1);
  // Serial.printf("arg2: %d\n", p->arg2);
  // Serial.printf("arg3: %d\n", p->arg3);

  String ret;
  switch (h->opcode) {
  // get all remotes
  case GET_COVERS: {
    ret = handleGetAllCovers();
    break;
  }
  // cover control
  case COVER_CMD: {
    ret = handleCoverCommand(h);
    break;
  }
  // add cover, optional remoteId and rollingCode (arg1)
  case ADD_COVER: {
    ret = handleAddCover(h);
    break;
  }
  // rename cover
  case REN_COVER: {
    ret = handleRenameCover(h);
    break;
  }
  // custom command
  case CUSTOM_CMD: {
    ret = handleCustomCommand(h);
    break;
  }
  default:
    ret = log(ERROR, "invalid opcode: " + String(h->opcode, HEX));
  }
  return ret;
}

String handleGetAllCovers() { return controllersToString(); }

String handleCoverCommand(ReqHeader *h) {
  Controller *c = findControllerByRemoteId(h->remoteId);
  if (c == 0) {
    return log(ERROR,
               "controller: 0x" + String(h->remoteId, HEX) + " not found");
  }

  ReqCoverCmd *req = (ReqCoverCmd *)((char *)h + sizeof(*h));
  switch (req->command) {
  case UP:
    sendCommand(UP, c);
    return "open";

  case STOP:
    sendCommand(STOP, c);
    return "stopped";

  case DOWN:
    sendCommand(DOWN, c);
    return "closed";

  case DEL:
    sendCommand(DEL, c);

    if (!deleteController(c)) {
      return log(ERROR, "failed to remove controller: " + String(c->name));
    }
    return log(INFO, "removed controller: 0x" + String(h->remoteId, HEX));
  }

  return log(ERROR, "wrong cmd: 0x" + String(req->command, HEX));
}

String handleRenameCover(ReqHeader *h) {
  Controller *c = findControllerByRemoteId(h->remoteId);
  if (c == 0) {
    return log(ERROR,
               "controller: 0x" + String(h->remoteId, HEX) + " not found");
  }

  ReqRenameCover *req = (ReqRenameCover *)((char *)h + sizeof(*h));

  char *name = (char *)&req->name;

  if (!updateName(c, name, req->nameLen))
    return log(ERROR, "updating name failed");
  return log(INFO, "controller renamed");
}

String handleAddCover(ReqHeader *h) {
  ReqAddCover *req = (ReqAddCover *)((char *)h + sizeof(*h));
  char *name = (char *)&req->name;

  Serial.printf("Adding new cover: %s\n", name);
  int r = addController(name, req->nameLen, req->rollingCode, h->remoteId);
  if (r < 0) {
    if (r == -1)
      return log(ERROR, "controller already exists");
    else if (r == -2)
      return log(ERROR, "no more controller space available");
    else if (r == -3)
      return log(ERROR, "invalid name length");
    else
      return log(ERROR, "unknown error occured");
  }
  return log(INFO, "added cover");
}

String handleCustomCommand(ReqHeader *h) {
  ReqCustomCmd *req = (ReqCustomCmd *)((char *)h + sizeof(*h));
  buildCustomFrame(req->command, h->remoteId, req->rollingCode);
  sendCustomCommand(req->frameRepeat);
  return "success";
}

String log(LogLevel level, String msg) {
  String ts = getTimeString();
  String logMsg = ts + " - ";

  if (level == INFO)
    logMsg.concat("INFO: ");
  else if (level == ERROR)
    logMsg.concat("ERROR: ");
  logMsg.concat(msg);

  Serial.println(logMsg);

  if (rbidx >= RING_BUFFER_SIZE)
    rbidx = 0;
  ringBuffer[rbidx++] = logMsg;

  String allLogs = getLog();
  String jsonStr = "{\"log\": \"" + allLogs + "\"}";
  return logMsg;
}

String getLog() {
  String logMsg;
  int rbi = rbidx - 1;

  for (int i = 0; i < RING_BUFFER_SIZE; i++) {
    if (rbi < 0)
      rbi = RING_BUFFER_SIZE - 1;

    if (ringBuffer[rbi].isEmpty())
      break;

    if (i > 0)
      logMsg.concat("\n");

    logMsg.concat(ringBuffer[rbi--]);
  }

  logMsg.replace("\n", "\\n");
  return logMsg;
}

String getTimeString() {
  timeClient.update();
  time_t epochTime = (time_t)timeClient.getEpochTime();
  struct tm *ptm = gmtime(&epochTime);

  char buf[40];
  // 12.12.2023 12:59:12
  sprintf(buf, "%02d.%02d.%04d %02d:%02d:%02d", ptm->tm_mday, ptm->tm_mon + 1,
          ptm->tm_year + 1900, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

  return String(buf);
}

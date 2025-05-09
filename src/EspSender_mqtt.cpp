#define DEFINE_WIFI_CREDS
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

typedef enum {
  GET_COVERS = 1,
  COVER_CMD = 2,
  ADD_COVER = 3,
  REN_COVER = 4,
  CUSTOM_CMD = 5
} OpCode;

#define SUCCESS 0
#define INVALID_PAYLOAD_LENGTH 1
#define INVALID_MAGIC_NUMBER 2
#define INVALID_OPCODE 3

#pragma pack(push, 1)

typedef struct {
  u_int16_t magicNumber; // 2 byte
  u_int8_t opcode;       // 1 byte
} ReqHeader;

typedef struct {
  u_int32_t remoteId; // 4 byte
  u_int8_t command;
} ReqCoverCmd;

typedef struct {
  u_int32_t remoteId;    // 4 byte
  u_int32_t rollingCode; // 4 byte
  char name[MAX_NAME_LEN];
} ReqAddCover;

typedef struct {
  u_int32_t remoteId; // 4 byte
  char name[MAX_NAME_LEN];
} ReqRenameCover;

typedef struct {
  u_int32_t remoteId; // 4 byte
  u_int32_t rollingCode;
  u_int8_t command;
  u_int8_t frameRepeat;
} ReqCustomCmd;

typedef struct {
  u_int8_t opcode;
  u_int8_t status;
} ResHeader;

#pragma pack(pop)

enum LogLevel { INFO, ERROR };

String ringBuffer[RING_BUFFER_SIZE];
int rbidx = -1;
byte *resBuf;
size_t resBufLng = 0;

void handleRequest(char *data);
void receivedCallback(char *topic, byte *payload, unsigned int length);
void mqttconnect();
String log(LogLevel level, String msg);
String getLog();
String getTimeString();
void handleGetAllCovers(ReqHeader *req);
void handleCoverCommand(ReqHeader *h);
void handleAddCover(ReqHeader *h);
void handleRenameCover(ReqHeader *h);
void handleCustomCommand(ReqHeader *h);
ResHeader *allocateResHeader();

WiFiClient wifiClient;

// Define NTP Client to get time
WiFiUDP ntpUDP;

// 2h offset for summertime
NTPClient timeClient(ntpUDP, "pool.ntp.org", 2 * 3600);

WiFiServer server(TCP_PORT); // TCP server on port 1234
LogLevel lastStatus;
ResHeader res;

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

  pinMode(TRANSMIT_PIN, OUTPUT);
  digitalWrite(TRANSMIT_PIN, HIGH);
  setupControllers();
  Serial.println(controllersToString());
}

void loop() {
  MDNS.update();
  WiFiClient client = server.accept(); // check for incoming clients

  if (client) {
    Serial.println("Client connected");
    char buffer[128];
    int dC = 0;

    while (client.connected()) {
      dC = client.available();
      if (dC) {
        client.read(buffer, dC);
        String res;
        handleRequest(buffer);
        client.write(resBuf, resBufLng);
        free(resBuf);
        resBufLng = 0;
      }
    }

    client.stop();
    Serial.println("Client disconnected");
  }
}

void handleRequest(char *data) {
  log(INFO, "Received data: " + String(data));

  if (strlen(data) < sizeof(ReqHeader)) {
    res.status = 1;
    log(ERROR, "invalid message");
    return;
  }

  ReqHeader *h = (ReqHeader *)data;

  if (h->magicNumber != 0xAFFE) {
    log(ERROR, "invalid magicNumber");
    return;
  }

  switch (h->opcode) {
  // get all remotes
  case GET_COVERS: {
    handleGetAllCovers(h);
    break;
  }
  // cover control
  case COVER_CMD: {
    handleCoverCommand(h);
    break;
  }
  // add cover, optional remoteId and rollingCode (arg1)
  case ADD_COVER: {
    handleAddCover(h);
    break;
  }
  // rename cover
  case REN_COVER: {
    handleRenameCover(h);
    break;
  }
  // custom command
  case CUSTOM_CMD: {
    handleCustomCommand(h);
    break;
  }
  default:
    log(ERROR, "invalid opcode: " + String(h->opcode, HEX));
    break;
  }
}

void handleGetAllCovers(ReqHeader *req) {
  log(INFO, "Get all covers");
  u_int8_t c = getControllerCount();

  // Allocate space for header, count and covers
  resBufLng = sizeof(ResHeader) + sizeof(u_int8_t) + c * sizeof(Controller);
  resBuf = (byte *)malloc(resBufLng);

  ResHeader *h = (ResHeader *)resBuf;
  h->opcode = req->opcode | 0x80;
  h->status = 0;
  u_int8_t *count = (u_int8_t *)(resBuf + sizeof(ResHeader));
  *count = c;
  listToByteArray(resBuf + sizeof(ResHeader) + sizeof(u_int8_t));
}

void handleCoverCommand(ReqHeader *h) {
  ReqCoverCmd *req = (ReqCoverCmd *)((char *)h + sizeof(*h));

  ResHeader *res = allocateResHeader();
  res->opcode = h->opcode | 0x80;
  res->status = 0;

  Controller *c = findControllerByRemoteId(req->remoteId);
  if (c == 0) {
    log(ERROR, "controller: 0x" + String(req->remoteId, HEX) + " not found");
    res->status = 1;
    return;
  }

  switch (req->command) {
  case UP:
    sendCommand(UP, c);
    log(INFO, String(c->name) + " opened");
    break;

  case STOP:
    sendCommand(STOP, c);
    log(INFO, String(c->name) + "stopped");
    break;

  case DOWN:
    sendCommand(DOWN, c);
    log(INFO, String(c->name) + "closed");
    break;

  case DEL:
    sendCommand(DEL, c);
    deleteController(c);
    log(INFO, "removed controller: 0x" + String(req->remoteId, HEX));
    break;
  default:
    res->status = 2;
    log(ERROR, "wrong cmd: 0x" + String(req->command, HEX));
    break;
  }
}

void handleAddCover(ReqHeader *h) {
  ReqAddCover *req = (ReqAddCover *)((char *)h + sizeof(*h));

  resBufLng = sizeof(ResHeader) + sizeof(Controller);
  resBuf = (byte *)malloc(resBufLng);
  ResHeader *res = (ResHeader *)resBuf;
  res->opcode = h->opcode | 0x80;
  res->status = 0;

  int idx = addController(req->name, req->rollingCode, req->remoteId);
  if (idx < 0) {
    res->status = -idx;
    if (idx == -1) {
      log(ERROR, "name already exists");
    } else if (idx == -2) {
      log(ERROR, "remoteId already exists");
    } else if (idx == -3) {
      log(ERROR, "no more controller space available");
    }
    return;
  }

  Controller *resC = (Controller *)(resBuf + sizeof(ResHeader));
  Controller *c = getControllerByIndex(idx);
  memcpy(resC, c, sizeof(Controller));

  log(INFO, "added " + String(req->name));
  Serial.println(controllersToString());
}

void handleRenameCover(ReqHeader *h) {
  ReqRenameCover *req = (ReqRenameCover *)((char *)h + sizeof(*h));

  ResHeader *res = allocateResHeader();
  res->opcode = h->opcode | 0x80;
  res->status = 0;

  Controller *c = findControllerByRemoteId(req->remoteId);
  if (c == 0) {
    res->status = 1;
    log(ERROR, "controller: 0x" + String(req->remoteId, HEX) + " not found");
  }

  if (!updateName(c, req->name)) {
    res->status = 2;
    log(ERROR, "updating name failed");
  }
  log(INFO, "controller 0x" + String(c->remoteId, HEX) + " renamed to " +
                String(req->name));
}

void handleCustomCommand(ReqHeader *h) {
  ReqCustomCmd *req = (ReqCustomCmd *)((char *)h + sizeof(*h));
  ResHeader *res = allocateResHeader();
  res->opcode = h->opcode | 0x80;
  res->status = 0;
  buildCustomFrame(req->command, req->remoteId, req->rollingCode);
  sendCustomCommand(req->frameRepeat);
}

ResHeader *allocateResHeader() {
  resBufLng = sizeof(ResHeader);
  resBuf = (byte *)malloc(resBufLng);
  return (ResHeader *)resBuf;
}

String log(LogLevel level, String msg) {
  String ts = getTimeString();
  String logMsg = ts + " - ";
  lastStatus = level;

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

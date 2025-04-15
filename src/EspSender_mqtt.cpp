#include "config.h"
#include <Controllers.h>
#include <SomfyProtocol.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <WiFiUdp.h>

typedef struct {
  unsigned short magicNumber; // 2 byte
  unsigned short opcode;      // 2 byte
  unsigned int remoteId;      // 4 byte
  unsigned long rollingCode;  // 4 byte
  byte cmd_nameLen;           // 4 bytes
  unsigned int frameRepeat;   // 4 byte
} Payload;

String ringBuffer[RING_BUFFER_SIZE];
int rbidx = -1;

String callback(char *data);
void receivedCallback(char *topic, byte *payload, unsigned int length);
void mqttconnect();
void addLog(String msg);
String getLog();
String getTimeString();
String handleGetAllCovers(Payload *p);
String handleCoverCommand(Payload *p);
String handleAddCover(Payload *p);
String handleRenameCover(Payload *p);
String handleCustomCommand(Payload *p);

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
  WiFi.setHostname("esp-somfy-remote");
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Print the IP address
  Serial.println("IP-Address: " + WiFi.localIP().toString());

  server.begin(); // start TCP server
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

  Payload *p = (Payload *)data;

  if (p->magicNumber != 0xAFFE)
    return "ERROR: invalid message format";

  // Serial.printf("magicNumber: 0x%x\n", p->magicNumber);
  // Serial.printf("opcode: %d\n", p->opcode);
  // Serial.printf("remoteId: %d\n", p->remoteId);
  // Serial.printf("arg1: %lu\n", p->arg1);
  // Serial.printf("arg2: %d\n", p->arg2);
  // Serial.printf("arg3: %d\n", p->arg3);

  String ret;
  switch (p->opcode) {
  // get all remotes
  case 0x1: {
    ret = handleGetAllCovers(p);
    break;
  }
  // cover control
  case 0x2: {
    ret = handleCoverCommand(p);
    break;
  }
  // add cover, optional remoteId and rollingCode (arg1)
  case 0x3: {
    ret = handleAddCover(p);
    break;
  }
  // rename cover
  case 0x4: {
    ret = handleRenameCover(p);
    break;
  }
  // custom command
  case 0x5: {
    ret = handleCustomCommand(p);
    break;
  }
  default:
    ret = "ERROR: invalid opcode: " + String(p->opcode, HEX);
  }
  return ret;
}

String handleGetAllCovers(Payload *p) { return controllersToString(); }

String handleCoverCommand(Payload *p) {
  Controller *c = findControllerByRemoteId(p->remoteId);
  if (c == 0) {
    addLog("Cmd, controller: 0x" + String(p->remoteId, HEX) + " not found");
    return "Cmd, controller: 0x" + String(p->remoteId, HEX) + " not found";
  }

  byte cmd = (byte)p->cmd_nameLen;
  switch (cmd) {
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
      addLog("Cmd, failed to remove controller: " + String(c->name));
      return "Cmd, failed to remove controller: " + String(c->name);
    }
    addLog("INFO: removed controller: " + String(c->name));
    return "removed";
  }

  addLog("ERROR: wrong cmd: 0x" + String(cmd, HEX));
  return "ERROR: wrong cmd: 0x" + String(cmd, HEX);
}

String handleRenameCover(Payload *p) {
  Controller *c = findControllerByRemoteId(p->remoteId);
  int nameLen = p->cmd_nameLen;
  char *name = (char *)&p->frameRepeat;
  name[nameLen] = 0;
  if (!updateName(c, name))
    return "ERROR: controller does not exist";
  return "INFO: controller renamed";
}

String handleAddCover(Payload *p) {
  int nameLen = p->cmd_nameLen;
  char *name = (char *)&p->frameRepeat;
  name[nameLen] = 0;
  unsigned long rc = p->rollingCode;
  int remoteId = p->remoteId;

  Serial.printf("Adding new cover: %s\n", name);
  int r = addController(name, rc, remoteId);
  if (r == -1) {
    return "ERROR: controller already exists";
  }
  if (r == -2) {
    return "ERROR: no more controller space available";
  }
  return "INFO: added cover";
}

String handleCustomCommand(Payload *p) {
  buildCustomFrame(p->cmd_nameLen, p->remoteId, p->rollingCode);
  sendCustomCommand(p->frameRepeat);
  return "success";
}

void addLog(String msg) {
  String ts = getTimeString();
  msg.replace("\n", "\\n");
  String logMsg = ts + " - " + msg;
  if (rbidx >= RING_BUFFER_SIZE)
    rbidx = 0;
  ringBuffer[rbidx++] = logMsg;

  String allLogs = getLog();
  String jsonStr = "{\"log\": \"" + allLogs + "\"}";
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
      logMsg.concat("\\n");

    logMsg.concat(ringBuffer[rbi--]);
  }

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

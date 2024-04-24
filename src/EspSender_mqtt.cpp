#include "config.h"
#include <Controllers.h>
#include <SomfyProtocol.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <WiFiUdp.h>

String ringBuffer[RING_BUFFER_SIZE];
int rbidx = -1;

void receivedCallback(char *topic, byte *payload, unsigned int length);
void mqttconnect();
void addLog(String msg);
String getLog();
String getTimeString();

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Define NTP Client to get time
WiFiUDP ntpUDP;
// 2h offset for summertime
NTPClient timeClient(ntpUDP, "pool.ntp.org", 2 * 3600);

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

  // Configure to mqtt-server
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(receivedCallback);
  client.setBufferSize(2048);

  // NTPClient
  timeClient.begin();

  pinMode(transmitPin, OUTPUT);
  digitalWrite(transmitPin, HIGH);
  setupControllers();
  Serial.println(controllersToString());
}

void loop() {
  if (!client.connected()) {
    mqttconnect();
  }
  client.loop();
}

void receivedCallback(char *topic, byte *payload, unsigned int length) {
  String t = String(topic);
  String name = t;
  name.replace("home/somfy/", "");

  if (t.endsWith("/custom")) {
    /**
     * Payload:
     * {
     *  "remoteId": <id>
     *  "rollingCode": <rollingCode>
     *  "command": <command>
     *  "frameRepeat": <numberOfFrameRepetitions>
     * }
     */

    StaticJsonDocument<256> doc;
    deserializeJson(doc, (char *)payload);

    int remoteId = doc["remoteId"].as<int>();
    unsigned long rollingCode = doc["rollingCode"].as<unsigned long>();
    char c = doc["command"].as<uint8_t>();
    int command;
    int frameRepeat = doc["frameRepeat"].as<int>();

    switch (c) {
    case 'u':
      command = UP;
      break;

    case 'd':
      command = DOWN;
      break;

    case 's':
      command = STOP;
      break;

    case 'r':
      command = DEL;
      break;

    case 'p':
      command = PROG;
      break;

    default:
      addLog("unknown command: " + String(c));
      return;
      break;
    }

    buildCustomFrame(command, remoteId, rollingCode);

    sendCustomCommand(2);
    for (int i = 0; i < frameRepeat; i++) {
      sendCustomCommand(7);
    }

    String ackString = "Custom command, remoteId: ";
    ackString.concat(remoteId);
    ackString.concat(", rollingCode: ");
    ackString.concat(rollingCode);
    ackString.concat(", cmd: ");
    ackString.concat(command);

    addLog(ackString);
  } else if (t.endsWith("/addCustom")) {
    name.replace("/addCustom", "");

    /**
     * Payload:
     * {
     *  "remoteId": <id>
     *  "rollingCode": <rollingCode>
     * }
     */

    StaticJsonDocument<256> doc;
    deserializeJson(doc, (char *)payload);

    int remoteId = doc["remoteId"].as<int>();
    unsigned long rollingCode = doc["rollingCode"].as<unsigned long>();

    Controller *c = addController(name.c_str(), ("home/somfy/" + name).c_str());
    if (c == 0) {
      addLog("Custom add, failed to add controller: " + name);
      return;
    }

    doc.clear();
    char discoverPayload[256];

    doc["name"] = name;
    doc["avty_t"] = "home/somfy/status";
    doc["cmd_t"] = "home/somfy/" + name + "/cmd";
    doc["pl_open"] = "u";
    doc["pl_stop"] = "s";
    doc["pl_cls"] = "d";

    serializeJson(doc, discoverPayload);
    client.publish((discovery_topic + "/" + name + "/config").c_str(),
                   discoverPayload);

    c->remoteId = remoteId;
    c->rollingCode = rollingCode;
    saveControllers();

    sendCommand(PROG, c);

    String ackString = "Custom add, added controller: ";
    ackString.concat(name);
    ackString.concat(", remoteId: ");
    ackString.concat(remoteId);
    ackString.concat(", rollingCode: ");
    ackString.concat(rollingCode);
    addLog(ackString);
  } else if (t.endsWith("/add")) {
    String cName = String((char *)payload);

    Controller *c =
        addController(cName.c_str(), ("home/somfy/" + cName).c_str());
    if (c == 0) {
      addLog("Add, failed to add controller: " + cName);
      return;
    }

    StaticJsonDocument<256> doc;
    char discoverPayload[256];

    doc["name"] = cName;
    doc["avty_t"] = "home/somfy/status";
    doc["cmd_t"] = "home/somfy/" + cName + "/cmd";
    doc["pl_open"] = "u";
    doc["pl_stop"] = "s";
    doc["pl_cls"] = "d";

    serializeJson(doc, discoverPayload);
    client.publish((discovery_topic + "/" + cName + "/config").c_str(),
                   discoverPayload);

    sendCommand(PROG, c);

    addLog("Add, added controller: " + cName);
  } else if (t.endsWith("/rename")) {
    name.replace("/rename", "");
    Controller *c = findControllerByName(name.c_str());

    if (updateName(c, (char *)payload))
      addLog("Rename, renamed: " + name + " to: " + String((char *)payload));
    else
      addLog("Rename, controller: " + name + " not found");
  } else if (t.endsWith("/cmd")) {
    name.replace("/cmd", "");
    Controller *c = findControllerByName(name.c_str());
    char cmd = payload[0];
    String state;

    if (c == 0) {
      addLog("Cmd, controller: " + name + " not found");
      return;
    }

    switch (cmd) {
    case 'u':
      sendCommand(UP, c);
      state = "open";
      break;

    case 's':
      sendCommand(STOP, c);
      state = "stopped";
      break;

    case 'd':
      sendCommand(DOWN, c);
      state = "closed";
      break;

    case 'r':
      sendCommand(DEL, c);

      if (deleteController(name.c_str()))
        addLog("Cmd, removed controller: " + name);
      else {
        addLog("Cmd, failed to remove controller: " + name);
        return;
      }
      state = "removed";
      break;
    }
    String ackString = "Cmd, controller: ";
    ackString.concat(name);
    ackString.concat(", cmd: ");
    ackString.concat(cmd);
    addLog(ackString);
    // client.publish((String(c->base_topic) + "/ack").c_str(),
    // ackString.c_str()); client.publish((String(c->base_topic) +
    // "/state").c_str(), state.c_str(), true);
  } else if (t.endsWith("/getControllers")) {
    addLog(controllersToString());
  }
}

void mqttconnect() {
  unsigned long connectionStart = millis();
  // Loop until reconnected
  while (!client.connected()) {
    Serial.print("MQTT connecting...");

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if (millis() > connectionStart + 60000)
        ESP.restart();
    }

    // Connect to MQTT, with retained last will message "offline"
    if (client.connect(mqtt_id, mqtt_user, mqtt_password, status_topic, 1, true,
                       "offline")) {
      Serial.println("connected");
      client.subscribe("home/somfy/#", 1);
      client.publish(status_topic, "online", true);
      client.publish("home/somfy/state", "On");
      return;
    } else {
      Serial.print("failed, status code = ");
      Serial.print(client.state());
      Serial.println(", try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    if (millis() > connectionStart + 60000)
      ESP.restart();
  }
}

void addLog(String msg) {
  String ts = getTimeString();
  msg.replace("\n", "\\n");
  String logMsg = ts + " - " + msg;
  if (++rbidx >= RING_BUFFER_SIZE)
    rbidx = 0;
  ringBuffer[rbidx] = logMsg;

  String allLogs = getLog();
  String jsonStr = "{\"log\": \"" + allLogs + "\"}";

  client.publish(log_topic, jsonStr.c_str(), true);
}

String getLog() {
  String logMsg;
  int rbi = rbidx;
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

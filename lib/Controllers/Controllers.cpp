#include <Controllers.h>

ControllerList controllers; // EEPROM structure

u_int32_t getMaxRemoteNumber() {
  u_int32_t max = 10;
  for (int i = 0; i < controllers.cap; i++) {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (max < controllers.list[i].remoteId)
      max = controllers.list[i].remoteId;
  }
  return max;
}

bool updateName(Controller *c, const char newName[MAX_NAME_LEN]) {
  if (c == 0)
    return false;

  strncpy(c->name, newName, MAX_NAME_LEN);
  c->name[MAX_NAME_LEN - 1] = 0;
  saveControllers();
  return true;
}

Controller *findControllerByName(const char *name) {
  for (int i = 0; i < controllers.cap; i++) {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (strcmp(controllers.list[i].name, name) == 0)
      return &controllers.list[i];
  }
  return 0;
}

Controller *findControllerByRemoteId(u_int32_t remoteId) {
  for (int i = 0; i < controllers.cap; i++) {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (controllers.list[i].remoteId == remoteId)
      return &controllers.list[i];
  }
  return 0;
}

Controller *getControllerByIndex(u_int32_t idx) {
  return &controllers.list[idx];
}

/// @returns -1 name already exists
/// @returns -2 remoteId already exists
/// @returns -3 no more space available
/// @returns the index of the newly created controller
int addController(const char *name, u_int32_t rc, u_int32_t remoteId) {
  if (findControllerByName(name) != 0) {
    return -1;
  }
  if (remoteId != 0 && findControllerByRemoteId(remoteId) != 0) {
    return -2;
  }

  for (int i = 0; i < controllers.cap; i++) {
    if (controllers.list[i].name[0] != 0)
      continue;

    strncpy(controllers.list[i].name, name, MAX_NAME_LEN);
    controllers.list[i].name[MAX_NAME_LEN - 1] = 0;

    controllers.list[i].remoteId =
        remoteId != 0 ? remoteId : getMaxRemoteNumber() + 1;
    controllers.list[i].rollingCode = rc;

    controllers.count++;
    saveControllers();
    return i;
  }
  return -3; // All available Controllers used
}

bool deleteControllerByName(const char *name) {
  for (int i = 0; i < controllers.cap; i++) {
    if (strcmp(controllers.list[i].name, name) == 0) {
      deleteController(&controllers.list[i]);
      return true;
    }
  }
  return false; // controller not found
}

void deleteController(Controller *c) {
  c->name[0] = 0;
  c->remoteId = 0;
  c->rollingCode = 0;
  controllers.count--;
  saveControllers();
}

void setupControllers() {
  EEPROM.begin(sizeof(controllers) + BASE_CONTROLLERLIST);
  EEPROM.get(BASE_CONTROLLERLIST, controllers);

  if (controllers.magicNumber != dMagicNumber) {
    Serial.println("Setting up controllers");
    controllers.magicNumber = dMagicNumber;
    controllers.cap = REMOTE_COUNT;
    controllers.count = 0;
    for (int i = 0; i < controllers.cap; i++) {
      controllers.list[i].name[0] = 0;
      controllers.list[i].remoteId = 0;
      controllers.list[i].rollingCode = 0;
    }
    saveControllers();
  }
}

void saveControllers() {
  EEPROM.put(BASE_CONTROLLERLIST, controllers);
  EEPROM.commit();
}

// idx = controller index
void saveRollingCode(Controller *c) {
  int offset = BASE_CONTROLLERLIST;
  offset += (char *)&c->rollingCode - (char *)&controllers;
  c->rollingCode++; // increment!
  EEPROM.put(offset, c->rollingCode);
  EEPROM.commit();
}

String controllersToString() {
  String s = String();
  int c = 0;
  for (int i = 0; i < controllers.cap; i++) {
    if (controllers.list[i].name[0] == 0)
      continue;

    s.concat(String(controllers.list[i].name) + ";" +
             String(controllers.list[i].remoteId) + ";" +
             String(controllers.list[i].rollingCode));
    if (c < controllers.count - 1)
      s.concat('\n');
  }
  return s;
}

u_int8_t getControllerCount() { return controllers.count; }

void listToByteArray(byte *b) {
  int c = 0;
  for (int i = 0; i < controllers.cap; i++) {
    if (controllers.list[i].name[0] != 0) {
      memcpy(b + (c++) * sizeof(Controller), &controllers.list[i],
             sizeof(Controller));
    }
  }
}

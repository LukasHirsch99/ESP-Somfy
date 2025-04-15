#include <Controllers.h>

ControllerList controllers; // EEPROM structure

int getMaxRemoteNumber() {
  int max = 10;
  for (int i = 0; i < controllers.count; i++) {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (max < controllers.list[i].remoteId)
      max = controllers.list[i].remoteId;
  }
  return max;
}

bool updateName(Controller *c, const char *newName, size_t nameLen) {
  if (c == 0 || nameLen >= MAX_NAME_LEN - 1)
    return false;

  Serial.println("Renaming: " + String(c->name));

  strncpy(c->name, newName, nameLen);
  c->name[nameLen] = 0;
  Serial.println("New name: " + String(c->name));
  saveControllers();
  return true;
}

Controller *findControllerByName(const char *name) {
  for (int i = 0; i < controllers.size; i++) {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (strcmp(controllers.list[i].name, name) == 0)
      return &controllers.list[i];
  }
  return 0;
}

Controller *findControllerByRemoteId(int remoteId) {
  for (int i = 0; i < controllers.size; i++) {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (controllers.list[i].remoteId == remoteId)
      return &controllers.list[i];
  }
  return 0;
}

/// @returns -1 if controller already exists
/// @returns -2 if no more space available
/// @returns -3 invalid name length
int addController(const char *name, size_t nameLen, unsigned long rc,
                  int remoteId) {
  if (nameLen <= 0 || nameLen >= MAX_NAME_LEN - 1)
    return -3;

  if (findControllerByName(name) != 0) {
    Serial.println("Controller already exists");
    return -1;
  }
  for (int i = 0; i < controllers.size; i++) {
    if (controllers.list[i].name[0] != 0)
      continue;

    strncpy(controllers.list[i].name, name, nameLen);
    controllers.list[i].name[nameLen] = 0;
    controllers.list[i].remoteId =
        remoteId != 0 ? remoteId : getMaxRemoteNumber() + 1;
    controllers.list[i].rollingCode = rc != 0 ? rc : 1;

    if (i >= controllers.count)
      controllers.count++;
    saveControllers();
    return 0;
  }
  Serial.println("No available Controller space");
  return -2; // All available Controllers used
}

bool deleteControllerByName(const char *name) {
  for (int i = 0; i < controllers.size; i++) {
    if (strcmp(controllers.list[i].name, name) == 0) {
      return deleteController(&controllers.list[i]);
    }
  }
  return false; // controller not found
}

bool deleteController(Controller *c) {
  c->name[0] = 0;
  c->remoteId = -1;
  c->rollingCode = 1;
  saveControllers();
  return true;
}

void setupControllers() {
  EEPROM.begin(sizeof(controllers) + BASE_CONTROLLERLIST);
  EEPROM.get(BASE_CONTROLLERLIST, controllers);

  if (controllers.magicNumber != dMagicNumber) {
    Serial.println("Setting up controllers");
    controllers.magicNumber = dMagicNumber;
    controllers.size = mArraySize(controllers.list);
    controllers.count = 0;
    for (int i = 0; i < controllers.size; i++) {
      controllers.list[i].name[0] = 0;
      controllers.list[i].remoteId = 0;
      controllers.list[i].rollingCode = 1;
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
  for (int i = 0; i < controllers.size; i++) {
    if (controllers.list[i].name[0] == 0)
      continue;
    s.concat(String(controllers.list[i].name) + ";" +
             String(controllers.list[i].remoteId) + ";" +
             String(controllers.list[i].rollingCode) + "\n");
  }
  return s;
}

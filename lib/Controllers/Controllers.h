#ifndef REMOTES_H
#define REMOTES_H

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

#define REMOTE_COUNT 20
#define BASE_CONTROLLERLIST 16 // Byte offset
#define dMagicNumber 0xaffeaffe
#define MAX_NAME_LEN 30

#define mArraySize(a) (sizeof(a) / sizeof(a[0]))

#pragma pack(push, 1)

typedef struct Controller {
  u_int32_t remoteId;
  u_int32_t rollingCode;
  char name[MAX_NAME_LEN]; // empty name = free item
} Controller;

typedef struct ControllerList {
  u_int32_t magicNumber; // dMagicNumber
  u_int8_t count;        // current count of items in list
  u_int8_t cap;          // maximum capacity of list
  Controller list[REMOTE_COUNT];
} ControllerList;

#pragma pack(pop)

u_int32_t getMaxRemoteNumber();
bool updateName(Controller *c, const char *newName);
Controller *findControllerByName(const char *name);
Controller *findControllerByRemoteId(u_int32_t remoteId);
Controller *getControllerByIndex(u_int32_t idx);
int addController(const char *name, u_int32_t rc, u_int32_t remoteId);
bool deleteControllerByName(const char *name);
void deleteController(Controller *c);
void setupControllers();
void saveControllers();
void saveRollingCode(Controller *c);
String controllersToString();
u_int8_t getControllerCount();
void listToByteArray(byte *b);

#endif

#ifndef REMOTES_H
#define REMOTES_H

#include <Arduino.h>
#include <EEPROM.h>

#define REMOTE_COUNT 20
#define BASE_CONTROLLERLIST 16 // Byte offset
#define dMagicNumber 0xaffeaffe
#define MAX_NAME_LEN 30

#define mArraySize(a) (sizeof(a) / sizeof(a[0]))

// Maybe try to remove name from this structure
// and only save the name on hassio
typedef struct Controller {
  int remoteId; // remoteId = -1: free item
  unsigned long rollingCode;
  char name[MAX_NAME_LEN]; // empty name = free item
} Controller;

typedef struct ControllerList {
  unsigned int magicNumber; // dMagicNumber
  int size;                 // size of list
  int count;                // current count of items in list
  Controller list[REMOTE_COUNT];
} ControllerList;

int getMaxRemoteNumber();
bool updateName(Controller *c, const char *newName, size_t nameLen);
Controller *findControllerByName(const char *name);
Controller *findControllerByRemoteId(int remoteId);
int addController(const char *name, size_t nameLen, unsigned long rc, int remoteId);
bool deleteControllerByName(const char *name);
bool deleteController(Controller *c);
void setupControllers();
void saveControllers();
void saveRollingCode(Controller *c);
String controllersToString();

#endif

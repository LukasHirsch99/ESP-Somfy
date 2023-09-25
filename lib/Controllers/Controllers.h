#ifndef REMOTES_H
#define REMOTES_H

#include <Arduino.h>
#include <string.h>
#include <EEPROM.h>

#define REMOTE_COUNT 20
#define BASE_CONTROLLERLIST 16 // Byte offset
#define dMagicNumber 0xaffaaffa

#define mArraySize(a) (sizeof(a) / sizeof(a[0]))

typedef struct Controller
{
  int remoteId;
  unsigned long rollingCode;
  char base_topic[100];
  char name[30]; // empty name = free item
} Controller;

typedef struct ControllerList
{
  unsigned int magicNumber; // dMagicNumber
  int size;        // size of list
  int count;       // current count of items in list
  Controller list[REMOTE_COUNT];
} ControllerList;

int getMaxRemoteNumber();
bool updateName(Controller *c, const char *newName);
Controller *findControllerByTopic(const char *topic);
Controller *findControllerByName(const char *name);
Controller *addController(const char *name, const char *base_topic);
bool deleteController(const char *name);
void setupControllers();
void saveControllers();
void saveRollingCode(Controller *c);

#endif
#ifndef SOMFY_PROTOCOL
#define SOMFY_PROTOCOL

#include <Arduino.h>
#include <Controllers.h>

#define SYMBOL 640 // [usec]
#define transmitPin D4

#define STOP 0x1
#define UP 0x2
#define DOWN 0x4
#define PROG 0x8
#define DEL 0x8

void sendCommand(byte command, Controller *c);
void sendCustomCommand(byte sync);
void buildFrame(byte command, Controller* c);
void buildCustomFrame(byte command, int remoteId, unsigned long rollingCode);

#endif
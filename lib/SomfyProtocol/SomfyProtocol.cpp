#include "../../include/config.h"
#include <SomfyProtocol.h>

byte frame[7];
byte checksum;

void sendCommand(byte command, Controller *c) {
  buildFrame(command, c);

  sendCustomCommand(2);
  for (int i = 0; i < 2; i++) {
    sendCustomCommand(7);
  }
}

void sendCustomCommand(byte sync) {
  if (sync == 2) { // Only with the first frame.
    // Wake-up pulse & Silence
    digitalWrite(TRANSMIT_PIN, HIGH);
    delayMicroseconds(9415);
    digitalWrite(TRANSMIT_PIN, LOW);
    delayMicroseconds(89565);
  }

  // Hardware sync: two sync for the first frame, seven for the following ones.
  for (int i = 0; i < sync; i++) {
    digitalWrite(TRANSMIT_PIN, HIGH);
    delayMicroseconds(4 * SYMBOL);
    digitalWrite(TRANSMIT_PIN, LOW);
    delayMicroseconds(4 * SYMBOL);
  }

  // Software sync
  digitalWrite(TRANSMIT_PIN, HIGH);
  delayMicroseconds(4550);
  digitalWrite(TRANSMIT_PIN, LOW);
  delayMicroseconds(SYMBOL);

  // Data: bits are sent one by one, starting with the MSB.
  for (byte i = 0; i < 56; i++) {
    if (((frame[i / 8] >> (7 - (i % 8))) & 1) == 1) {
      digitalWrite(TRANSMIT_PIN, LOW);
      delayMicroseconds(SYMBOL);
      digitalWrite(TRANSMIT_PIN, HIGH);
      delayMicroseconds(SYMBOL);
    } else {
      digitalWrite(TRANSMIT_PIN, HIGH);
      delayMicroseconds(SYMBOL);
      digitalWrite(TRANSMIT_PIN, LOW);
      delayMicroseconds(SYMBOL);
    }
  }

  digitalWrite(TRANSMIT_PIN, LOW);
  delayMicroseconds(30415); // Inter-frame silence
}

void buildFrame(byte command, Controller *c) {
  buildCustomFrame(command, c->remoteId, c->rollingCode);
  saveRollingCode(c);
}

void buildCustomFrame(byte command, u_int32_t remoteId, u_int32_t rollingCode) {
  frame[1] =
      command
      << 4; // Which button did  you press? The 4 LSB will be the checksum
  frame[2] = rollingCode >> 8; // Rolling code (big endian)
  frame[3] = rollingCode;      // Rolling code
  frame[4] = remoteId >> 16;   // Remote address
  frame[5] = remoteId >> 8;    // Remote address
  frame[6] = remoteId;         // Remote address

  // Checksum calculation: a XOR of all the nibbles
  checksum = 0;
  for (byte i = 0; i < 7; i++) {
    checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
  }
  checksum &= 0b1111; // We keep the last 4 bits only

  // Checksum integration
  frame[1] |=
      checksum; //  If a XOR of all the nibbles is equal to 0, the blinds will
                // consider the checksum ok.

  // Obfuscation: a XOR of all the bytes
  for (byte i = 1; i < 7; i++) {
    frame[i] ^= frame[i - 1];
  }
}

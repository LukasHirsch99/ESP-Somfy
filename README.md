# Somfy RTS Remote Emulation

This project turns a normal EPS-8266 paired with a RTS module into a somfy rts remote controller.

The aim of this project is to provide a IoT capable remote controller for somfy rts devices.

A breakdown of the protocol can be found here:
- https://pushstack.wordpress.com/somfy-rts-protocol/

Similar code snippets can be found all over github.

This project combines the somfy protocol with a small tcp server to receive commands over the network.

You can either write your own client or use my library for python:
- [somfy-rts-hub](https://github.com/LukasHirsch99/somfy-rts-hub)

The tcp server also supports mDNS to discover this device on the network.
- Service: `somfy-socks`
- Protocol: `tcp`


## The protocol

All requests start with a header followed by an optional body.

### The header

The header consists of two parts:
  1. `magicnumber`: This is 2 bytes and has to be `0xAFFE`
  2. `opcode`: This is 1 byte and has to be one of the valid [opcodes](#opcodes)


#### opcodes

- `1`: [get all covers](#get-covers)
- `2`: [send command to cover](#cover-commands)
- `3`: [add a cover](#add-cover)
- `4`: [rename a cover](#rename-cover)
- `5`: [send a custom command](#custom-command)

### Body

The body depends on the opcode.

#### get covers
No need to send a body

#### cover commands

With this you control the covers.
Make them go up, down or stop them where they are.

1. `remoteId`: 4 bytes unsigned int to identify the cover
2. `command`: 1 byte indicating the (command)[#commands]

##### Commands
- `stop`: 1
- `up`: 2
- `down`: 3
- `add / del`: 8

The `stop` corresponds to the `my` button on the remote.

#### add cover
With this body you create a new cover and store it to the EEPROM of the ESP. 

1. `remoteId`: 4 bytes unsigned int
2. `rollingCode`: 4 bytes unsigned int
3. `name`: 30 bytes long string

The remoteId and rollingCode are optional, if you want the ESP to choose a remoteId
and set the rollingCode to 0 just set them to `0`.

The name has to be 30 bytes, non empty and zero-padded.

#### rename cover
With this body you rename an existing cover and store the new name to the EEPROM of the ESP. 

1. `remoteId`: 4 bytes unsigned int
2. `name`: 30 bytes long string

#### custom command
This can be useful for finding the remoteId or rollingCode of a forgotten controller.

1. `remoteId`: 4 bytes unsigned int
2. `rollingCode`: 4 bytes unsigned int
3. `command`: 1 byte indicating the (command)[#commands]
4. `frameRepeat`: 1 byte

`frameRepeat` corresponds to how long the button is pressed, so you can emulate button holds.
The default is 2.

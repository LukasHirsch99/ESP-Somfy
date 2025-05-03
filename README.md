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
  1. `MagicNumber`: This is 2 bytes and has to be `0xAFFE`
  2. `Opcode`: This is 1 byte and has to be one of the valid [Opcodes](#opcodes)


#### Opcodes

`1`: get all covers
`2`: send command to cover
`3`: add a cover
`4`: rename a cover
`5`: send a custom command

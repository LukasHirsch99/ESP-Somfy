# Somfy Remote Emulation

## MQTT-Commands

### Sending Commands via mosquitto_pub

`mosquitto_pub -L mqtt://mqtt-user:sweethome@homeassistant.local:1883/[topic] -m [payload]`


Whenever an error occurs, it is sent to 'home/somfy/error'

### List all Controllers
topic: **'home/somfy/getControllers'**

payload: **anything**

Sends all Controllers plus their info to **'home/somfy/log'**

### Adding Controller
topic: **'home/somfy/add'**

payload: \<name of controller>


### Renaming Controller
topic: **'home/somfy/\<name of controller>/rename'**

payload: \<new name of controller>

### Using Controller

topic: **'home/somfy/\<name of controller>/cmd'**

payload: 'u' | 'd' | 's' | 'r'

'u': up \
'd': down \
's': stop \
'r': remove cover

### Custom Command
topic: **'home/somfy/custom'**

payload:
```json
{
  "remoteId": <remote id>,
  "rollingCode": <rolling code>,
  "command": <one of the 4 commands>,
  "frameRepeat": <How often the frame gets sent>
}
```

### Adding a Custom Controller
topic: **'home/somfy/addCustom'**

payload:
```json
{
  "remoteId": <remote id>,
  "rollingCode": <rolling code>
}
```

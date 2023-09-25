# Somfy Remote Emulation

## MQTT-Commands

Whenever an error occurs, it is sent to 'home/somfy/error'

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
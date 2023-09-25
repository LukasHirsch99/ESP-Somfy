#include <Controllers.h>

ControllerList controllers; // EEPROM structure

int getMaxRemoteNumber()
{
  int max = 10;
  for (int i = 0; i < controllers.count; i++)
  {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (max < controllers.list[i].remoteId)
      max = controllers.list[i].remoteId;
  }
  return max;
}

bool updateName(Controller *c, const char *newName)
{
  Serial.println("Renaming: " + String(c->name));

  if (c == 0)
    return false;
  
  strcpy(c->name, newName);
  char baseTopic[50] = "home/somfy/";
  strcat(baseTopic, newName);
  strcpy(c->base_topic, baseTopic);
  Serial.println(c->name);
  Serial.println(c->base_topic);
  saveControllers();
  return true;
}

Controller *findControllerByTopic(const char *topic)
{
  int i = 0;
  for (i = 0; i < controllers.size; i++)
  {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (strcmp(controllers.list[i].base_topic, topic) == 0)
      return &controllers.list[i];
  }
  return 0;
}

Controller *findControllerByName(const char *name)
{
  int i = 0;
  for (i = 0; i < controllers.size; i++)
  {
    if (controllers.list[i].name[0] == 0)
      continue;
    if (strcmp(controllers.list[i].name, name) == 0)
      return &controllers.list[i];
  }
  return 0;
}

Controller *addController(const char *name, const char *base_topic)
{
  if (findControllerByTopic(base_topic) != 0)
  {
    Serial.println("Controller already existing");
    return 0;
  }
  for (int i = 0; i < controllers.size; i++)
  {
    if (controllers.list[i].name[0] == 0)
    {
      strcpy(controllers.list[i].name, name);
      strcpy(controllers.list[i].base_topic, base_topic);
      controllers.list[i].remoteId = getMaxRemoteNumber() + 1;
      controllers.list[i].rollingCode = 1;
      if (i >= controllers.count)
        controllers.count++;
      saveControllers();
      return &controllers.list[i];
    }
  }
  Serial.println("No available Controller space");
  return 0; // All available Controllers used
}

bool deleteController(const char *name)
{
  for (int i = 0; i < controllers.size; i++)
  {
    if (strcmp(controllers.list[i].name, name) == 0)
    {
      controllers.list[i].name[0] = 0;
      controllers.list[i].base_topic[0] = 0;
      controllers.list[i].remoteId = 0;
      controllers.list[i].rollingCode = 1;
      saveControllers();
      return true;
    }
  }
  return false; // All available Controllers used
}

void setupControllers()
{
  Serial.println("Setting up controllers");
  int i;

  EEPROM.begin(sizeof(controllers) + BASE_CONTROLLERLIST);
  EEPROM.get(BASE_CONTROLLERLIST, controllers);

  if (controllers.magicNumber != dMagicNumber)
  {
    controllers.magicNumber = dMagicNumber;
    controllers.size = mArraySize(controllers.list);
    controllers.count = 0;
    for (i = 0; i < controllers.size; i++)
    {

      controllers.list[i].name[0] = 0;
      controllers.list[i].base_topic[0] = 0;
      controllers.list[i].remoteId = 0;
      controllers.list[i].rollingCode = 1;
    }
    saveControllers();
  }
  for (i = 0; i < controllers.size; i++)
  {
    if (controllers.list[i].name[0] != 0)
    {
      Serial.print(controllers.list[i].name);
      Serial.print(": ");
      Serial.print(controllers.list[i].base_topic);
      Serial.print(", ");
      Serial.print(controllers.list[i].remoteId);
      Serial.print(": ");
      Serial.println(controllers.list[i].rollingCode);
    }
  }
}

void saveControllers()
{
  EEPROM.put(BASE_CONTROLLERLIST, controllers);
  EEPROM.commit();
}

void saveRollingCode(Controller *c) // idx = controller index
{
  int offset = (char *)&c->rollingCode - (char *)&controllers;
  offset += BASE_CONTROLLERLIST;
  c->rollingCode++; // increment!
  EEPROM.put(offset, c->rollingCode);
  EEPROM.commit();
}
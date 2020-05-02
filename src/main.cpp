#include <Homie.h>
#include <Shutters.h>
#include <EEPROM.h>

#include <EnemDoubleButton.hpp>

const int SHUTTER_PIN_BUTTON_DOWN = 0;
const int SHUTTER_PIN_BUTTON_UP = 9;
const int SHUTTER_PIN_RELAY_DOWN = 12;
const int SHUTTER_PIN_RELAY_UP = 5;
const int BUTTON_PIN_CASE = 10;
const int LED_PIN_STATUS = 13;
const int SHUTTER_UPCOURSETIME_SEC = 120000;
const int SHUTTER_DOWNCOURSETIME_SEC = 120000;
const float SHUTTER_CALIBRATION_RATIO = 0.1;

uint8_t rebootCount = 0;
unsigned long disconnectedTime = 0;
unsigned long const maxDisconnectedTime = 20000;

unsigned long buttonLockStartTime = 0;
unsigned int buttonLockDuration = 0;
unsigned long publishingButtonLockRemaining = ULONG_MAX;
unsigned long lastPublishButtonLock = 0;

EnemDoubleButton button = EnemDoubleButton(SHUTTER_PIN_BUTTON_UP, SHUTTER_PIN_BUTTON_DOWN, 60, 100, 1000);
Shutters shutter;

byte publishingLevel = ShuttersInternal::LEVEL_NONE;
unsigned long publishingUpCourseTime = 0;
unsigned long publishingDownCourseTime = 0;

HomieNode voletNode("shutters", "shutters", "shutters");

bool positiveIntTryParse(const String& value, unsigned long& out)
{
  for (byte i = 0; i < value.length(); i++) {
    if (isDigit(value.charAt(i)) == false) return false;
  }

  out = value.toInt();

  return true;
}

bool voletLevelHandler(const HomieRange& range, const String& value)
{
  unsigned long level;
  if(!positiveIntTryParse(value, level)) { return false; }

  if (level > 100) return false;

  shutter.setLevel(level);

  return true;
}

bool voletUpCommandHandler(const HomieRange& range, const String& value)
{
  shutter.setLevel(0);
  return true;
}

bool voletDownCommandHandler(const HomieRange& range, const String& value)
{
  shutter.setLevel(100);
  return true;
}

bool voletStopCommandHandler(const HomieRange& range, const String& value)
{
  shutter.stop();
  return true;
}

bool buttonLockCommandHandler(const HomieRange& range, const String& value)
{
  unsigned long duration;
  if(!positiveIntTryParse(value, duration)) { return false; }

  // Entre 10 secondes et 1h
  if (duration > 60*60 || duration < 10) return false;

  lastPublishButtonLock = 0;
  
  if(duration == 0)
  {
    buttonLockStartTime = 0;
  }
  else
  {
    buttonLockStartTime = millis();
    buttonLockDuration = duration * 1000;
  }

  return true;
}

bool voletupCourseTimeHandler(const HomieRange& range, const String& value)
{
  unsigned long upCourseTime;
  if(!positiveIntTryParse(value, upCourseTime)) { return false; }

  //Garde fou : plus d'une minute = foutage de gueule, pas moins de 5 secondes, = foutage de gueule aussi !
  if(upCourseTime > SHUTTER_UPCOURSETIME_SEC) { return false; }
  if(upCourseTime < 5000) { return false; }

  unsigned long downCourseTime = shutter.getDownCourseTime();

  shutter
    .reset()
    .setCourseTime(upCourseTime, downCourseTime)
    .begin();

  publishingUpCourseTime = upCourseTime;

  return true;
}

bool voletdownCourseTimeHandler(const HomieRange& range, const String& value)
{
  unsigned long downCourseTime;
  if(!positiveIntTryParse(value, downCourseTime)) { return false; }

  //Garde fou : plus d'une minute = foutage de gueule, pas moins de 5 secondes, = foutage de gueule aussi !
  if(downCourseTime > SHUTTER_DOWNCOURSETIME_SEC) { return false; }
  if(downCourseTime < 5000) { return false; }

  unsigned long upCourseTime = shutter.getUpCourseTime();

  shutter
    .reset()
    .setCourseTime(upCourseTime, downCourseTime)
    .begin();

  publishingDownCourseTime = downCourseTime;

  return true;
}

void onShuttersLevelReached(Shutters* shutter, byte level) {
  Serial.print("Shutters at ");
  Serial.print(level);
  Serial.println("%");

  publishingLevel = level;
}

void shuttersOperationHandler(Shutters* shutter, ShuttersOperation operation) {
  switch (operation) {
    case ShuttersOperation::UP:
      Serial.println("Shutters going up.");
      digitalWrite(SHUTTER_PIN_RELAY_DOWN, LOW);
      digitalWrite(SHUTTER_PIN_RELAY_UP, HIGH);
      break;
    case ShuttersOperation::DOWN:
      Serial.println("Shutters going down.");
      digitalWrite(SHUTTER_PIN_RELAY_UP, LOW);
      digitalWrite(SHUTTER_PIN_RELAY_DOWN, HIGH);
      break;
    case ShuttersOperation::HALT:
      Serial.println("Shutters halting.");
      digitalWrite(SHUTTER_PIN_RELAY_UP, LOW);
      digitalWrite(SHUTTER_PIN_RELAY_DOWN, LOW);
      break;
  }
}

void readInEeprom(char* dest, byte length) {
  int offset = 0;

  for (byte i = 0; i < length; i++) {
    dest[i] = EEPROM.read(offset + i);
  }
}

void shuttersWriteStateHandler(Shutters* shutter, const char* state, byte length) {
  int offset = 0;

  for (byte i = 0; i < length; i++) {
    EEPROM.write(offset + i, state[i]);
    EEPROM.commit();
  }
}

void readRebootCount() {
  int offset = shutter.getStateLength();
  rebootCount = EEPROM.read(offset);
}

void writeRebootCount() {
  int offset = shutter.getStateLength();
  EEPROM.write(offset, rebootCount);
  EEPROM.commit();
}

void loopHandler() {
  if(publishingLevel != ShuttersInternal::LEVEL_NONE)
  {
    voletNode.setProperty("level").send(String(publishingLevel));
    publishingLevel = ShuttersInternal::LEVEL_NONE;
  }

  if(publishingUpCourseTime != 0)
  {
    voletNode.setProperty("upCourseTime").send(String(publishingUpCourseTime));
    publishingUpCourseTime = 0;
  }

  if(publishingDownCourseTime != 0)
  {
    voletNode.setProperty("downCourseTime").send(String(publishingDownCourseTime));
    publishingDownCourseTime = 0;
  }

  if(publishingButtonLockRemaining != ULONG_MAX)
  {
    voletNode.setProperty("buttonLock").send(String(publishingButtonLockRemaining / 1000));
    publishingButtonLockRemaining = ULONG_MAX;
  }
}

void upPressed(EnemDoubleButton* button)
{
  if(buttonLockStartTime != 0) { return; }
  shutter.setLevel(0);
  voletNode.setProperty("log").send("upPressed");
}

void downPressed(EnemDoubleButton* button)
{
  if(buttonLockStartTime != 0) { return; }
  shutter.setLevel(100);
  voletNode.setProperty("log").send("downPressed");
}

void stopPressed(EnemDoubleButton* button)
{
  if(buttonLockStartTime != 0) { return; }
  shutter.stop();
  voletNode.setProperty("log").send("stopPressed");
}

void upDoublePressed(EnemDoubleButton* button)
{
  if(buttonLockStartTime != 0) { return; }
  voletNode.setProperty("upCommand").setRetained(false).send("true");
  voletNode.setProperty("log").send("upDoublePressed");
}

void downDoublePressed(EnemDoubleButton* button)
{
  if(buttonLockStartTime != 0) { return; }
  voletNode.setProperty("downCommand").setRetained(false).send("true");
  voletNode.setProperty("log").send("downDoublePressed");
}

void stopDoublePressed(EnemDoubleButton* button)
{
  if(buttonLockStartTime != 0) { return; }
  voletNode.setProperty("stopCommand").setRetained(false).send("true");
  voletNode.setProperty("log").send("stopDoublePressed");
}

void onHomieEvent(const HomieEvent& event) {
  switch(event.type) {
    case HomieEventType::MQTT_READY:
      rebootCount = 0;
      writeRebootCount();
      Serial.println("Connect back to normal");
      break;
    default:
      break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial << endl << endl;
  EEPROM.begin(512);

  readRebootCount();
  rebootCount++;
  writeRebootCount();

  Homie_setFirmware("EnemHomieSonoffDualShutter", "1.1.0");
  Homie.setLoopFunction(loopHandler);
  Homie.setLedPin(LED_PIN_STATUS, LOW).setResetTrigger(BUTTON_PIN_CASE, LOW, 5000);
  Homie.onEvent(onHomieEvent);
  Homie.setup();

  voletNode.advertise("level").settable(voletLevelHandler);
  voletNode.advertise("upCourseTime").settable(voletupCourseTimeHandler);
  voletNode.advertise("downCourseTime").settable(voletdownCourseTimeHandler);
  voletNode.advertise("upCommand").settable(voletUpCommandHandler);
  voletNode.advertise("downCommand").settable(voletDownCommandHandler);
  voletNode.advertise("stopCommand").settable(voletStopCommandHandler);
  voletNode.advertise("buttonLock").settable(buttonLockCommandHandler);

  char storedShuttersState[shutter.getStateLength()];
  readInEeprom(storedShuttersState, shutter.getStateLength());

  shutter
    .setOperationHandler(shuttersOperationHandler)
    .setWriteStateHandler(shuttersWriteStateHandler)
    .restoreState(storedShuttersState);

  if(shutter.getUpCourseTime() == 0)
  {
    publishingUpCourseTime = SHUTTER_UPCOURSETIME_SEC;
  }
  else
  {
    publishingUpCourseTime = shutter.getUpCourseTime();
  }

  if(shutter.getDownCourseTime() == 0)
  {
    publishingDownCourseTime = SHUTTER_DOWNCOURSETIME_SEC;
  }
  else
  {
    publishingDownCourseTime = shutter.getDownCourseTime();
  }

  shutter
    .setCourseTime(publishingUpCourseTime, publishingDownCourseTime)
    .setCalibrationRatio(SHUTTER_CALIBRATION_RATIO)
    .onLevelReached(onShuttersLevelReached)
    .begin();

  button.setup();

  button.setUpPressHandler(&upPressed);
  button.setDownPressHandler(&downPressed);
  button.setStopPressHandler(&stopPressed);
  button.setUpDoublePressHandler(&upDoublePressed);
  button.setDownDoublePressHandler(&downDoublePressed);
  button.setStopDoublePressHandler(&stopDoublePressed);

  digitalWrite(SHUTTER_PIN_RELAY_UP, LOW);
  digitalWrite(SHUTTER_PIN_RELAY_DOWN, LOW);

  pinMode(SHUTTER_PIN_RELAY_UP,OUTPUT);
  pinMode(SHUTTER_PIN_RELAY_DOWN,OUTPUT);
}

void HomieIndependentLoop()
{
  button.loop();
  shutter.loop();

  if(buttonLockStartTime != 0)
  {
    if(millis() - lastPublishButtonLock >= 10000)
    {
      publishingButtonLockRemaining = buttonLockDuration - (millis() - buttonLockStartTime);
      if(publishingButtonLockRemaining < 0) { publishingButtonLockRemaining = 0; }
      lastPublishButtonLock = millis();
    }

    if(millis() - buttonLockStartTime >= buttonLockDuration)
    {
      buttonLockStartTime = 0;
      buttonLockDuration = 0;
      publishingButtonLockRemaining = 0;
    }
  }
}

void loop() {
  Homie.loop();

  if(rebootCount >= 3)
  {
    if(Homie.isConfigured())
    {
      if (Homie.isConnected())
      {
        disconnectedTime = 0;
      }
      else
      {
        if(disconnectedTime == 0)
        {
          disconnectedTime = millis();
        }
        else
        {
          if(millis() - disconnectedTime >= maxDisconnectedTime)
          {
            rebootCount = 0;
            writeRebootCount();
            Homie.reset();
          }
        }
      }
    }
  }

  HomieIndependentLoop();
}

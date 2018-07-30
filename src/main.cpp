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
const int SHUTTER_UPCOURSETIME_SEC = 60000;
const int SHUTTER_DOWNCOURSETIME_SEC = 60000;
const float SHUTTER_CALIBRATION_RATIO = 0.1;

EnemDoubleButton button = EnemDoubleButton(SHUTTER_PIN_BUTTON_UP, SHUTTER_PIN_BUTTON_DOWN, 60, 100, 1000);
Shutters shutter;

byte publishingLevel = ShuttersInternal::LEVEL_NONE;
unsigned long publishingUpCourseTime = 0;
unsigned long publishingDownCourseTime = 0;

HomieNode voletNode("shutters", "shutters");

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

bool voletupCourseTimeHandler(const HomieRange& range, const String& value)
{
  unsigned long upCourseTime;
  if(!positiveIntTryParse(value, upCourseTime)) { return false; }

  //Garde fou : plus d'une minute = foutage de gueule, pas moins de 5 secondes, = foutage de gueule aussi !
  if(upCourseTime > 60000) { return false; }
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
  if(downCourseTime > 60000) { return false; }
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
}

void upPressed(EnemDoubleButton* button)
{
  shutter.setLevel(0);
}

void downPressed(EnemDoubleButton* button)
{
  shutter.setLevel(100);
}

void stopPressed(EnemDoubleButton* button)
{
  shutter.stop();
}

void upDoublePressed(EnemDoubleButton* button)
{
  voletNode.setProperty("externalUpCommand").setRetained(false).send("true");
}

void downDoublePressed(EnemDoubleButton* button)
{
  voletNode.setProperty("externalDownCommand").setRetained(false).send("true");
}

void stopDoublePressed(EnemDoubleButton* button)
{
  voletNode.setProperty("externalStopCommand").setRetained(false).send("true");
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial << endl << endl;
  EEPROM.begin(512);

  Homie_setFirmware("sonoffdual-shutters", "1.0.0");
  Homie.setLoopFunction(loopHandler);
  Homie.setLedPin(LED_PIN_STATUS, LOW).setResetTrigger(BUTTON_PIN_CASE, LOW, 5000);
  Homie.setup();

  voletNode.advertise("level").settable(voletLevelHandler);
  voletNode.advertise("upCourseTime").settable(voletupCourseTimeHandler);
  voletNode.advertise("downCourseTime").settable(voletdownCourseTimeHandler);
  voletNode.advertise("externalUpCommand");
  voletNode.advertise("externalDownCommand");
  voletNode.advertise("externalStopCommand");

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
}

void loop() {
  Homie.loop();
  HomieIndependentLoop();
}

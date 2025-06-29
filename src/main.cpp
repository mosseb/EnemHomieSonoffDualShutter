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
const int SHUTTER_COURSETIME_MIN = 5000;
const int SHUTTER_COURSETIME_MAX = 120000;
const float SHUTTER_CALIBRATION_RATIO = 0.1;

uint8_t rebootCount = 0;
bool rebootIncremented = false;
unsigned long disconnectedTime = 0;
unsigned long const maxDisconnectedTimeUntilRebootIncr = 10000;

unsigned long buttonLockStartTime = 0;
unsigned int buttonLockDuration = 0;
unsigned long publishingButtonLockRemaining = ULONG_MAX;
unsigned long lastPublishButtonLock = 0;

EnemDoubleButton button = EnemDoubleButton(SHUTTER_PIN_BUTTON_UP, SHUTTER_PIN_BUTTON_DOWN, 30, 50, 1000);
Shutters shutter;

byte publishingLevel = 101;

HomieNode voletNode("shutters", "shutters", "shutters");
HomieSetting<long> upCourseTimeSetting("upCourseTime", "upCourseTime");
HomieSetting<long> downCourseTimeSetting("downCourseTime", "downCourseTime");

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

bool resetHandler(const HomieRange& range, const String& value)
{
  Serial.println("Reboot requested !");
  Serial.flush();
  ESP.restart();
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

void readRebootCount() {
  rebootCount = EEPROM.read(0);
}

void writeRebootCount() {
  EEPROM.write(0, rebootCount);
  EEPROM.commit();
}

void loopHandler() {
  if(publishingLevel != 101)
  {
    voletNode.setProperty("level").send(String(publishingLevel));
    publishingLevel = 101;
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
  voletNode.setProperty("upCommand").setRetained(false).send("false");
  voletNode.setProperty("log").send("upDoublePressed");
}

void downDoublePressed(EnemDoubleButton* button)
{
  if(buttonLockStartTime != 0) { return; }
  voletNode.setProperty("downCommand").setRetained(false).send("true");
  voletNode.setProperty("downCommand").setRetained(false).send("false");
  voletNode.setProperty("log").send("downDoublePressed");
}

void stopDoublePressed(EnemDoubleButton* button)
{
  if(buttonLockStartTime != 0) { return; }
  voletNode.setProperty("stopCommand").setRetained(false).send("true");
  voletNode.setProperty("stopCommand").setRetained(false).send("false");
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

void shuttersWriteStateHandler(Shutters* shutter, const char* state, byte length) {
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial << endl << endl;
  EEPROM.begin(4);

  readRebootCount();

  Homie_setFirmware("EnemHomieSonoffDualShutter", "1.3.0");
  Homie.setLoopFunction(loopHandler);
  Homie.setLedPin(LED_PIN_STATUS, LOW).setResetTrigger(BUTTON_PIN_CASE, LOW, 5000);
  Homie.onEvent(onHomieEvent);

  auto validator = [] (long candidate) {
    return (candidate >= SHUTTER_COURSETIME_MIN) && (candidate <= SHUTTER_COURSETIME_MAX);
  };

  upCourseTimeSetting.setValidator(validator);
  downCourseTimeSetting.setValidator(validator);

  Homie.setup();

  voletNode.advertise("level").settable(voletLevelHandler);
  voletNode.advertise("upCommand").settable(voletUpCommandHandler);
  voletNode.advertise("downCommand").settable(voletDownCommandHandler);
  voletNode.advertise("stopCommand").settable(voletStopCommandHandler);
  voletNode.advertise("buttonLock").settable(buttonLockCommandHandler);
  voletNode.advertise("reset").settable(resetHandler);

  auto state = ShuttersInternal::StoredState{};
  state.setDownCourseTime(downCourseTimeSetting.get());
  state.setUpCourseTime(upCourseTimeSetting.get());
  state.setLevel(ShuttersInternal::LEVEL_NONE);

  shutter
    .setOperationHandler(shuttersOperationHandler)
    .setWriteStateHandler(shuttersWriteStateHandler)
    .onLevelReached(onShuttersLevelReached)
    .restoreState(state.getState())
    .setCourseTime(upCourseTimeSetting.get(), downCourseTimeSetting.get())
    .setCalibrationRatio(SHUTTER_CALIBRATION_RATIO)
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

void checkResetNeeded() {
  // Homie not even configured, should show the AP already
  if(!Homie.isConfigured()) {
    return;
  }

  // Fully functionnal ! Go back to business
  if (Homie.isConnected()) {
    disconnectedTime = 0;
    if(rebootCount != 0) {
      rebootCount = 0;
      writeRebootCount();
    }
    return;
  }

  // Homie is configured but not connected. This is when we probably want to have a reset

  // If already incremented, do nothing but wait for device restart.
  if(rebootIncremented) {
    return;
  }

  // If reboot count reached 3, wipe !
  if(rebootCount >= 3) {
    rebootCount = 0;
    writeRebootCount();
    Homie.reset();
  }

  // Init the disconnectedTime
  if(disconnectedTime == 0) {
    disconnectedTime = millis();
    return;
  }

  // Diconnected time reached the limit, increment and wait for a device restart
  if(millis() - disconnectedTime >= maxDisconnectedTimeUntilRebootIncr) {
    rebootCount++;
    writeRebootCount();
    rebootIncremented = true;
  }
}

void loop() {
  Homie.loop();
  checkResetNeeded();
  HomieIndependentLoop();
}

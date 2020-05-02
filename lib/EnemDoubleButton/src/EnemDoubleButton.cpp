#include "EnemDoubleButton.hpp"

EnemDoubleButton::EnemDoubleButton(int pinUp, int pinDown, int delayBounce, int delayBoth, int delayDouble)
{
  this->pinUp = pinUp;
  this->pinDown = pinDown;
  this->delayBounce = delayBounce;
  this->delayBoth = delayBoth;
  this->delayDouble = delayDouble;
}

void EnemDoubleButton::setup()
{
  pinMode(pinUp, INPUT_PULLUP);
  upDebouncer.attach(pinUp);
  upDebouncer.interval(delayBounce);

  pinMode(pinDown, INPUT_PULLUP);
  downDebouncer.attach(pinDown);
  downDebouncer.interval(delayBounce);
}

void EnemDoubleButton::loop()
{
  upDebouncer.update();
  downDebouncer.update();

  bool upCurrent = !upDebouncer.read();
  bool downCurrent = !downDebouncer.read();

  // On a relaché le up
  if(upPressed && !upCurrent)
  {
    upPressed = doublePressed = false;
    upSince = 0;
    lastUp = true;
    upRelease();
    return;
  }

  // On a relaché le down
  if(downPressed && !downCurrent)
  {
    downPressed = doublePressed = false;
    downSince = 0;
    lastDown = true;
    downRelease();
    return;
  }

  // On a relaché les 2, c'est la fin du stop
  if(stopPressed && !upCurrent && !downCurrent)
  {
    stopPressed = doublePressed = false;
    lastStop = true;
    upSince = downSince = 0;
    stopRelease();
    return;
  }

  //Si on viens juste d'appuyer, on mémorise le début
  if(upCurrent && upSince == 0) { upSince = millis(); }
  //Si on viens juste de ne plus appuyer, on reset
  if(!upCurrent && upSince != 0) { upSince = 0; }
  //Dans les autre cas, on continue simplement de mémoriser Since

  //Si on viens juste d'appuyer, on mémorise le début
  if(downCurrent && downSince == 0) { downSince = millis(); }
  //Si on viens juste de ne plus appuyer, on reset
  if(!downCurrent && downSince != 0) { downSince = 0; }
  //Dans les autre cas, on continue simplement de mémoriser Since

  //Reset du timer rendant possible le double
  if(lastActionSince != 0 && millis() - lastActionSince >= delayDouble)
  {
    lastActionSince = 0;
    lastUp = lastDown = lastStop = false;
    Serial.println(String("doubleTimeout"));
  }

  if(!stopPressed && upCurrent && millis() - upSince >= delayBoth && downSince == 0)
  {
    if(!upPressed)
    {
      if(lastUp && lastActionSince != 0 && lastAction == 1 && (millis() - lastActionSince) < delayDouble)
      {
        doublePressed = true;
        lastActionSince = 0;
        upPress();
        upPressDouble();
      }
      else
      {
        lastActionSince = millis();
        lastAction = 1;
        upPress();
      }
      upPressed = true;
    }

    return;
  }

  if(!stopPressed && downCurrent && millis() - downSince >= delayBoth && upSince == 0)
  {
    if(!downPressed)
    {
      if(lastDown && lastActionSince != 0 && lastAction == -1 && (millis() - lastActionSince) < delayDouble)
      {
        doublePressed = true;
        lastActionSince = 0;
        downPress();
        downPressDouble();
      }
      else
      {
        lastActionSince = millis();
        lastAction = -1;
        downPress();
      }
      downPressed = true;
    }

    return;
  }

  if(upSince != 0 && downSince != 0 && (millis() - upSince >= delayBoth || millis() - downSince >= delayBoth))
  {
    if(!stopPressed)
    {
      if(lastStop && lastActionSince != 0 && lastAction == 0 && (millis() - lastActionSince) < delayDouble)
      {
        doublePressed = true;
        lastActionSince = 0;
        stopPress();
        stopPressDouble();
      }
      else
      {
        lastActionSince = millis();
        lastAction = 0;
        stopPress();
      }
      stopPressed = true;
    }

    return;
  }
}

bool EnemDoubleButton::isUpPressed(bool dbl)
{
  return dbl ? (doublePressed && upPressed) : (upPressed);
}

bool EnemDoubleButton::isDownPressed(bool dbl)
{
  return dbl ? (doublePressed && downPressed) : (downPressed);
}

bool EnemDoubleButton::isStopPressed(bool dbl)
{
  return dbl ? (doublePressed && stopPressed) : (stopPressed);
}

bool EnemDoubleButton::isDoublePressed()
{
  return doublePressed;
}

void EnemDoubleButton::setUpPressHandler(HandlerFunction newUpPressHandler)
{
  upPressHandler = newUpPressHandler;
}

void EnemDoubleButton::upPress()
{
  Serial.println(String("upPress"));
  (*upPressHandler)(this);
}

void EnemDoubleButton::upRelease()
{
  Serial.println(String("upRelease"));
}

void EnemDoubleButton::setUpDoublePressHandler(HandlerFunction newUpDoublePressHandler)
{
  upDoublePressHandler = newUpDoublePressHandler;
}

void EnemDoubleButton::upPressDouble()
{
  Serial.println(String("upPressDouble"));
  (*upDoublePressHandler)(this);
}

void EnemDoubleButton::setDownPressHandler(HandlerFunction newDownPressHandler)
{
  downPressHandler = newDownPressHandler;
}

void EnemDoubleButton::downPress()
{
  Serial.println(String("downPress"));
  (*downPressHandler)(this);
}

void EnemDoubleButton::downRelease()
{
  Serial.println(String("downRelease"));
}

void EnemDoubleButton::setDownDoublePressHandler(HandlerFunction newDownDoublePressHandler)
{
  downDoublePressHandler = newDownDoublePressHandler;
}

void EnemDoubleButton::downPressDouble()
{
  Serial.println(String("downPressDouble"));
  (*downDoublePressHandler)(this);
}

void EnemDoubleButton::setStopPressHandler(HandlerFunction newStopPressHandler)
{
  stopPressHandler = newStopPressHandler;
}

void EnemDoubleButton::stopPress()
{
  Serial.println(String("stopPress"));
  (*stopPressHandler)(this);
}

void EnemDoubleButton::stopRelease()
{
  Serial.println(String("stopRelease"));
}

void EnemDoubleButton::setStopDoublePressHandler(HandlerFunction newStopDoublePressHandler)
{
  stopDoublePressHandler = newStopDoublePressHandler;
}

void EnemDoubleButton::stopPressDouble()
{
  Serial.println(String("stopPressDouble"));
  (*stopDoublePressHandler)(this);
}

#include <Arduino.h>
#include <Bounce2.h>

class EnemDoubleButton;

typedef void (*HandlerFunction)(::EnemDoubleButton*);

class EnemDoubleButton
{
private:
  unsigned int delayBounce;
  unsigned int delayBoth;
  unsigned int delayDouble;

  int pinUp;
  Bounce upDebouncer;

  int pinDown;
  Bounce downDebouncer;

  unsigned long upSince = 0;
  unsigned long downSince = 0;

  unsigned long lastActionSince = 0;
  int lastAction;

  bool upPressed = false;
  bool downPressed = false;
  bool stopPressed = false;
  bool doublePressed = false;

  bool lastUp = false;
  bool lastDown = false;
  bool lastStop = false;

  void upPress();
  void upRelease();
  void upPressDouble();
  void downPress();
  void downRelease();
  void downPressDouble();
  void stopPress();
  void stopRelease();
  void stopPressDouble();

  HandlerFunction upPressHandler = 0;
  HandlerFunction downPressHandler = 0;
  HandlerFunction stopPressHandler = 0;

  HandlerFunction upDoublePressHandler = 0;
  HandlerFunction downDoublePressHandler = 0;
  HandlerFunction stopDoublePressHandler = 0;
public:

  EnemDoubleButton(int pinUp, int pinDown, int delayBounce, int delayBoth, int delayDouble);
  void setup();
  void loop();
  bool isUpPressed(bool dbl=false);
  bool isDownPressed(bool dbl=false);
  bool isStopPressed(bool dbl=false);
  bool isDoublePressed();
  void setUpPressHandler(HandlerFunction newUpPressHandler);
  void setDownPressHandler(HandlerFunction newDownPressHandler);
  void setStopPressHandler(HandlerFunction newStopPressHandler);
  void setUpDoublePressHandler(HandlerFunction newUpDoublePressHandler);
  void setDownDoublePressHandler(HandlerFunction newDownDoublePressHandler);
  void setStopDoublePressHandler(HandlerFunction newStopDoublePressHandler);
};

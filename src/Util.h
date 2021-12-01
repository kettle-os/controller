#include <Arduino.h>

#include "Adafruit_ILI9341esp.h"

class Util {
public:
  static void drawCenteredString(Adafruit_ILI9341 *pTft, const String &buf, int x, int y);
  static void drawLogo(Adafruit_ILI9341 *pTft);
};

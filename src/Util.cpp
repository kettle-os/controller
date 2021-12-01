
#include "Util.h"

void Util::drawCenteredString(Adafruit_ILI9341 *pTft, const String &buf, int x, int y)
{
    int16_t x1, y1;
    uint16_t w, h;
    pTft->getTextBounds(buf, 0, y, &x1, &y1, &w, &h); //calc width of new string
    pTft->setCursor(x - w / 2, y);
    pTft->println(buf);
}

void Util::drawLogo(Adafruit_ILI9341 *pTft) {
  pTft->setRotation(1);
  pTft->fillRect(10, 10, 300, 50, ILI9341_DARKGREEN);
  pTft->drawRect(10, 10, 300, 50, ILI9341_WHITE);
  pTft->setTextColor(ILI9341_MAGENTA, ILI9341_DARKGREEN);
  pTft->setTextSize(3);
  Util::drawCenteredString(pTft, "-- Kettle OS --", 160, 25);
  pTft->setTextColor(ILI9341_MAGENTA, ILI9341_BLACK);
  pTft->setTextSize(2);  
}

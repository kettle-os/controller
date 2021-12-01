
// Web server enabling initial configuration

#include <ESP8266WebServer.h>
#include "Adafruit_ILI9341esp.h"

class AccessPoint {
public:
  static void setTft(Adafruit_ILI9341 *pTft);
  static ESP8266WebServer *start();
  static ESP8266WebServer *pServer;
private:
  static Adafruit_ILI9341 *pTft;
};

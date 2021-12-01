
#include <LittleFS.h>
#include <json/FirebaseJson.h>
#include "AccessPoint.h"
#include "Util.h"

ESP8266WebServer *AccessPoint::pServer = NULL;
Adafruit_ILI9341 *AccessPoint::pTft = NULL;

// Set up for web configuration

static String sendHeader() {
  String html = "<!DOCTYPE html>\r\n";
  html += "<html>\r\n";
  html += "  <head>\r\n";
  html += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\r\n";
  html += "    <title>Kettle OS Configuration</title>\r\n";
  html += "    <style>\r\n";
  html += "      html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; }\r\n";
  html += "      body { margin-top: 50px; }\r\n";
  html += "      h1 { color: #444444; margin: 50px auto 30px; }\r\n";
  html += "      h3 { color: #444444; margin-bottom: 50px; }\r\n";
  html += "      .button { display: block; width: 160px; background-color: #1abc9c; border: 0; margin: 15px; color: white; padding: 13px 30px; text-decoration: none; font-size: 25px; cursor: pointer; border-radius: 4px; text-align: center; }\r\n";
  html += "      .button-on { background-color: #1abc9c; }\r\n";
  html += "      .button-on:active { background-color: #16a085; }\r\n";
  html += "      .button-off { background-color: #34495e; }\r\n";
  html += "      .button-off:active { background-color: #2c3e50; }\r\n";
  html += "      p { font-size: 14px;color: #888;margin-bottom: 10px; }\r\n";
  html += "    </style>\r\n";
  html += "  </head>\r\n";
  html += "  <body>\r\n";
  return html;
}

static String sendFooter() {
  String html = "  </body>\r\n";
  html += "</html>\r\n";
  return html;
}

static String sendConfigForm() {
  String html = sendHeader();
  html += "    <h1>Kettle OS Configuration</h1>\r\n";
  html += "    <h3>No WiFi configuration found</h3>\r\n";
  html += "    <p>Enter WiFi SSID and password to connect to Internet</p>\r\n";
  html += "    <form action=\"setConfig\" method=\"post\">\r\n";
  html += "      <table>\r\n";
  html += "        <tr><td><label for=\"SSID\">WiFi SSID</label></td><td><input name=\"SSID\" type=\"text\"/></td></tr>\r\n";
  html += "        <tr><td><label for=\"password\">WiFi Password</label></td><td><input name=\"password\" type=\"password\"/></td></tr>\r\n";
  html += "        <tr><td><label for=\"email\">Email Address</label></td><td><input name=\"email\" type=\"email\"/></td></tr>\r\n";
  html += "      </table>\r\n";
  html += "      <button name=\"submit\" type=\"submit\" class=\"button\">Submit</button>\r\n";
  html += "    </form>\r\n";
  html += sendFooter();
  return html;
}

static String sendInstructions(String email) {
  String html = sendHeader();
  html += "    <h1>Check your email (" + email + ") for instructions</h1>\r\n";
  html += sendFooter();
  return html;
}

static void handleIndex() {
  AccessPoint::pServer->send(200, "text/html", sendConfigForm());
}

void handleConfig() {
  ESP8266WebServer *pServer = AccessPoint::pServer;
  if (!pServer ||
      !pServer->hasArg("SSID") ||
      !pServer->hasArg("password") ||
      !pServer->hasArg("email") ||
      pServer->arg("SSID") == NULL ||
      pServer->arg("password") == NULL ||
      pServer->arg("email") == NULL) {
    pServer->send(400, "text/plain", "400: Invalid Request");
    return;
  }
  FirebaseJson json;
  json.add("SSID", pServer->arg("SSID"));
  json.add("password", pServer->arg("password"));
  json.add("email", pServer->arg("email"));
  File fWifi = LittleFS.open("/wifi.json", "w");
  json.toString(fWifi, true);
  fWifi.close();
  AccessPoint::pServer->send(200, "text/html", sendInstructions(pServer->arg("email")));
  ESP.restart();
}

void AccessPoint::setTft(Adafruit_ILI9341 *pTft) {
  AccessPoint::pTft = pTft;
}

ESP8266WebServer *AccessPoint::start() {
  if (!pTft) {
    return NULL;
  }
 
  // Display instructions

  Util::drawLogo(pTft);
  Util::drawCenteredString(pTft, "No config found", 160, 70);
  Util::drawCenteredString(pTft, "Visit Web config", 160, 100);
  Util::drawCenteredString(pTft, "SSID: kettle", 160, 130);
  char password[9];
  for (int i = 0; i < 8; i++) {
    password[i] = 'a' + random(0, 26);
  }
  password[8] = 0;
  char buf[32];
  sprintf(buf, "Password: %s", password);
  Util::drawCenteredString(pTft, buf, 160, 160);
  Util::drawCenteredString(pTft, "IP: 192.168.0.1", 160, 190);

  // Start server

  pServer = new ESP8266WebServer(80);
  IPAddress localIP(192,168,0,1);
  IPAddress gatewayIP(192,168,0,1);
  IPAddress subnetIP(255,255,255,0);
  WiFi.softAPConfig(localIP, gatewayIP, subnetIP);
  WiFi.softAP("kettle", password);
  pServer->on("/", handleIndex);
  pServer->on("/setConfig", handleConfig);
  pServer->begin();
  return pServer;
}

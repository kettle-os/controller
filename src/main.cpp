#include <Arduino.h>
#include <SPI.h>

#include <LittleFS.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <Wifi.h>
#endif

#include <json/FirebaseJson.h>

#include "Adafruit_GFX.h"
#include "XPT2046.h"

#include "AccessPoint.h"
#include "Util.h"

// GPIO pins for TFT and touchscreen

#define TFT_DC 2
#define TFT_CS 15
#define TOUCH_CS 4
#define TOUCH_IRQ 5

// WiFi parameters

#define WIFI_PARAM_FILE "/wifi.json"
#define WIFI_TIMEOUT 30000

// Firebase parameters

#define FIREBASE_PARAM_FILE "/service-account.json"
#define REQUEST_ID_FILE "/request-id.json"
#define REQUEST_ID_SIZE 32

// UI details

#define BUTTON_X 40
#define BUTTON_Y 100
#define BUTTON_W 60
#define BUTTON_H 30
#define BUTTON_SPACING_X 20
#define BUTTON_SPACING_Y 20
#define BUTTON_TEXTSIZE 2

// Text box where numbers go

#define TEXT_X 10
#define TEXT_Y 10
#define TEXT_W 220
#define TEXT_H 50
#define TEXT_TSIZE 3
#define TEXT_TCOLOR ILI9341_MAGENTA

// The data (phone #) we store in the textfield

#define TEXT_LEN 12
char textfield[TEXT_LEN+1] = "";
uint8_t textfield_i=0;

// TFT and touch screen objects

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
XPT2046 touch(TOUCH_CS, TOUCH_IRQ);

// Operation mode

typedef enum { NO_MODE, ACCESS_POINT, NO_WIFI, OTP_SENT, DISCONNECTED,
               UNAUTHENTICATED_CLIENT, AUTHENTICATED_CLIENT } Mode;
Mode mode = NO_MODE;

// Access point for web configuration

AccessPoint *pAccessPoint = NULL;
ESP8266WebServer *pServer = NULL;

// create 15 buttons, in classic candybar phone style

char buttonlabels[15][9] = { "", "", "", "", "", "", "", "", "", "", "", "", "", "", "" };
Adafruit_GFX_Button buttons[15];
int nButtons = 0;

// Icons

// 'wifi1', 40x30px
const unsigned char wifi_bitmap_1 [] PROGMEM = {
	0x00, 0x03, 0xff, 0xe0, 0x00, 0x00, 0x1f, 0xff, 0xfc, 0x00, 0x00, 0x7f, 0xff, 0xff, 0x00, 0x01, 
	0xff, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xff, 0xff, 0xe0, 0x0f, 0xff, 0xff, 0xff, 0xf8, 0x1f, 0xfc, 
	0x00, 0x3f, 0xfc, 0x3f, 0xf0, 0x00, 0x07, 0xfe, 0x7f, 0xc0, 0x00, 0x01, 0xff, 0xff, 0x00, 0x1c, 
	0x00, 0xff, 0xfe, 0x01, 0xff, 0xc0, 0x3f, 0xfc, 0x07, 0xff, 0xf0, 0x1f, 0x78, 0x1f, 0xff, 0xfc, 
	0x0e, 0x30, 0x3f, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x80, 
	0x01, 0xff, 0x80, 0xff, 0x80, 0x01, 0xfe, 0x00, 0x3f, 0xc0, 0x01, 0xfc, 0x00, 0x1f, 0x80, 0x00, 
	0xf8, 0x00, 0x0f, 0x80, 0x00, 0x70, 0x08, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 
	0x80, 0x00, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0x7f, 0x00, 
	0x00, 0x00, 0x00, 0x3e, 0x00, 0x00
};

// 'wifi2', 40x40px
const unsigned char wifi_bitmap_2 [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 
	0x00, 0x00, 0x00, 0x01, 0xff, 0xc0, 0x00, 0x00, 0x07, 0xff, 0xf0, 0x00, 0x00, 0x1f, 0xff, 0xfc, 
	0x00, 0x00, 0x3f, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0x80, 
	0x01, 0xff, 0x80, 0xff, 0x80, 0x01, 0xfe, 0x00, 0x3f, 0xc0, 0x01, 0xfc, 0x00, 0x1f, 0x80, 0x00, 
	0xf8, 0x00, 0x0f, 0x80, 0x00, 0x70, 0x08, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 
	0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 
	0x80, 0x00, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0x7f, 0x00, 
	0x00, 0x00, 0x00, 0x3e, 0x00, 0x00
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 176)
const int bitmap_allArray_LEN = 2;
const unsigned char* bitmap_allArray[2] = {
	wifi_bitmap_1, wifi_bitmap_2
};

void getWifiParams(File &fWifi, String &SSID, String &password, String &email) {
  FirebaseJson json;
  json.readFrom(fWifi);
  FirebaseJsonData result;
  json.get(result, "SSID");
  SSID = result.to<String>();
  json.get(result, "password");
  password = result.to<String>();
  json.get(result, "email");
  email = result.to<String>();
}

bool connectToWifi(Adafruit_ILI9341 &tft, String &SSID, String &password) {
  Serial.printf("Connecting to WiFi SSID %s\n", SSID.c_str());
  int frame = 0;
  tft.setRotation(1);
  tft.drawBitmap(140, 105, wifi_bitmap_1, 40, 30, ILI9341_MAGENTA);
  WiFi.begin(SSID, password);
  unsigned long wifiMillis = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiMillis < WIFI_TIMEOUT)
  {
    Serial.print(".");
    delay(300);
    frame = !frame;
    tft.fillRect(140, 105, 40, 30, ILI9341_BLACK);
    tft.drawBitmap(140, 105, frame == 0 ? wifi_bitmap_1 : wifi_bitmap_2, 40, 30, ILI9341_MAGENTA);
  }
  tft.fillRect(140, 105, 40, 30, ILI9341_BLACK);
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

void drawInterface() {

  uint16_t buttoncolors[15] = { ILI9341_DARKGREEN, ILI9341_DARKGREY, ILI9341_RED, 
                                ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, 
                                ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, 
                                ILI9341_BLUE, ILI9341_BLUE, ILI9341_BLUE, 
                                ILI9341_ORANGE, ILI9341_BLUE, ILI9341_ORANGE};

  strcpy(buttonlabels[0], "Send");
  strcpy(buttonlabels[1], "Clr");
  strcpy(buttonlabels[2], "End");
  strcpy(buttonlabels[3], "1");
  strcpy(buttonlabels[4], "2");
  strcpy(buttonlabels[5], "3");
  strcpy(buttonlabels[6], "4");
  strcpy(buttonlabels[7], "5");
  strcpy(buttonlabels[8], "6");
  strcpy(buttonlabels[9], "7");
  strcpy(buttonlabels[10], "8");
  strcpy(buttonlabels[11], "9");
  strcpy(buttonlabels[12], "*");
  strcpy(buttonlabels[13], "0");
  strcpy(buttonlabels[14], "#");

  tft.setRotation(0);

  // Create buttons

  for (uint8_t row=0; row<5; row++) {
    for (uint8_t col=0; col<3; col++) {
      buttons[col + row*3].initButton(&tft, BUTTON_X+col*(BUTTON_W+BUTTON_SPACING_X), 
                BUTTON_Y+row*(BUTTON_H+BUTTON_SPACING_Y),    // x, y, w, h, outline, fill, text
                  BUTTON_W, BUTTON_H, ILI9341_WHITE, buttoncolors[col+row*3], ILI9341_WHITE,
                  buttonlabels[col + row*3], BUTTON_TEXTSIZE); 
      buttons[col + row*3].drawButton();
    }
  }
  nButtons = 15;

  // Create textfield

  tft.drawRect(TEXT_X, TEXT_Y, TEXT_W, TEXT_H, ILI9341_WHITE);

  // Draw initial textfield contents

  tft.setCursor(TEXT_X + 2, TEXT_Y + 10);
  tft.setTextColor(TEXT_TCOLOR, ILI9341_BLACK);
  tft.setTextSize(TEXT_TSIZE);
  strcpy(textfield, "Hello");
  tft.print(textfield);
}

// Set up no wifi screen

void drawNoWifi(Adafruit_ILI9341 &tft) {
  Util::drawLogo(&tft);
  Util::drawCenteredString(&tft, "WiFi timeout", 160, 70);

  strcpy(buttonlabels[0], "Continue");
  buttons[0].initButton(&tft, 80, 130, 130, 60,
                        ILI9341_WHITE, ILI9341_GREEN, ILI9341_WHITE, buttonlabels[0], 2);
  buttons[0].drawButton();
  strcpy(buttonlabels[1], "Reconfig");
  buttons[1].initButton(&tft, 240, 130, 130, 60,
                        ILI9341_WHITE, ILI9341_RED, ILI9341_WHITE, buttonlabels[1], 2);
  buttons[1].drawButton();
  nButtons = 2;
}

void setOTP() {
  char otp[REQUEST_ID_SIZE+1];
  otp[REQUEST_ID_SIZE] = 0;
  for (int i = 0; i < REQUEST_ID_SIZE; i++) {
    otp[i] = 'a' + random(0, 26);
  }
  File fOTP = LittleFS.open(REQUEST_ID_FILE, "w");
  fOTP.printf("%s\n", otp);
  fOTP.close();
}

// Inform user that OTP has been sent

void drawOtpSent(Adafruit_ILI9341 &tft) {
  Util::drawLogo(&tft);
  Util::drawCenteredString(&tft, "Check email for OTP", 160, 70);
  strcpy(buttonlabels[0], "Continue");
  strcpy(buttonlabels[1], "Retry");
  strcpy(buttonlabels[2], "Reconfig");
  buttons[0].initButton(&tft, 80, 130, 130, 60,
                        ILI9341_WHITE, ILI9341_GREEN, ILI9341_WHITE, buttonlabels[0], 2);
  buttons[1].initButton(&tft, 240, 130, 130, 60,
                        ILI9341_WHITE, ILI9341_GREEN, ILI9341_WHITE, buttonlabels[1], 2);
  buttons[2].initButton(&tft, 160, 200, 130, 60,
                        ILI9341_WHITE, ILI9341_RED, ILI9341_WHITE, buttonlabels[2], 2);
  buttons[0].drawButton();
  buttons[1].drawButton();
  buttons[2].drawButton();
  nButtons = 3;
}

// Initialization

void setup() {

  // Wait for... ?

  delay(1000);

  // Start serial monitor

  Serial.begin(115200);
  Serial.println("--- Kettle OS ---");

  // Set up SPI frequency

  SPI.setFrequency(ESP_SPI_FREQ);

  // Mount SPI filesystem

  if (!LittleFS.begin()) {
    Serial.println("ERROR: Unable to mount filesystem");
  }

  // Configure screen

  tft.begin();
  touch.begin(tft.width(), tft.height());  // Must be done before setting rotation
  Serial.print("tftx = ");
  Serial.print(tft.width());
  Serial.print(" tfty = ");
  Serial.println(tft.height());
  tft.fillScreen(ILI9341_BLACK);

  // Replace these for your screen module
  // touch.setCalibration(1832, 262, 264, 1782);
  // 1816
  // 281
  // 262
  // 1768
  touch.setCalibration(1816, 281, 262, 1768);

  // Check for WiFi details file.
  // If not found, start in access point mode
  // Otherwise, try to connect

  File fWifi = LittleFS.open(WIFI_PARAM_FILE, "r");
  if (!fWifi) {
    Serial.println("Warning: no /wifi.txt found");
    AccessPoint::setTft(&tft);
    pServer = AccessPoint::start();
    mode = ACCESS_POINT;
  } else {
    String SSID, password, email;
    getWifiParams(fWifi, SSID, password, email);
    fWifi.close();

    // Try to connect to WiFi.
    // If timeout, ask user whether to continue disconnected or reset WiFi config
    // Otherwise, try to get Firebase parameters

    if (!connectToWifi(tft, SSID, password)) {
      Serial.println("Warning: wifi timeout");
      drawNoWifi(tft);
      mode = NO_WIFI;
    } else {
      Serial.print("Connected with IP: ");
      Serial.println(WiFi.localIP());

      // Check for Firebase credentials file.
      // If found, start Firebase connection.
      // Otherwise, continue disconnected

      File fFirebase = LittleFS.open(FIREBASE_PARAM_FILE, "r");
      if (!fFirebase) {
        Serial.println("Warning: no firebase config found");
        File fOTP = LittleFS.open(REQUEST_ID_FILE, "r");
        if (!fOTP) {
          setOTP();
          drawOtpSent(tft);
          mode = OTP_SENT;
        } else {
          drawInterface();
        }
      } else {
        drawInterface();
      }
    }
  }
}

void loop() {
  
  // If running web server, poll for client connections

  if (mode == ACCESS_POINT && pServer) {
    pServer->handleClient();
  }

  // Check touch screen

  uint16_t x, y;
  if (touch.isTouching()) {
    touch.getPosition(x, y);
    if (mode == NO_WIFI || mode == OTP_SENT) {
      // Transform from rotation 0 to rotation 1
      uint16_t tmp = x;
      x = y;
      y = tft.height() - tmp;
    }
  }

  // Check if a button is pressed

  for (uint8_t b=0; b<nButtons; b++) {
    if (buttons[b].contains(x, y)) {
      buttons[b].press(true);  // tell the button it is pressed
    } else {
      buttons[b].press(false);  // tell the button it is NOT pressed
    }
  }

  // Process button state changes

  for (uint8_t b=0; b<15; b++) {
    if (buttons[b].justReleased()) {
      buttons[b].drawButton();  // draw normal
    }    
    if (buttons[b].justPressed()) {
      // Draw inverted
      buttons[b].drawButton(true);
      if (mode == NO_WIFI) {
        if (b == 0) {
          // Continue
          tft.fillScreen(ILI9341_BLACK);
          drawInterface();
          mode = DISCONNECTED;
        } else {
          // Reset configuration
          LittleFS.remove(WIFI_PARAM_FILE);
          LittleFS.remove(REQUEST_ID_FILE);
          ESP.reset();
        }
      } else if (mode == OTP_SENT) {
        if (b == 0) {
          // Continue
          tft.fillScreen(ILI9341_BLACK);
          drawInterface();
          mode = DISCONNECTED;
        } else if (b == 1) {
          // Retry
          ESP.reset();
        } else {
          // Reset configuration
          LittleFS.remove(WIFI_PARAM_FILE);
          LittleFS.remove(REQUEST_ID_FILE);
          ESP.reset();
        }
      } else {
        // if a numberpad button, append the relevant # to the textfield
        if (b >= 3) {
          if (textfield_i < TEXT_LEN) {
            textfield[textfield_i] = buttonlabels[b][0];
            textfield_i++;
              textfield[textfield_i] = 0; // zero terminate
          }
        }

        // Clr button: delete char
        if (b == 1) {        
          textfield[textfield_i] = 0;
          if (textfield_i > 0) {
            textfield_i--;
            textfield[textfield_i] = ' ';
          }
        }

        // update the current text field
        Serial.println(textfield);
        tft.setCursor(TEXT_X + 2, TEXT_Y+10);
        tft.setTextColor(TEXT_TCOLOR, ILI9341_BLACK);
        tft.setTextSize(TEXT_TSIZE);
        tft.print(textfield);
      }        
      delay(100); // UI debouncing
    }
  }
}

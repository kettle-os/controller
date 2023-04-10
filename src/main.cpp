#include <Arduino.h>
#include <SPI.h>

#include <LittleFS.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#else
#include <Wifi.h>
#endif
#include <WiFiClientSecureBearSSL.h>
#include <CertStoreBearSSL.h>

#include <Firebase.h>
#include <json/FirebaseJson.h>
#include <addons/TokenHelper.h>

#include <Adafruit_GFX.h>
#include <Adafruit_MAX31865.h>
#include <XPT2046.h>
#include <PID_v1.h>

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

// Firebase parameters. Can be overridden by contents of
// /service-account.json . If no service account file is
// found and no PROJECT_LOCATION/FIREBASE_PROJECT_ID is
// set, we connect to the default DIY-BREW for device
// registration.

#define FIREBASE_PARAM_FILE "/service-account.json"
#define DEVICE_REG_TOKEN_FILE "/reg-token.json"
#define FIREBASE_CONFIG_FILE "/firebase-config.json"
#define ID_TOKEN_FILE "/id-token.json"

// TFT and touch screen objects

#define DISPLAY_CYCLE_TIME 1000
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
XPT2046 touch(TOUCH_CS, TOUCH_IRQ);
unsigned long displayMillis = 0;

// Operation mode

typedef enum { NO_MODE, ACCESS_POINT, NO_WIFI, NO_CERTS, NO_FB_CONFIG,
               REGISTRATION_EXPIRED,
               REGISTRATION_ERROR, REGISTRATION_SENT, DISCONNECTED,
               AUTH_EXPIRED, AUTHENTICATED_CLIENT } Mode;
Mode mode = NO_MODE;

// Access point for web configuration

AccessPoint *pAccessPoint = NULL;
ESP8266WebServer *pServer = NULL;

// SSL certifiate store object

BearSSL::CertStore certStore;

// Firebase objects

#define DB_UPDATE_CYCLE_TIME 2000
FirebaseData fbdoWrite;
FirebaseData fbdoRead;
FirebaseAuth auth;
FirebaseConfig config;
String boardID;
unsigned long fbLoopMillis = 0;

// RTD probe parameters and module setup

#define RREF      430.0
#define RNOMINAL  100.0
int rtdPin = 16; // Pin marked D0/GPIO16 on the board, used for SPI CS
double rtdTemp = 0;
Adafruit_MAX31865 thermo = Adafruit_MAX31865(rtdPin); // Use D0 for SPI CS, HW SPI for MISO/MOSI/CLK

// Pins for potentiometer, SSR, input variable for potentiometer, LED output

int potPin = A0;
int sensorValue = 0;
int ledOutput = LOW;
#define SSR_CYCLE_TIME 5000
int ssrPin = 5;  // Pin marked D1/SCL/GPIO5 on the board
unsigned long dataMillis = 0;
unsigned long ssrMillis = 0;
unsigned long timeOnMs = 0;

// PID variables

typedef enum { CONTROL_OFF=0, CONTROL_MANUAL=1, CONTROL_PID=2 } ControlState;

double setPoint = 0;
ControlState controlState = CONTROL_OFF;
double pidOut = 0;
// Assuming deflection D = 2500 ms, amplitude A = 2 deg, period Pu = 120 s
// Ku = 4 * D / A * pi, Kp = 0.6 * Ku, Ki = 1.2 * Ku / Pu, Kd = 0.075 * Ku * Pu
PID tempPID(&rtdTemp, &pidOut, &setPoint, 9424.8, 157.08, 141372, DIRECT);

// 15 buttons, in classic candybar phone style

#define MAX_BUTTONS 3
char buttonlabels[MAX_BUTTONS][9] = { "", "", "" };
Adafruit_GFX_Button buttons[MAX_BUTTONS];
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

String startWifi() {
  File fWifi = LittleFS.open(WIFI_PARAM_FILE, "r");
  if (!fWifi) {
    Serial.println("Warning: no /wifi.txt found");
    AccessPoint::setTft(&tft);
    pServer = AccessPoint::start();
    mode = ACCESS_POINT;
    return "";
  }

  // Get WiFi parameters from file

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
  }
  return email;
}

// Set time via NTP, as required for x.509 validation

void setClock() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

// Screen to display when disconnected (will be update to show manual heat control screen)

void drawDisconnected() {
  tft.setTextSize(3);
  tft.setCursor(10, 100);
  tft.print("DISCONNECTED");
}

// Get a device registration token for given MAC address and email address

bool getDeviceRegistrationToken(String getTokenUrl, String mac, String email) {
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setCertStore(&certStore);
  HTTPClient https;
  String url = getTokenUrl + "?mac=" + mac + "&email=" + email;
  if (https.begin(*client, url)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = https.getString();
        Serial.println(payload);
        File fToken = LittleFS.open(DEVICE_REG_TOKEN_FILE, "w");
        fToken.printf(payload.c_str());
        fToken.close();
        return true;
      } else {
        Serial.printf("Error: unexpected http return code: %d\n", httpCode);
      }
    } else {
      Serial.printf("Error fetching token: %s\n", https.errorToString(httpCode).c_str());
    }
  }
  return false;
}

// Inform user that some configuration/registration action is required

void drawNeedSetupScreen(Adafruit_ILI9341 &tft, String message, String label0="Continue",
                         String label1="Retry", String label2="Reconfig") {
  Util::drawLogo(&tft);
  Util::drawCenteredString(&tft, message, 160, 70);
  strcpy(buttonlabels[0], label0.c_str());
  strcpy(buttonlabels[1], label1.c_str());
  strcpy(buttonlabels[2], label2.c_str());
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

String getOrFetchToken(String &email) {
  FirebaseJsonData result;
  File fToken = LittleFS.open(DEVICE_REG_TOKEN_FILE, "r");
  if (!fToken) {
    File fFirebase = LittleFS.open(FIREBASE_CONFIG_FILE, "r");
    if (!fFirebase) {
      Serial.println("Warning: no firebase initial config found");
      drawNeedSetupScreen(tft, "No firebase config found");
      mode = NO_FB_CONFIG;
      return "";
    }
    FirebaseJson jsonFBConfig;
    jsonFBConfig.readFrom(fFirebase);
    jsonFBConfig.get(result, "getTokenUrl");
    String getTokenUrl = result.to<String>();
    fFirebase.close();
    // Attempt to get registration token
    Serial.println("Getting device registration token...");
    bool success = getDeviceRegistrationToken(getTokenUrl, WiFi.macAddress(), email);
    if (success) {
      drawNeedSetupScreen(tft, "Check email for reg link");
      mode = REGISTRATION_SENT;
    } else {
      drawNeedSetupScreen(tft, "Registration error");
      mode = REGISTRATION_ERROR;
    }
    return "";
  }
  // Look for device registration
  FirebaseJson jsonToken;
  jsonToken.readFrom(fToken);
  jsonToken.get(result, "token");
  String token = result.to<String>();
  fToken.close();
  return token;
}

// Request ID token in the case that user has confirmed the registration

bool getCredentials(String &getCredentialsUrl, String &token) {
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setCertStore(&certStore);
  HTTPClient https;
  String url = getCredentialsUrl + "?token=" + token;
  Serial.printf("Fetching credentials from %s\n", getCredentialsUrl.c_str());
  if (https.begin(*client, url)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = https.getString();
        Serial.println(payload);
        File fToken = LittleFS.open(ID_TOKEN_FILE, "w");
        fToken.printf("%s\n", payload.c_str());
        fToken.close();
        return true;
      } else if (httpCode == 410) {
        Serial.printf("Registration token expired\n");
        mode = REGISTRATION_EXPIRED;
      } else {
        Serial.printf("Error: unexpected http return code: %d\n", httpCode);
      }
    } else {
      Serial.printf("Error fetching credentials: %s\n", https.errorToString(httpCode).c_str());
    }
  }
  return false;
}

String getOrFetchCredentials(String &email) {
  FirebaseJsonData result;
  File fIdToken = LittleFS.open(ID_TOKEN_FILE, "r");
  if (!fIdToken) {
    Serial.println("Warning: no firebase ID token found");
    // Without firebase to do the NTP sync and SSL setup, we do it ourselves
    setClock();
    int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    Serial.printf("Number of CA certs read: %d\n", numCerts);
    if (numCerts == 0) {
      Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
      drawNeedSetupScreen(tft, "No SSL certificates found");
      mode = NO_CERTS;
      return "";
    }
    String token = getOrFetchToken(email);
    if (mode == NO_FB_CONFIG || mode == REGISTRATION_SENT || mode == REGISTRATION_ERROR) return "";

    File fFirebase = LittleFS.open(FIREBASE_CONFIG_FILE, "r");
    if (!fFirebase) {
      Serial.println("Warning: no firebase initial config found");
      drawNeedSetupScreen(tft, "No firebase config found");
      mode = NO_FB_CONFIG;
      return "";
    }
    FirebaseJson jsonFBConfig;
    jsonFBConfig.readFrom(fFirebase);
    jsonFBConfig.get(result, "getCredentialsUrl");
    String getCredentialsUrl = result.to<String>();
    fFirebase.close();
    bool success = getCredentials(getCredentialsUrl, token);
    if (success) {
      fIdToken = LittleFS.open(ID_TOKEN_FILE, "r");
    } else if (mode == REGISTRATION_EXPIRED) {
      drawNeedSetupScreen(tft, "Registration expired");
      return "";
    } else {
      drawNeedSetupScreen(tft, "Registration unconfirmed");
      mode = REGISTRATION_ERROR;
      return "";
    }
  }
  FirebaseJson jsonIdToken;
  jsonIdToken.readFrom(fIdToken);
  jsonIdToken.get(result, "idToken");
  String idToken = result.to<String>();
  fIdToken.close();
  return idToken;
}

// Firebase stream read callback

void streamCallback(MultiPathStream data) {
  Serial.println("Got stream data");
  if (data.get("/setPoint")) {
    Serial.print("Setpoint stream event type: "); Serial.println(data.eventType.c_str());
    Serial.printf("Value %s (%f)\n", data.value.c_str(), data.value.toFloat());
    setPoint = data.value.toFloat();
  }
  if (data.get("/controlState")) {
    Serial.print("Control state stream event type: "); Serial.println(data.eventType.c_str());
    Serial.printf("Value %s (%ld)\n", data.value.c_str(), data.value.toInt());
    controlState = (ControlState)data.value.toInt();
  }
  Serial.println(data.value.c_str());
  Serial.printf("After stream data update: setpoint %f control state %d\n", setPoint, controlState);
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("Stream timeout, resume streaming...");
  }
}

void startFirebase(String &idToken) {
  File fFirebase = LittleFS.open(FIREBASE_CONFIG_FILE, "r");
  if (!fFirebase) {
    Serial.println("Warning: no firebase initial config found");
    drawNeedSetupScreen(tft, "No firebase config found");
    mode = NO_FB_CONFIG;
    return;
  }
  Serial.printf("Firebase client v%s\n\n", FIREBASE_CLIENT_VERSION);
  FirebaseJson jsonFBConfig;
  FirebaseJsonData result;
  jsonFBConfig.readFrom(fFirebase);
  jsonFBConfig.get(result, "apiKey");
  String apiKey = result.to<String>();
  jsonFBConfig.get(result, "dbUrl");
  String dbUrl = result.to<String>();
  fFirebase.close();
  config.api_key = apiKey;
  config.database_url = dbUrl;
  Firebase.reconnectWiFi(true);
  Firebase.setIdToken(&config, idToken.c_str(), 3600);
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.begin(&config, &auth);
  Firebase.RTDB.setMultiPathStreamCallback(&fbdoRead, streamCallback, streamTimeoutCallback);
    String fbReadPath = boardID + "/inputs";
  if (!Firebase.RTDB.beginMultiPathStream(&fbdoRead, fbReadPath)) {
    Serial.print("Firebase read stream error: ");
    Serial.println(fbdoRead.errorReason());
  }
  fbdoWrite.setBSSLBufferSize(512, 2048);
}

#define FIREBASE_BEGIN_WAIT_MILLIS 5000

void waitForFirebase(Adafruit_ILI9341 &tft) {
  Serial.println("Waiting for Firebase...");
  fbLoopMillis = millis();
  while(millis() < fbLoopMillis + FIREBASE_BEGIN_WAIT_MILLIS) {
    if (Firebase.ready()) {
      Serial.println("Ready!");
      break;
    }
  }
  if (!Firebase.ready()) {
    Serial.println("Can't connect to Firebase");
    drawNeedSetupScreen(tft, "Can't connect to Firebase");
    mode = DISCONNECTED;
  }
}

void verifyAuthentication(Adafruit_ILI9341 &tft) {
  bool ok = Firebase.RTDB.setInt(&fbdoWrite, "/" + boardID + "/sensors/pot", 0);
  if (!ok) {
    Serial.printf("Problem writing to RTDB: %s\n", fbdoWrite.errorReason().c_str());
    Serial.printf("HTTP code %d\n", fbdoWrite.httpCode());
    if (fbdoWrite.httpCode() == 401) {
      drawNeedSetupScreen(tft, "Authentication expired", "Continue", "Retry", "Reauthenticate");
      mode = AUTH_EXPIRED;
    } else {
      drawNeedSetupScreen(tft, "Can't connect to Firebase");
      mode = DISCONNECTED;
    }
  }
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

  // Initialize digital pins as outputs

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ssrPin, OUTPUT);
  pinMode(rtdPin, OUTPUT);

  // Set unique board ID to wifi MAC address

  boardID = WiFi.macAddress();

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

  // Connect to RTD probe and configure PID

  thermo.begin(MAX31865_3WIRE);
  tempPID.SetOutputLimits(0, SSR_CYCLE_TIME);
  tempPID.SetMode(AUTOMATIC);

  // Check for WiFi details file.
  // If not found, start in access point mode
  // Otherwise, try to connect

  String email = startWifi();

  if (mode == NO_WIFI || mode == ACCESS_POINT) return;

  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Check for token file. If not found, start registration process

  String idToken = getOrFetchCredentials(email);
  if (mode == NO_FB_CONFIG || mode == REGISTRATION_ERROR || mode == REGISTRATION_EXPIRED ||
      mode == REGISTRATION_SENT || mode == NO_CERTS) return;
  Serial.printf("Got ID token %s\n", idToken.c_str());
  startFirebase(idToken);
  if (mode == NO_FB_CONFIG) return;
  mode = AUTHENTICATED_CLIENT;

  // Check firebase connectivity
  
  waitForFirebase(tft);
  if (mode == DISCONNECTED) return;

  // Verify that we are authenticated

  verifyAuthentication(tft);
  if (mode == AUTH_EXPIRED || mode == DISCONNECTED) return;

  // Prepare for first cycle for SSR loop and DB loop

  ssrMillis = millis() - SSR_CYCLE_TIME;
  dataMillis = millis() - DB_UPDATE_CYCLE_TIME;
  tft.fillScreen(ILI9341_BLACK);
}

void showStats(float pot, float temp, uint8_t fault) {
  float level = (pot - 10) / 100;
  if (level < 0) level = 0;
  if (level > 10) level = 10;

  char buf[80];
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setTextSize(2);
  if (controlState == CONTROL_MANUAL || controlState == CONTROL_OFF) {
    sprintf(buf, " Manual Heat Control ");
  } else {
    sprintf(buf, "  Auto Heat Control  ");
  }
  Util::drawCenteredString(&tft, buf, 160, 20);

  tft.setTextSize(4);
  if (controlState == CONTROL_MANUAL || controlState == CONTROL_OFF) {
    sprintf(buf, " %2.2f ", level);
  } else {
    sprintf(buf, " %2.2f ", pidOut / SSR_CYCLE_TIME * 10);
  }
  Util::drawCenteredString(&tft, buf, 160, 60);
 
  tft.setTextSize(2);
  sprintf(buf, "Current Temperature");
  Util::drawCenteredString(&tft, buf, 160, 120);

  tft.setTextSize(4);
  if (fault) {
    sprintf(buf, " FAULT ");
  } else {
    sprintf(buf, "  %3.1f  ", temp);
  }
  Util::drawCenteredString(&tft, buf, 160, 160);

  return;
}

void loop() {
  
  // If running web server, poll for client connections

  if (mode == ACCESS_POINT && pServer) {
    pServer->handleClient();
    return;
  }

  // Read inputs

  sensorValue = analogRead(potPin);
  rtdTemp = thermo.temperature(RNOMINAL, RREF);
  uint8_t fault = thermo.readFault();
  tempPID.Compute();

  // If SSR cycle time has been exceeded, start a new cycle

  unsigned long now = millis();
  if (now >= ssrMillis + SSR_CYCLE_TIME) {
    // Start next loop
    switch (controlState) {
      case CONTROL_OFF:
        timeOnMs = 0;
        break;
      case CONTROL_MANUAL:
        timeOnMs = SSR_CYCLE_TIME * sensorValue / 1024;
        break;
      case CONTROL_PID:
        timeOnMs = pidOut;
        break;
    }
    if (timeOnMs < 100) {
      timeOnMs = 0;
    }
    if (timeOnMs > SSR_CYCLE_TIME) {
      timeOnMs = SSR_CYCLE_TIME;
    }
    ssrMillis += SSR_CYCLE_TIME;
  }

  // Turn on SSR if it's under control and we're in the ON cycle

  if (controlState != CONTROL_OFF && timeOnMs > now - ssrMillis) {
    digitalWrite(ssrPin, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(ssrPin, LOW);
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // Update display if it's time

  if (millis() > displayMillis + DISPLAY_CYCLE_TIME) {
    displayMillis += DISPLAY_CYCLE_TIME;
    if (mode == AUTHENTICATED_CLIENT) {
      showStats(sensorValue, rtdTemp, fault);
    }
  }

  // Write state to Firebase if it's time

  if (mode == AUTHENTICATED_CLIENT && Firebase.ready() &&
      millis() > dataMillis + DB_UPDATE_CYCLE_TIME)
  {
    dataMillis += DB_UPDATE_CYCLE_TIME;
    Serial.printf("Setting pot sensor val %d\n", sensorValue);
    bool ok = Firebase.RTDB.setIntAsync(&fbdoWrite, "/" + boardID + "/sensors/pot", sensorValue);
    if (!ok) {
      Serial.printf("Problem writing pot sensor val: %s\n", fbdoWrite.errorReason().c_str());
    }
    if (fault) {
      Serial.printf("RTD probe fault 0x%x -- check connection -- ", fault);
      thermo.clearFault();
    }
    Serial.printf("Setting temperature sensor val %f\n", rtdTemp);
    ok = Firebase.RTDB.setFloatAsync(&fbdoWrite, "/" + boardID + "/sensors/temp", (float)rtdTemp);
    if (!ok) {
      Serial.printf("Problem writing temp sensor val: %s\n", fbdoWrite.errorReason().c_str());
    }
    float power = (float) timeOnMs / SSR_CYCLE_TIME;
    Serial.printf("Setting output power %f\n", power);
    ok = Firebase.RTDB.setFloatAsync(&fbdoWrite, "/" + boardID + "/output", power);
    if (!ok) {
      Serial.printf("Problem writing power: %s\n", fbdoWrite.errorReason().c_str());
    }
    Serial.printf("Temp PID temp %f out %f set %f\n", rtdTemp, pidOut, setPoint);
  }

  // Check touch screen

  uint16_t x, y;
  if (touch.isTouching()) {
    touch.getPosition(x, y);
    if (mode == NO_WIFI || mode == REGISTRATION_SENT ||
        mode == NO_CERTS || mode == NO_FB_CONFIG || mode == REGISTRATION_ERROR ||
        mode == REGISTRATION_EXPIRED || mode == DISCONNECTED || mode == AUTH_EXPIRED) {
      // Transform from rotation 0 to rotation 1
      uint16_t tmp = x;
      x = y;
      y = tft.height() - tmp;
    }
    Serial.printf("Touch at %d, %d\n", x, y);
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

  for (uint8_t b=0; b<MAX_BUTTONS; b++) {
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
          drawDisconnected();
          mode = DISCONNECTED;
        } else {
          // Reset configuration
          LittleFS.remove(WIFI_PARAM_FILE);
          ESP.reset();
        }
      } else if (mode == REGISTRATION_SENT || mode == REGISTRATION_ERROR ||
                 mode == REGISTRATION_EXPIRED || mode == AUTH_EXPIRED) {
        if (b == 0) {
          // Continue
          tft.fillScreen(ILI9341_BLACK);
          drawDisconnected();
          mode = DISCONNECTED;
        } else if (b == 1) {
          // Retry
          ESP.reset();
        } else {
          // Reset configuration
          LittleFS.remove(DEVICE_REG_TOKEN_FILE);
          LittleFS.remove(ID_TOKEN_FILE);
          ESP.reset();
        }
      }        
      delay(100); // UI debouncing
    }
  }
}

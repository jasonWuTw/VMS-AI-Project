#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h> // ArduinoJson 函式庫

extern const char HTML_PAGE[];  // 設定HTML Page
extern String htmlHeader;
extern const char *ap_ssid; // 設定您的 AP 名稱
extern const char *ap_password;      // 設定您的 AP 密碼 (至少8個字元，或設為 NULL 代表開放網路)
extern char ssid[];    // Set the AP SSID
extern char pass[];    // Set the AP password
extern String _post_ssid;
extern String _post_pass;

extern char apSSID[];    // Set the AP SSID
extern char apPASSWORD[];        // Set the AP password
extern char channel[];               // Set the AP channel
extern int ssid_status;                // Set SSID status, 1 hidden, 0 not hidden

extern WiFiServer server;
extern int status;
extern bool isWebServerEnabled;

#define SWITCH_BUTTON   AMB_D8           
#define VOL_UP_BUTTON   AMB_D7           
#define VOL_DOWN_BUTTON AMB_D22          
#define WIFIAP_LED      LED_B
#define WIFICLIENT_LED  LED_G

void handleWIFIAPWebProcess(WiFiClient client);
void handleWifiPageRoot(WiFiClient client);
void handleScanWiFi(WiFiClient client);
void handleConnectWiFi(WiFiClient client, String postData);
void handleNotFound(WiFiClient client);
void printWifiStatus();
void parseFormData(String formData);
void wifiAPProcess();
void printMacAddress();





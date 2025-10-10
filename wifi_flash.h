#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h> // ArduinoJson 函式庫
#include "wifi_html.h"
#include "FlashMemory.h"
#include "AmebaFatFS.h"

// --- 配置 ---
// !! 重要 !!
// 這個地址必須是 Flash Sector (FlashMemory.h 中 FLASH_SECTOR_SIZE 通常是 0x1000 = 4KB) 對齊的。
// 並且必須位於用戶可用的 Flash 區域，不能與韌體衝突。
// 範例地址 0x180000，請根據你的 Ameba Pro 2 實際情況和 FlashMemory.h 的適用範圍確認。
// FlashMemory.h 中有 FLASH_MEMORY_APP_BASE (0xFD000)，如果你的區域不同，請確保你選擇的地址有效。
// #define WIFI_CONFIG_FLASH_ADDR  FLASH_MEMORY_APP_BASE           // 範例地址，請確認其適用性！
#define WIFI_CONFIG_FLASH_ADDR 0xFD0000  // FLASH_MEMORY_APP_BASE
// FLASH_SECTOR_SIZE 在 FlashMemory.h 中定義為 0x1000 (4KB)

#define MAGIC_NUMBER            0xABADCAFE // 用於驗證資料是否有效

// #define JSON_SYSTEM_PARAMS_CAPACITY 4096
// extern StaticJsonDocument<JSON_STORAGE_PARAMS_CAPACITY> jsonSystemParamsDoc;

// 定義儲存 Wi-Fi 憑證的結構
struct WifiCredentials {
  char ssid[33];      // 最大 32 字元 + '\0'
  char password[65];  // 最大 64 字元 + '\0'
  // char prompts[3073]; // 最大 2072字元 + '\0'
  // char LCDType[21];    // 最大 20字元 + '\0'
  uint32_t magic;     // Magic number 用於驗證
};

// FlashMemory 物件在 FlashMemory.h 中已透過 extern 宣告，我們直接使用全域的 FlashMemory

// 函數原型
bool saveWifiCredentials(const char* ssid, const char* password);
bool readWifiCredentials(char* ssid_buf, size_t ssid_buf_len, char* password_buf, size_t password_buf_len);
bool clearWifiCredentials();
void printStoredCredentials();
// bool loadSystemParamsFromFS();
// bool saveSystemParamsFromFS();

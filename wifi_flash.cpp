#include <Arduino.h>
#include "FlashMemory.h"
#include "wifi_flash.h"

/**
 * @brief 儲存 Wi-Fi 憑證到 Flash
 * @param ssid 要儲存的 SSID
 * @param password 要儲存的密碼
 * @return true 如果操作已執行 (API 為 void, 假設成功), false 如果前置檢查失敗
 */
bool saveWifiCredentials(const char* ssid, const char* password) {

   FlashMemory.begin(WIFI_CONFIG_FLASH_ADDR, sizeof(WifiCredentials));

  if (sizeof(WifiCredentials) > FlashMemory.buf_size) {
    Serial.println("Error: WifiCredentials struct is larger than FlashMemory internal buffer!");
    return false;
  }

  WifiCredentials creds;
  memset(&creds, 0, sizeof(WifiCredentials)); // 清空結構

  strncpy(creds.ssid, ssid, sizeof(creds.ssid) - 1);
  creds.ssid[sizeof(creds.ssid) - 1] = '\0'; // 確保 null 結尾

  strncpy(creds.password, password, sizeof(creds.password) - 1);
  creds.password[sizeof(creds.password) - 1] = '\0'; // 確保 null 結尾

  creds.magic = MAGIC_NUMBER;

  Serial.println("Erasing flash sector (offset 0 from base address)...");
  // 抹除相對於基底地址 (WIFI_CONFIG_FLASH_ADDR) 的第一個磁區
  FlashMemory.eraseSector(0); // sector_offset = 0
  Serial.println("Flash sector erased.");

  Serial.println("Copying data to FlashMemory internal buffer...");
  memcpy(FlashMemory.buf, &creds, sizeof(WifiCredentials));

  Serial.println("Writing internal buffer to flash (offset 0 from base address)...");
  // 從基底地址 (WIFI_CONFIG_FLASH_ADDR) 的偏移 0 開始寫入 FlashMemory.buf_size 字節
  FlashMemory.write(0); // offset = 0
  Serial.println("Data written to flash.");

  FlashMemory.end();

  return true;
}

/**
 * @brief 從 Flash 讀取 Wi-Fi 憑證
 * @param ssid_buf 用於儲存讀取到的 SSID 的緩衝區
 * @param ssid_buf_len SSID 緩衝區大小
 * @param password_buf 用於儲存讀取到的密碼的緩衝區
 * @param password_buf_len 密碼緩衝區大小
 * @return true 如果成功讀取且資料有效, false 如果失敗或資料無效
 */
bool readWifiCredentials(char* ssid_buf, size_t ssid_buf_len, char* password_buf, size_t password_buf_len) {

  FlashMemory.begin(WIFI_CONFIG_FLASH_ADDR, sizeof(WifiCredentials));

  if (sizeof(WifiCredentials) > FlashMemory.buf_size) {
    Serial.println("Error: WifiCredentials struct is larger than FlashMemory internal buffer, cannot read correctly!");
    return false;
  }

  Serial.println("Reading from flash (offset 0) into FlashMemory internal buffer...");
  // 從基底地址 (WIFI_CONFIG_FLASH_ADDR) 的偏移 0 開始讀取 FlashMemory.buf_size 字節到內部緩衝區
  FlashMemory.read(0); // offset = 0
  Serial.println("Data read into internal buffer.");

  WifiCredentials creds;
  // 從內部緩衝區複製資料到我們的結構
  memcpy(&creds, FlashMemory.buf, sizeof(WifiCredentials));

  FlashMemory.end();

  if (creds.magic == MAGIC_NUMBER) {
    strncpy(ssid_buf, creds.ssid, ssid_buf_len - 1);
    ssid_buf[ssid_buf_len - 1] = '\0'; // 確保 null 結尾

    strncpy(password_buf, creds.password, password_buf_len - 1);
    password_buf[password_buf_len - 1] = '\0'; // 確保 null 結尾
    return true;
  } else {
    Serial.print("No valid credentials found (magic number mismatch). Read magic: 0x");
    Serial.println(creds.magic, HEX);
    // 可以選擇清空輸出緩衝區
    ssid_buf[0] = '\0';
    password_buf[0] = '\0';
    return false;
  }
}

/**
 * @brief 清除儲存在 Flash 中的 Wi-Fi 憑證 (透過抹除對應的 Sector)
 * @return true 如果操作已執行 (API 為 void, 假設成功)
 */
bool clearWifiCredentials() {

  FlashMemory.begin(WIFI_CONFIG_FLASH_ADDR, sizeof(WifiCredentials));
  Serial.println("Erasing flash sector (offset 0 from base address) to clear credentials...");
  // 抹除相對於基底地址 (WIFI_CONFIG_FLASH_ADDR) 的第一個磁區
  FlashMemory.eraseSector(0); // sector_offset = 0
  FlashMemory.end();
  Serial.println("Flash sector erased.");
  return true;
}

/**
 * @brief 讀取並列印儲存的憑證到 Serial
 */
void printStoredCredentials() {
  char current_ssid[33];
  char current_password[65];

  Serial.println("Attempting to read credentials from flash...");
  if (readWifiCredentials(current_ssid, sizeof(current_ssid), current_password, sizeof(current_password))) {
    Serial.println("Stored Credentials Found:");
    Serial.println("  SSID: " + String(current_ssid));
    Serial.println("  Password: " + String(current_password));
  } else {
    Serial.println("No valid credentials found or error reading.");
  }
}

// Make sure FlashMemory.end() is called if needed, though for continuous operation it might not be.
// The destructor ~FlashMemoryClass() might free the buffer if FlashMemory goes out of scope,
// but it's a global object, so it persists.
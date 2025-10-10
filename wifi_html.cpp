#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h> // ArduinoJson 函式庫
#include "wifi_html.h"
#include "wifi_flash.h"
#include <U8g2lib.h>
#include "v7_tft_lcd.h"

String htmlHeader = "";

char ssid[64]; // 假設 SSID 最長 63 字元 + null
char pass[64]; // 假設 SSID 最長 63 字元 + null

// 用於演示的全局變數
String _post_ssid = "";
String _post_password = "";

bool isSuccessWIFIConnect = false;

bool isWebServerEnabled = false;

// --- HTML 內容 ---
// 使用 C++11 的 Raw String Literals (R"rawliteral(...)rawliteral") 來嵌入 HTML，避免轉義問題
// 將您之前設計的完整 HTML 內容複製貼上到 R"rawliteral( 和 )rawliteral" 之間
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Wi-Fi 連線設定 (Ameba Pro 2)</title>
    <style>
        /* 基本重置與通用樣式 */
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji", "Segoe UI Symbol";
            margin: 0;
            padding: 20px;
            background-color: #f4f7f6;
            color: #333;
            line-height: 1.6;
        }
        .container {
            max-width: 600px;
            margin: 20px auto;
            padding: 25px;
            background-color: #ffffff;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
        }
        h1 {
            color: #2c3e50;
            text-align: center;
            margin-bottom: 25px;
        }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 8px; font-weight: bold; color: #34495e; }
        input[type="text"], input[type="password"] {
            width: 100%; padding: 12px; border: 1px solid #ccc; border-radius: 4px;
            box-sizing: border-box; font-size: 16px;
        }
        input[type="text"]:focus, input[type="password"]:focus {
            border-color: #3498db; outline: none; box-shadow: 0 0 0 0.2rem rgba(52,152,219,.25);
        }
        button {
            padding: 12px 20px; background-color: #3498db; color: white; border: none;
            border-radius: 4px; cursor: pointer; font-size: 16px;
            transition: background-color 0.3s ease; width: 100%; margin-top: 10px;
        }
        button:hover { background-color: #2980b9; }
        button:disabled { background-color: #bdc3c7; cursor: not-allowed; }
        #scanButton { background-color: #2ecc71; }
        #scanButton:hover { background-color: #27ae60; }
        #apListContainer {
            margin-top: 20px; border: 1px solid #eee; border-radius: 4px;
            padding: 10px; background-color: #fdfdfd; max-height: 200px; overflow-y: auto;
        }
        #apListContainer h3 { margin-top: 0; color: #34495e; }
        #apList { list-style: none; padding: 0; margin: 0; }
        #apList li {
            padding: 10px; border-bottom: 1px solid #f0f0f0; cursor: pointer;
            transition: background-color 0.2s ease;
        }
        #apList li:last-child { border-bottom: none; }
        #apList li:hover { background-color: #e8f4fd; }
        .ap-ssid { font-weight: bold; }
        .ap-details { font-size: 0.9em; color: #7f8c8d; }
        #statusMessage, #scanStatus {
            margin-top: 15px; padding: 10px; border-radius: 4px;
            text-align: center; font-weight: bold;
        }
        .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
        .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
        .info { background-color: #e2e3e5; color: #383d41; border: 1px solid #d6d8db; }
        @media (max-width: 768px) {
            body { padding: 10px; }
            .container { margin: 10px auto; padding: 15px; }
            h1 { font-size: 1.8em; }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Wi-Fi 設定</h1>
        <div class="form-group">
            <button id="scanButton" type="button">掃描附近的 Wi-Fi</button>
            <div id="scanStatus"></div>
        </div>
        <div id="apListContainer" style="display: none;">
            <h3>可用的 Wi-Fi 網路:</h3>
            <ul id="apList"></ul>
        </div>
        <form id="wifiForm">
            <div class="form-group">
                <label for="ssid">Wi-Fi 名稱 (SSID):</label>
                <input type="text" id="ssid" name="ssid" required>
            </div>
            <div class="form-group">
                <label for="password">Wi-Fi 密碼:</label>
                <input type="password" id="password" name="password">
                <small>如果 Wi-Fi 沒有密碼，請留空。</small>
            </div>
            <button type="submit" id="connectButton">連線</button>
        </form>
        <div id="statusMessage"></div>
    </div>
    <script>
        const scanButton = document.getElementById('scanButton');
        const apListContainer = document.getElementById('apListContainer');
        const apListUl = document.getElementById('apList');
        const scanStatusDiv = document.getElementById('scanStatus');
        const wifiForm = document.getElementById('wifiForm');
        const ssidInput = document.getElementById('ssid');
        const passwordInput = document.getElementById('password');
        const connectButton = document.getElementById('connectButton');
        const statusMessageDiv = document.getElementById('statusMessage');

        scanButton.addEventListener('click', function() {
            scanStatusDiv.innerHTML = '<span class="info">掃描中...</span>';
            scanStatusDiv.className = 'info';
            scanButton.disabled = true;
            apListUl.innerHTML = '';
            apListContainer.style.display = 'none';
            const xhr = new XMLHttpRequest();
            xhr.open('GET', '/scan_wifi_aps', true);
            xhr.onreadystatechange = function() {
                if (xhr.readyState === 4) {
                    scanButton.disabled = false;
                    if (xhr.status === 200) {
                        try {
                            const aps = JSON.parse(xhr.responseText);
                            if (aps && aps.length > 0) {
                                aps.forEach(function(ap) {
                                    const listItem = document.createElement('li');
                                    let details = `訊號: ${ap.rssi} dBm`;
                                    if (ap.security && ap.security !== "OPEN") {
                                        details += `, 安全性: ${ap.security}`;
                                    } else if (ap.security === "OPEN") {
                                        details += `, 安全性: 開放`;
                                    }
                                    listItem.innerHTML = `<span class="ap-ssid">${ap.ssid}</span><br><span class="ap-details">${details}</span>`;
                                    listItem.addEventListener('click', function() {
                                        ssidInput.value = ap.ssid;
                                        passwordInput.focus();
                                    });
                                    apListUl.appendChild(listItem);
                                });
                                apListContainer.style.display = 'block';
                                scanStatusDiv.innerHTML = `<span class="success">掃描完成，找到 ${aps.length} 個網路。</span>`;
                                scanStatusDiv.className = 'success';
                            } else {
                                scanStatusDiv.innerHTML = '<span class="info">未找到任何 Wi-Fi 網路。</span>';
                                scanStatusDiv.className = 'info';
                            }
                        } catch (e) {
                            console.error("解析 AP 列表 JSON 錯誤:", e);
                            console.error("收到的回應:", xhr.responseText);
                            scanStatusDiv.innerHTML = '<span class="error">掃描失敗：無法解析伺服器回應。</span>';
                            scanStatusDiv.className = 'error';
                        }
                    } else {
                        console.error("掃描 AP 錯誤:", xhr.status, xhr.statusText);
                        scanStatusDiv.innerHTML = `<span class="error">掃描失敗：伺服器錯誤 ${xhr.status}。</span>`;
                        scanStatusDiv.className = 'error';
                    }
                }
            };
            xhr.onerror = function() {
                scanButton.disabled = false;
                scanStatusDiv.innerHTML = '<span class="error">掃描失敗：網路請求錯誤。請檢查設備連線。</span>';
                scanStatusDiv.className = 'error';
            };
            xhr.send();
        });

        wifiForm.addEventListener('submit', function(event) {
            event.preventDefault();
            const ssid = ssidInput.value.trim();
            const password = passwordInput.value;
            if (!ssid) {
                statusMessageDiv.innerHTML = '<span class="error">請輸入 Wi-Fi 名稱 (SSID)。</span>';
                statusMessageDiv.className = 'error';
                ssidInput.focus();
                return;
            }
            statusMessageDiv.innerHTML = '<span class="info">嘗試WIFI連線，注意連線燈號！</span>';
            statusMessageDiv.className = 'info';
            connectButton.disabled = true;
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/connect_wifi', true);
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.onreadystatechange = function() {
                if (xhr.readyState === 4) {
                    connectButton.disabled = false;
                    if (xhr.status === 200) {
                        if (xhr.responseText.toUpperCase().includes("SUCCESS")) {
                             statusMessageDiv.innerHTML = '<span class="success">成功送出連線請求！請檢查設備狀態。</span>';
                             statusMessageDiv.className = 'success';
                        } else {
                             statusMessageDiv.innerHTML = `<span class="error">連線失敗：${xhr.responseText || '未知錯誤'}</span>`;
                             statusMessageDiv.className = 'error';
                        }
                    }
                    // } else {
                    //     statusMessageDiv.innerHTML = `<span class="error">連線請求失敗：伺服器錯誤 ${xhr.status}。</span>`;
                    //     statusMessageDiv.className = 'error';
                    // }
                }
            };
            // xhr.onerror = function() {
            //     connectButton.disabled = false;
            //     statusMessageDiv.innerHTML = '<span class="error">連線請求失敗：網路錯誤。</span>';
            //     statusMessageDiv.className = 'error';
            // };
            const params = 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password);
            xhr.send(params);
        });

        function checkWIFISetupStatus() {   // eheck wifi connection status after submit;

            console.log('check wifi connect status!');
            const xhr = new XMLHttpRequest();
            xhr.open('GET', '/connect_wifi_status', true);
            xhr.onreadystatechange = function() {
                console.log('xhr on ready state!!');
                if (xhr.readyState === 4) {
                    console.log('get result.....');
                    scanButton.disabled = false;
                    if (xhr.status === 200) {
                        console.log('STATUS 200');
                        if(xhr.responseText.toUpperCase().includes("SUCCESS")) {
                            statusMessageDiv.innerHTML = '<span class="info">WIFI連線測試成功，請RESET</span>';
                            statusMessageDiv.className = 'info';
                            connectButton.disabled = true;
                        } else {
                            connectButton.disabled = false;
                        }
                    } else {
                        // Error
                        connectButton.disabled = false;
                    }
                }
                window.setTimeout(checkWIFISetupStatus, 5000);
            };
            xhr.send();
            console.log('check wifi connect end!');
        }

        window.setTimeout(checkWIFISetupStatus, 5000);

    </script>
</body>
</html>
)rawliteral";

void handleWIFIAPWebProcess(WiFiClient client) {

//   if (client) { // 如果有客戶端連接

//         Serial.println("\n[+] New client connected!");
//         String currentLine = ""; // 用於儲存客戶端發來的 HTTP 請求

//         unsigned long currentTime = millis();
//         unsigned long previousTime = currentTime;

//         enum {HEADER, BODY} state = HEADER; // 區分目前是在header 或是 body

//         while (client.connected() && (currentTime - previousTime <= 2000)) { // 連線保持且未超時 (2秒)
//             currentTime = millis();
//             if (client.available()) { // 如果客戶端有數據發送
//                 char c = client.read(); // 讀取一個字節
//                 Serial.write(c);      // 在 Serial Monitor 上印出請求 (除錯用)
                
//                 if (c == '\n') { // 如果讀到換行符

//                     // 如果當前行是空的 (連續兩個換行符)，表示 HTTP 請求頭結束
//                     if (currentLine.length() == 0) {
//                         // 解析請求路徑
//                         Serial.println("===================");
//                         Serial.print("htmlHeader: ");
//                         Serial.println(htmlHeader);
//                         Serial.println("===================");

//                         if (htmlHeader.startsWith("GET /scan_wifi_aps")) {
//                             handleScanWiFi(client);
//                         } else if (htmlHeader.startsWith("POST /connect_wifi")) {
//                             // 為了處理 POST 數據，我們需要讀取請求體
//                             // 首先，找到 Content-Length
//                             int contentLength = 0;
//                             String postBody = "";
//                             // 繼續讀取 headers 直到找到 Content-Length 或空行
//                             while(client.available()){
//                                 String line = client.readStringUntil('\r');
//                                 client.read(); // 讀掉 '\n'
//                                 Serial.println(line); // Debug headers
//                                 if(line.startsWith("Content-Length: ")){
//                                     contentLength = line.substring(String("Content-Length: ").length()).toInt();
//                                 }
//                                 if(line.length() == 0) break; // Header 結束
//                             }
//                             // 讀取 POST body
//                             if(contentLength > 0){
//                                 size_t len_to_read_contentLength = (contentLength > 0) ? (size_t)contentLength : 0; // 重命名以示區分
//                                 while(postBody.length() < len_to_read_contentLength && client.available()) { // 使用 contentLength
//                                     postBody += (char)client.read();
//                                 }
//                                 Serial.print("POST Body: "); Serial.println(postBody);
//                             }
//                             handleConnectWiFi(client, postBody);
//                         } else if (htmlHeader.startsWith("GET /")) { // 預設根路徑，提供 HTML 頁面
//                             handleWifiPageRoot(client);
//                         } else {
//                             // handleNotFound(client);
//                             handleWifiPageRoot(client);
//                         }
//                         break; // 跳出 while client.available() 迴圈
//                     } else { // 如果不是空行，清空 currentLine 準備接收下一行
//                         currentLine = "";
//                     }
//                 } else if (c != '\r') { // 如果不是回車符
//                     currentLine += c; // 將字元加到 currentLine
//                 }

//                     // 記錄第一行請求 (HTTP Header)
//                     if (currentLine.endsWith("\n") && htmlHeader.length() == 0) {
//                         htmlHeader = currentLine;
//                         htmlHeader.trim(); // 去除前後空白
//                         // 如果是 POST 請求，currentLine 在這裡會是 POST /connect_wifi ...
//                         // 如果是 GET，currentLine 會是 GET / ...
//                         // 但因為我們在上面判斷空行時才處理，所以這裡 header 變數主要用於 GET
//                         // 對於 POST，我們會在讀完所有 header 後處理
//                     }

//             }
//         }
        
//         // 清理
//         htmlHeader = ""; // 清空 header 供下次請求使用
//         client.stop(); // 關閉連接
//         Serial.println("[*] Client disconnected.");
//     }

    if (client) {                          // if you get a client,
    
        Serial.println("\nNew client connected");
        String currentLine = "";       // 用於存儲來自客戶端的一行數據
        String requestMethod = "";
        String requestPath = "";
        long contentLength = 0;
        String contentType = "";
        bool isPostRequest = false;
        String postData = "";

        // HTTP 請求通常以空行結束請求頭
        // 我們需要先讀取請求頭，然後再根據 Content-Length 讀取請求體
        enum {HEADER, BODY} state = HEADER;

        // unsigned long startTime = millis(); // 用於超時
        Serial.println("new client");      // print a message out the serial port
        
        while (client.connected()) {       // loop while the client's connected
            if (client.available()) {      // if there's bytes to read from the client,
                
                char c = client.read();    // 讀取一個字節
                Serial.write(c);           // 在序列監視器上打印出來 (調試用)

                if (state == HEADER) {
                    if (c == '\n') { // 如果讀到換行符
                        // 檢查是否是空行 (請求頭結束)
                        if (currentLine.length() == 0) {
                            Serial.println("End of HTTP headers.");
                            if (isPostRequest && contentLength > 0) {
                                state = BODY; // 進入讀取請求體狀態
                                postData.reserve(contentLength + 1); // 為請求體預分配內存
                                Serial.print("Expecting POST body of length: "); Serial.println(contentLength);
                            } else {
                                // 如果不是 POST 或沒有 Content-Length，則準備發送響應
                                break; // 跳出讀取循環，處理請求
                            }
                        } else { // 如果不是空行，則處理這一行請求頭
                            if (requestMethod.length() == 0) { // 通常第一行是請求行
                                if (currentLine.startsWith("POST ")) {
                                    isPostRequest = true;
                                    requestMethod = "POST";
                                    int pathStart = currentLine.indexOf(' ') + 1;
                                    int pathEnd = currentLine.indexOf(' ', pathStart);
                                    if (pathEnd != -1) {
                                        requestPath = currentLine.substring(pathStart, pathEnd);
                                    }
                                    Serial.print("Method: POST, Path: "); Serial.println(requestPath);
                                } else if (currentLine.startsWith("GET ")) {
                                    requestMethod = "GET";
                                    int pathStart = currentLine.indexOf(' ') + 1;
                                    int pathEnd = currentLine.indexOf(' ', pathStart);
                                    if (pathEnd != -1) {
                                        requestPath = currentLine.substring(pathStart, pathEnd);
                                    }
                                    Serial.print("Method: GET, Path: "); Serial.println(requestPath);
                                }
                            } else { // 處理其他請求頭
                                if (currentLine.startsWith("Content-Length: ")) {
                                    contentLength = currentLine.substring(16).toInt(); // "Content-Length: " 長度為16
                                } else if (currentLine.startsWith("Content-Type: ")) {
                                    contentType = currentLine.substring(14); // "Content-Type: " 長度為14
                                }
                            }
                            currentLine = ""; // 清空当前行，準備讀取下一行
                        }
                    } else if (c != '\r') { // 如果不是回車符 (通常忽略回車符)
                        currentLine += c;      // 將字元添加到当前行
                    }
                } else if (state == BODY) { // 讀取請求體
                    postData += c;
                    if (postData.length() >= contentLength) {
                        Serial.println("\nPOST Body completely received:");
                        Serial.println(postData);
                        break; // 請求體讀取完畢，跳出循環
                    }
                }
            }
        }
        
        // ---- 請求處理和響應 ----
        if (requestMethod == "GET" && requestPath == "/") {
            handleWifiPageRoot(client);     // index.html 網頁
        } else if (requestMethod == "GET" && requestPath == "/scan_wifi_aps") {
            handleScanWiFi(client);      
        } else if (requestMethod == "GET" && requestPath == "/connect_wifi_status") {
            Serial.println("[+] GET /connect_wifi_status");
            client.println("HTTP/1.1 200");
            client.println("Content-type:text/plain");
            client.println("Connection: close");
            client.println();
            if(isSuccessWIFIConnect) {
                client.println("SUCCESS");
            } else {
                client.println("ERROR");
            }
            
        } else if (isPostRequest && requestPath == "/connect_wifi") {   // 按下連線WIFI

            if (contentType.startsWith("application/x-www-form-urlencoded")) {

                // parseFormData(postData); // 解析表單數據
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/plain");
                client.println("Connection: close");
                client.println();
                client.println("嘗試連結中....."); // 回傳簡單的成功/失敗訊息
                Serial.print("[+] Sent connection status: "); 
                Serial.println("嘗試連結中.....");
                delay(3000);
                handleConnectWiFi(client, postData);

            } else {
                Serial.print("Unsupported POST Content-Type: "); Serial.println(contentType);
                // 在這裡你可以嘗試解析其他類型，例如 JSON
            }

            // 發送成功響應
            // client.println("HTTP/1.1 200 OK");
            // client.println("Content-type:text/html");
            // client.println("Connection: close");
            // client.println();
            // client.println("<!DOCTYPE HTML><html><body><h1>Data Received!</h1>");
            // client.println("<p>SSID: " + _post_ssid + "</p>");
            // client.println("<p>PASSWORD: " + _post_password + "</p>");
            // client.println("<a href='/'>Go Back</a></body></html>");
        } else {
            // 404 Not Found
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-type:text/plain");
            client.println("Connection: close");
            client.println();
            client.println("Not Found");
        }

        // 給客戶端一些時間接收數據，然後關閉連接
        delay(10);
        client.stop();
        Serial.println("Client disconnected.");

    }

}

void handleWifiPageRoot(WiFiClient client) {
    Serial.println("[*] Serving HTML page for /");
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println(); // HTTP headers 結束，空行

    // 發送 HTML 內容
    // 從 PROGMEM 讀取並發送
    size_t html_page_len = strlen_P(HTML_PAGE); // 獲取 HTML 總長度
    char print_buffer[65]; // 緩衝區大小，64 字元 + null 終止符
    
    for (size_t i = 0; i < html_page_len; ) {
        size_t current_chunk_size = 0;
        // 計算本次要讀取的字節數 (最多 64)
        for (size_t j = 0; j < 64 && (i + j) < html_page_len; ++j) {
            // 從 PROGMEM 讀取一個字節到緩衝區
            print_buffer[j] = pgm_read_byte(&HTML_PAGE[i + j]); 
            current_chunk_size++;
        }
        print_buffer[current_chunk_size] = '\0'; // 在緩衝區末尾添加 null 終止符
        
        client.print(print_buffer); // 發送當前區塊的內容
        i += current_chunk_size;    // 更新已讀取的索引
    }
    client.println(); // 確保內容發送完畢後有一個換行 (某些客戶端可能需要)
}

void handleScanWiFi(WiFiClient client) {

    // 顯示掃描
    // u8g2.clearBuffer();
    drawAutoWrappedText(u8g2, 0, 12, 128, "掃描WIFI服務..");
    // u8g2.sendBuffer();
    
    Serial.println("[*] Handling /scan_wifi_aps");
    
    int n = WiFi.scanNetworks(); // 執行 Wi-Fi 掃描
    Serial.print(n);
    Serial.println(" networks found");

    // DynamicJsonDocument doc(1024 + (n * 128)); // 根據 AP 數量動態調整 JSON 文檔大小，每個AP預估128字節
    JsonDocument doc; // 新的 (ArduinoJson v6+) - 容量會自動調整，或按需指定
    JsonArray array = doc.to<JsonArray>();

    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            // JsonObject ap = array.createNestedObject();
            JsonObject ap = array.add<JsonObject>();   // 新的，解決 ArduinoJson 棄用警告
            ap["ssid"] = WiFi.SSID(i);
            ap["rssi"] = WiFi.RSSI(i);
            
            // 使用 Ameba SDK 可能的加密類型常數
            // 注意：這些常數的確切名稱可能因 Ameba SDK 版本而略有不同。
            // 您可能需要查閱 Ameba Pro 2 的 SDK 文件或相關的頭文件 (例如 wifi_constants.h 或類似文件)
            // 來確認正確的常數名稱。
            switch (WiFi.encryptionType(i)) {
                // 這些是基於常見 Realtek SDK (如 RTL872x) 的猜測
                case SECURITY_OPEN:             // 替換 WIFI_AUTH_OPEN
                    ap["security"] = "OPEN"; 
                    break;
                case SECURITY_WEP_PSK:          // 替換 WIFI_AUTH_WEP (WEP 通常是 PSK)
                case SECURITY_WEP_SHARED:
                // case SECURITY_WEP_SHARED:       // WEP 也可能有 Shared Key 模式
                    ap["security"] = "WEP"; 
                    break;
                case SECURITY_WPA_TKIP_PSK:
                case SECURITY_WPA_AES_PSK:
                // case SECURITY_WPA_AES_PSK:      // 替換 WIFI_AUTH_WPA_PSK
                    ap["security"] = "WPA_PSK"; 
                    break;
                case SECURITY_WPA2_AES_PSK:
                case SECURITY_WPA2_TKIP_PSK:
                case SECURITY_WPA2_MIXED_PSK:
                case WPA2_SECURITY:
                // case SECURITY_WPA2_TKIP_PSK:
                    ap["security"] = "WPA2_PSK"; 
                    break;
                case SECURITY_WPA_WPA2_MIXED: // 替換 WIFI_AUTH_WPA_WPA2_PSK
                    ap["security"] = "WPA/WPA2_PSK"; 
                    break;
                case SECURITY_WPA3_AES_PSK:
                    ap["security"] = "WPA3_AES";
                    break;
                case SECURITY_WPA2_WPA3_MIXED:
                    ap["security"] = "WPA2/WPA3_PSK";
                    break;
                case AES_ENABLED:
                    ap["security"] = "AES";
                    break;
                case TKIP_ENABLED:
                    ap["security"] = "TKIP";
                    break;
                // case case WEP_ENABLED::
                //     ap["security"] = "WEP";
                //     break;
                // 如果 Ameba Pro 2 SDK 使用不同的枚舉值表示企業級加密，也需要添加
                // case RTW_SECURITY_WPA_ENTERPRISE: 
                // case RTW_SECURITY_WPA2_ENTERPRISE:
                //    ap["security"] = "WPA2_ENTERPRISE"; break;
                default:
                    ap["security"] = "UNKNOWN (" + String(WiFi.encryptionType(i)) + ")"; // 顯示原始枚舉值以供調試
            }
        }
    }
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Access-Control-Allow-Origin: *"); 
    client.println("Connection: close");
    client.println();
    client.println(jsonResponse);
    Serial.println("[+] Sent AP list JSON response.");

    // 顯示掃描
    // u8g2.clearBuffer();
    drawAutoWrappedText(u8g2, 0, 12, 128, "掃描完成!");
    // u8g2.sendBuffer();

}

void wifiAPProcess() {

    Serial.print("WIFI AP Start:");
    Serial.println(apSSID);
    status = WiFi.apbegin(apSSID, apPASSWORD, channel, ssid_status);
    delay(2000);
    Serial.println("Complete WIFI AP process.");
    printWifiStatus();
    Serial.println("starting web server..");
    if(!isWebServerEnabled) {
        server.begin();
        Serial.println("[+] Start Web Server!");
        isWebServerEnabled = true;
    } else {
        Serial.println("[!] Web Server is started alreade. Don't restart again!");
    }
}

void handleConnectWiFi(WiFiClient client, String postData) {

    // 顯示掃描
    // u8g2.clearBuffer();
    drawAutoWrappedText(u8g2, 0, 2, 128, "WIFI設定中..");
    // u8g2.sendBuffer();

    Serial.println("[*] Handling /connect_wifi");

    parseFormData(postData);

    String responseMessage = "";
    bool success = false;

    // // **您需要在這裡實現實際的 Wi-Fi 連線邏輯**
    // // 解析 postData (例如: "ssid=MyNetwork&password=MyPassword")
    // String ssid_param = "";
    // String pass_param = "";

    // int ssid_start = postData.indexOf("ssid=");
    // int pass_start = postData.indexOf("password=");
    // int ssid_end = postData.indexOf('&', ssid_start);
    
    // if (ssid_start != -1) {
    //     if (ssid_end != -1 && ssid_end > ssid_start) { // 如果密碼也存在
    //         ssid_param = postData.substring(ssid_start + 5, ssid_end);
    //     } else { // 只有 SSID，沒有密碼（或者密碼是最後一個參數）
    //          ssid_param = postData.substring(ssid_start + 5);
    //          // 如果 password 在 ssid 之前，這個邏輯需要調整，但通常 ssid 在前
    //     }
    // }
    // if (pass_start != -1) {
    //     pass_param = postData.substring(pass_start + 9); // 密碼是最後一個參數
    // }
    
    // // URL 解碼 (非常簡化的版本，實際應使用庫或更完整的實現)
    // ssid_param.replace("+", " "); // 空格被編碼為 +
    // ssid_param.replace("%20", " ");
    // ssid_param.replace("%21", "!"); // ...等等，根據需要添加更多
    // // ...對 pass_param 也做同樣處理

    // Serial.print("  Attempting to connect to SSID: '");
    // Serial.print(ssid_param);
    // Serial.print("' with Password: '");
    // Serial.print(pass_param);
    // Serial.println("'");

    // responseMessage = "TRY: Connected to " + _post_ssid;

    // client.println("HTTP/1.1 200 OK");
    // client.println("Content-Type: text/plain");
    // client.println("Connection: close");
    // client.println();
    // client.println(responseMessage); // 回傳簡單的成功/失敗訊息

    // 實際連線嘗試
    // server.stop();
    // delay(100);
    digitalWrite(WIFIAP_LED, LOW);
    digitalWrite(WIFICLIENT_LED, LOW);
    WiFi.disconnect(); // 先斷開之前的連接
    delay(100);

    strncpy(ssid, _post_ssid.c_str(), sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0'; // 確保 null 結尾
    strncpy(pass, _post_password.c_str(), sizeof(pass) - 1);
    pass[sizeof(pass) - 1] = '\0'; // 確保 null 結尾
    WiFi.begin(ssid, pass);

    int retries = 0;
    const int maxRetries = 30; // 大約 15 秒 (30 * 500ms)
    while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {

        // 顯示掃描
        

        // 確定WIFI可以成功連線
        digitalWrite(WIFICLIENT_LED, HIGH);
        Serial.println("\n[+] Wi-Fi Connected!");
        Serial.print("[+] IP Address: ");
        Serial.println(WiFi.localIP());
        responseMessage = "SUCCESS: Connected to " + _post_ssid;
        success = true;
        isSuccessWIFIConnect = true;

        saveWifiCredentials(ssid, pass);
        // 要將目前需要儲存的SSID/Password儲存到Flash
        // 連線成功後，您可能希望停止 AP 模式 (如果不再需要配置)
        // Serial.println("[*] AP mode stopped.");
        WiFi.disconnect(); // 先斷開之前的連接
        // delay(2000);
        // wifiAPProcess();
        // Serial.print("[*] AP mode re-enabled. AP IP: "); 
        // printWifiStatus();

        // NVIC_SystemReset();
        // u8g2.clearBuffer();
        topMenuArea(u8g2, "SUCCESS", 0, 0);
        drawAutoWrappedText(u8g2, 0, 2, 128, "順利連上WIFI AP\n請重啟裝置！");
        delay(30000);
        // u8g2.sendBuffer();

    } else {
         digitalWrite(WIFICLIENT_LED, LOW);
        Serial.println("\n[-] Wi-Fi Connection Failed.");
        responseMessage = "FAILURE: Could not connect to " + _post_ssid;
        success = false;

        // u8g2.clearBuffer();
        topMenuArea(u8g2, "FAILED", 0, 0);
        drawAutoWrappedText(u8g2, 0, 2, 12, "WIFI連線失敗\n請重啟裝置再進行設定！");
        // u8g2.sendBuffer();
        // 連線失敗，可能重新啟動 AP 模式讓使用者重試
        if (ap_password && strlen(ap_password) > 0) WiFi.apbegin(apSSID, apPASSWORD, channel, ssid_status);
        else WiFi.apbegin(apSSID, apPASSWORD, channel, ssid_status);
        Serial.print("[*] AP mode re-enabled. AP IP: "); 
       printWifiStatus();
       digitalWrite(WIFIAP_LED, HIGH);
       
    }

    // client.println("HTTP/1.1 200 OK");
    // client.println("Content-Type: text/plain");
    // client.println("Connection: close");
    // client.println();
    // client.println(responseMessage); // 回傳簡單的成功/失敗訊息
    // Serial.print("[+] Sent connection status: "); Serial.println(responseMessage);
}

void handleNotFound(WiFiClient client) {
    Serial.println("[!] Client requested unknown resource.");
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-type:text/plain");
    client.println("Connection: close");
    client.println();
    client.println("404 Not Found");
}

void printWifiStatus()
{
    // print the SSID of the network you're attached to:
    Serial.println();
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your WiFi IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
}

// 輔助函數：解析 URL 編碼的表單數據
// 注意：這是一個非常簡化的解析器，可能無法處理所有邊界情況
void parseFormData(String formData) {
    int currentPos = 0;
    while (currentPos < formData.length()) {
        int nextAmp = formData.indexOf('&', currentPos);
        if (nextAmp == -1) {
            nextAmp = formData.length();
        }
        String pair = formData.substring(currentPos, nextAmp);
        currentPos = nextAmp + 1;

        int eqPos = pair.indexOf('=');
        if (eqPos != -1) {
            String key = pair.substring(0, eqPos);
            String value = pair.substring(eqPos + 1);
            // 進行簡單的 URL 解碼 (只處理 %20 -> 空格，實際需要更完整的解碼)
            value.replace("%20", " ");
            value.replace("+", " "); // 加號也代表空格

            if (key == "ssid") {
                _post_ssid = value;
            } else if (key == "password") {
                _post_password = value;
            }
            Serial.print("Parsed: "); Serial.print(key); Serial.print(" = "); Serial.println(value);
        }
    }
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
// 你的 code...
#pragma GCC diagnostic pop

#include <WiFi.h>
#include "audio_api.h"
#include "opus.h" // 確保包含 Opus 頭文件
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "AIAssistant.h"
#include "wifi_html.h"
#include <U8g2lib.h>
#include "v7_tft_lcd.h"


WiFiClient wifiClient;
PubSubClient client(wifiClient);

// 全局變數
audio_t audio_dev;

__attribute__((aligned(32))) uint8_t rx_buf[AUDIO_DMA_PAGE_SIZE * PAGE_NUM];
__attribute__((aligned(32))) uint8_t tx_buf[AUDIO_DMA_PAGE_SIZE * PAGE_NUM];

OpusEncoder *encoder; // Opus 編碼器實例 **
OpusDecoder *decoder; // Opus 解碼器實例 **

unsigned char opus_encoded_buffer[MAX_ENCODED_PACKET_SIZE]; // 存放本地編碼後的數據
opus_int16 pcm_decoded_buffer[FRAME_SIZE_SAMPLES * CHANNELS]; // ** 存放從 MQTT 解碼後的 PCM 數據 **

volatile bool new_audio_data_ready_for_opus_encoding = false;
volatile uint8_t* dma_filled_rx_buffer_ptr = NULL;

opus_int16 playback_ring_buffer[PLAYBACK_RING_BUFFER_FRAMES][FRAME_SIZE_SAMPLES * CHANNELS];
volatile int playback_write_idx = 0; // 主循環寫入解碼數據的位置
volatile int playback_read_idx = 0;  // audio_tx_handler 讀取數據播放的位置
volatile int playback_frames_in_buffer = 0; // 緩衝區中實際可播放的幀數

volatile bool new_opus_data_received_for_decoding = false;
volatile uint8_t mqtt_received_opus_payload[MAX_ENCODED_PACKET_SIZE]; // 假設 MQTT 包不會超過此大小
volatile unsigned int mqtt_received_opus_length = 0;

volatile bool vad_voice_active_flag = false; // VAD 檢測到的語音活動狀態 (考慮 hangover 後)
unsigned long vad_last_voice_time_ms = 0;   // 上次檢測到語音的時間戳
const unsigned long VAD_HANGOVER_DURATION_MS = 300; // 語音結束後的掛起時間 (毫秒)

const long long VAD_ENERGY_THRESHOLD = 150000; // 示例值 (總能量，不是平均能量)
                                                // 對於 960 個樣本，每個樣本幅值約 228 (228^2 * 960 ~= 50M)
                                                // 如果 PCM 幅度在 +/- 32768 之間
bool vad_debug_print_energy = false; // 設置為 true 以打印每幀能量用於調試閾值
// ------------------------------------

/// --- JSON 相關 ---
// 直接指定 JSON 文檔的最大容量 (以字節為單位)
// 這個大小需要包含 JSON 結構本身以及所有字串的總長度。
// 你可以從一個較大的值開始，然後用 ArduinoJson Assistant (在官網上) 估算精確值。
// const int JSON_STATUS_CAPACITY = 256; // 示例大小，請根據你的 JSON 內容調整
StaticJsonDocument<JSON_STATUS_CAPACITY> jsonStatusDoc;

char jsonOutputBuffer[JSON_MAX_BUFFER]; // 用於序列化 JSON 字串 (這個可以保留或根據需要調整)

// const int JSON_COMMAND_CAPACITY = 256; // 示例大小
StaticJsonDocument<JSON_COMMAND_CAPACITY> jsonCommandDoc;

// const int JSON_AUDIO_PARAMS_CAPACITY = 256;
StaticJsonDocument<JSON_AUDIO_PARAMS_CAPACITY> jsonAudioParamsDoc;

// --------------------

// 定期發送 JSON 狀態的時間間隔
const unsigned long JSON_STATUS_INTERVAL_MS = 5000; // 每 5 秒發送一次
unsigned long last_json_status_sent_time_ms = 0;

bool isRobotReady = false;              // 機器人是否已經啟動完成;
bool isRobotListen = false;             // 機器人是否正在等待回應;
int currentRobotState = ROBOT_INIT;

bool isMQTTJsonProcess = false;

volatile bool isTTSProcess = false;

// handle receive mix audio stream handler.
void rx_irq_handler(uint32_t arg, uint8_t *pbuf) {
    dma_filled_rx_buffer_ptr = pbuf;
    new_audio_data_ready_for_opus_encoding = true;
    audio_set_rx_page(&audio_dev);
}

void reconnectMQTT()
{
    // Loop until we're reconnected
    while (!(client.connected())) {

        currentRobotState = ROBOT_CONNECT_MQTT;
        isRobotReady = false;
        isRobotListen = false;
        
        Serial.print("\r\nAttempting MQTT connection...");
        // Attempt to connect
        if (client.connect(clientId)) {
        
            Serial.println("connected");
            // Once connected, publish an announcement and resubscribe
            // client.publish(publishTopic, publishPayload);

            client.subscribe(subscribeTopic);
            client.subscribe(subscribeAudioTopic);

            currentRobotState = ROBOT_SAY_HELLO;
            isRobotReady = true;
            isRobotListen = false;

            sendHelloToServer();

        } else {

            Serial.println("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);

        }
    }
}

// Send Listen state to MQTT broker;
bool sendListenStateToServer(char* state, char* mode, char* text) {

    while(isMQTTJsonProcess) {
        delay(1);
    }

    isMQTTJsonProcess = true;

    bool isSuccess = false;

    // Send Hello Command to MQTT;
    jsonCommandDoc.clear();
    jsonCommandDoc["type"] = "listen";
    jsonCommandDoc["state"] = state;
    jsonCommandDoc["mode"] = mode;
    jsonCommandDoc["text"] = text;

    size_t json_len = serializeJson(jsonCommandDoc, jsonOutputBuffer);

    if (json_len > 0 && json_len < sizeof(jsonOutputBuffer)) {
        bool published = client.publish(publishTopic, jsonOutputBuffer, false);
        if (published) { 
            isSuccess = true;
        } 
    }

    isMQTTJsonProcess = false;

    return isSuccess;

}

bool sendHelloToServer() {

    while(isMQTTJsonProcess) {
        delay(1);
    }

    isMQTTJsonProcess = true;   // 設定目前正在準備傳送MQTT JSON Payload;

    // Send Hello Command to MQTT;
    jsonAudioParamsDoc.clear();
    jsonAudioParamsDoc["format"] = "opus";
    jsonAudioParamsDoc["sample_rate"] = 16000;
    jsonAudioParamsDoc["channels"] = 1;
    jsonAudioParamsDoc["frame_duration"] = 60;

    jsonCommandDoc.clear();
    jsonCommandDoc["type"] = "hello";
    jsonCommandDoc["version"] = "1.0";
    jsonCommandDoc["transport"] = "mqtt";
    jsonCommandDoc["audio_params"] = jsonAudioParamsDoc;

    size_t json_len = serializeJson(jsonCommandDoc, jsonOutputBuffer);

    bool isSuccess = false;

    if (json_len > 0 && json_len < sizeof(jsonOutputBuffer)) {
        bool published = client.publish(publishTopic, jsonOutputBuffer, false);
        if (published) { 
            isSuccess = true;
        }
    } 
    else { 
        Serial.println("JSON: Serialization failed or buffer too small."); 
    }

    isMQTTJsonProcess = false;  // 解除MQTT JSON Payload傳送狀態;

    return isSuccess;

}

bool sendAbortToServer(char* reason) {

    while(isMQTTJsonProcess) {
        delay(1);
    }

    isMQTTJsonProcess = true;

    bool isSuccess = false;

    // Send Hello Command to MQTT;
    jsonCommandDoc.clear();
    jsonCommandDoc["type"] = "abort";
    jsonCommandDoc["reason"] = reason;

    size_t json_len = serializeJson(jsonCommandDoc, jsonOutputBuffer);

    if (json_len > 0 && json_len < sizeof(jsonOutputBuffer)) {
        bool published = client.publish(publishTopic, jsonOutputBuffer, false);
        if (published) { 
            isSuccess = true;
        } 
    }

    isMQTTJsonProcess = false;

    return isSuccess;

}

void audio_tx_handler(uint32_t arg, uint8_t *pbuf) {

     // pbuf 是剛播放完的頁面，現在是空閒的 (理論上)
    // 我們需要從環形緩衝區獲取數據填充下一個 TX DMA page

    uint8_t* tx_page_to_fill = audio_get_tx_page_adr(&audio_dev); // 獲取一個可用的 TX page
    if (tx_page_to_fill) {
        bool data_played = false;
        // 臨界區保護 (簡化：實際應用可能需要更強的同步)
        // taskENTER_CRITICAL(); // 如果使用 FreeRTOS
        noInterrupts(); // 禁用中斷，防止競爭 playback_frames_in_buffer

        if (playback_frames_in_buffer > 0) {
            memcpy(tx_page_to_fill, playback_ring_buffer[playback_read_idx], PCM_BUFFER_SIZE_BYTES);
            playback_read_idx = (playback_read_idx + 1) % PLAYBACK_RING_BUFFER_FRAMES;
            playback_frames_in_buffer--;
            data_played = true;
            // Serial.print("TX IRQ: Played frame from RB. Buf count: "); Serial.println(playback_frames_in_buffer);
        }
        // taskEXIT_CRITICAL();
        interrupts(); // 重新啟用中斷

        if (!data_played) {
            // 緩衝區為空 (underrun)，填充靜音
            // Serial.println("TX IRQ: Playback buffer underrun, playing silence.");
            memset(tx_page_to_fill, 0, AUDIO_DMA_PAGE_SIZE);
        }
        audio_set_tx_page(&audio_dev, tx_page_to_fill); // 提交頁面給 DMA
    } else {
        // Serial.println("TX IRQ: No TX page available!"); // 這不應該經常發生
    }

}

// Callback function
void callback(char* topic, byte* payload, unsigned int length)
{
    // Serial.print("Message arrived [");
    // Serial.print(topic);
    // Serial.print("] ");
    // Serial.println();

    if (strcmp(topic, subscribeAudioTopic) == 0) {
        if (length > 0 && length <= MAX_ENCODED_PACKET_SIZE) {
            // ** 將收到的 Opus 數據複製到全局緩衝區，並設置標誌 **
            // ** 注意：這裡可能存在競爭條件，如果 MQTT 消息來得太快 **
            // ** 實際應用中，這裡應該放入一個隊列 **
            if(isTTSProcess) {

                if (!new_opus_data_received_for_decoding ) { // 簡單的防止覆蓋

                memcpy((void*)mqtt_received_opus_payload, payload, length);
                mqtt_received_opus_length = length;
                new_opus_data_received_for_decoding = true;
                // Serial.println("MQTT: Opus data queued for decoding.");
                
                } else {
                    Serial.println("MQTT: Decoder busy, dropping incoming Opus packet.");
                }

            } else {

                Serial.println("收到音訊，但是isTTSProcess是false;");

            }
            
        } else if (length > MAX_ENCODED_PACKET_SIZE) {
            Serial.println("MQTT: Received Opus packet too large, dropping.");
        }
    } else if (strcmp(topic, subscribeTopic) == 0) {
        // 收到來自於MQTT的Message;

        jsonCommandDoc.clear();
        DeserializationError error = deserializeJson(jsonCommandDoc, payload, length);

        if (error) {
            Serial.print("JSON: deserializeJson() failed: "); 
            Serial.println(error.f_str());
            return;
        }

        if (jsonCommandDoc.containsKey("type")) {
            
            const char* messageType = jsonCommandDoc["type"];
            Serial.print("JSON type: "); 
            Serial.println(messageType);
            if (strcmp(messageType, "hello") == 0) {
                // 表示已經成功得到hello;
                currentRobotState = ROBOT_IDLE;
                isRobotReady = true;
                isRobotListen = false;

                // 要開啟機器人端的監聽模式 (發送自動監聽給主機)
               
                char listenState[] = "start";
                char listenMode[] = "auto";
                char listenText[] = "";
                bool jsonResult = false;
                jsonResult = sendListenStateToServer(listenState, listenMode, listenText);

                if(jsonResult) {
                    currentRobotState = ROBOT_LISTEN;   // 進入監聽狀態;
                } else {
                    currentRobotState = ROBOT_ERROR;   // 進入監聽狀態;
                    Serial.println("ERROR: CAN't SEND LISTEN Payload to SERVER");
                }

                // u8g2.clearBuffer();
                // drawAutoWrappedText(u8g2, 20, 32, 108, "聆聽中...");
                // u8g2.sendBuffer();
                topMenuArea(u8g2, "聆聽中", 0, 0);

            } else if (strcmp(messageType, "tts") == 0) {

                const char* ttsState = jsonCommandDoc["state"];
                if(strcmp(ttsState, "start") == 0) {
                    //  表示主機開始發出TTS串流
                    isTTSProcess = true;
                    Serial.println("TTS START");
                    digitalWrite(WIFIAP_LED, HIGH);
                    topMenuArea(u8g2, "說話中", 0, 0);

                } else if(strcmp(ttsState, "stop") == 0) {
                    //  表示主機停止TTS串流
                    Serial.println("TTS STOP");
                    isTTSProcess = false;
                    digitalWrite(WIFIAP_LED, LOW);

                    // 確認機器人不是在IDEL中
                    if(currentRobotState != ROBOT_IDLE) {
                        // u8g2.clearBuffer();
                        // drawAutoWrappedText(u8g2, 20, 32, 108, "聆聽中...");
                        // u8g2.sendBuffer();
                        topMenuArea(u8g2, "聆聽中", 0, 0);

                    }   

                } else if(strcmp(ttsState, "sentence_start") == 0) {
                    // 表示目前串流的文字是
                    const char* ttsText = jsonCommandDoc["text"];
                    Serial.print("收到TTS文字：");
                    Serial.print(ttsText);
                    // u8g2.clearBuffer();
                    topMenuArea(u8g2, "說話中", 0, 0);
                    drawAutoWrappedText(u8g2, 0, 0, 118, ttsText, -1);
                    // u8g2.sendBuffer();
                }

            } else if (strcmp(messageType, "stt") == 0) {
                // 收到STT訊息
                const char* sttText = jsonCommandDoc["text"];
                Serial.print("收到STT文字：");
                Serial.println(sttText);
                // u8g2.clearBuffer();
                topMenuArea(u8g2, "我聽到..", 0, 0);
                drawAutoWrappedText(u8g2, 0, 0, 118, sttText,-1);
                // u8g2.sendBuffer();

            } else if (strcmp(messageType, "emotion") == 0) {
                // 收到EMOTION訊息
                const char* emotionText = jsonCommandDoc["text"];
                
                Serial.print("收到emotion：");
                Serial.println(emotionText);

            } else if (strcmp(messageType, "iot") == 0) {
                // 收到ios commands
                // const char* emotionText = jsonCommandDoc["text"];
                Serial.println("收到ios commands");
                // Serial.print(emotionText);
                
                const char* iotDevice = jsonCommandDoc["device"];
                const char* iotAction = jsonCommandDoc["action"];
                const char* iotObject = jsonCommandDoc["object"];
                const int iotValue = jsonCommandDoc["value"];

                topMenuArea(u8g2, "IOT控制", 0, 0);

                if(strcmp(iotDevice, "light") == 0) {       // 燈光控制
                    if(iotValue == 1) {
                        digitalWrite(lightGPIO, LOW);
                        drawAutoWrappedText(u8g2, 0, 0, 118, "開燈動作",-1);
                    } else {
                        digitalWrite(lightGPIO, HIGH);
                        drawAutoWrappedText(u8g2, 0, 0, 118, "關燈動作",-1);
                    }
                    delay(1000);
                } else if(strcmp(iotDevice, "fan") == 0) {       // 電風扇控制
                    if(iotValue == 1) {
                        digitalWrite(lightGPIO, LOW);
                        drawAutoWrappedText(u8g2, 0, 0, 118, "開電風扇動作",-1);
                    } else {
                        digitalWrite(lightGPIO, HIGH);
                        drawAutoWrappedText(u8g2, 0, 0, 118, "關電風扇動作",-1);
                    }
                    delay(1000);
                }


            }
        }

    }
}

void setup_audio_mic_input() {

    Serial.println("Audio: Initializing for MIC and Speaker...");
    audio_output_mode output_mode = OUTPUT_CAPLESS;
    audio_init(&audio_dev, output_mode, MIC_SINGLE_EDNED, AUDIO_CODEC_2p8V);
    Serial.println("Audio: Setting parameters (SR, WL, CH)...");
    audio_set_param_adv(&audio_dev, ASR_16KHZ, WL_16BIT, A_MONO, A_MONO);
    Serial.println("Audio: Setting DMA buffers...");
    audio_set_dma_buffer(&audio_dev, tx_buf, rx_buf, AUDIO_DMA_PAGE_SIZE, PAGE_NUM);
    Serial.println("Audio: Setting RX/TX pages...");

    delay(1000);

    uint32_t actual_num_pages = 0;
    switch (PAGE_NUM) {
        case AUDIO_PNUM_2: actual_num_pages = 2; break;
        case AUDIO_PNUM_3: actual_num_pages = 3; break;
        case AUDIO_PNUM_4: actual_num_pages = 4; break;
        default:
            Serial.println("错误：无效的 PAGE_NUM 配置！");
            return;
    }
    for (uint32_t i = 0; i < actual_num_pages; i++) {
        memset(&tx_buf[i * AUDIO_DMA_PAGE_SIZE], 0, AUDIO_DMA_PAGE_SIZE);
        audio_set_tx_page(&audio_dev, &tx_buf[i * AUDIO_DMA_PAGE_SIZE]);
        audio_set_rx_page(&audio_dev);
    }

    audio_rx_irq_handler(&audio_dev, rx_irq_handler, NULL);
    audio_tx_irq_handler(&audio_dev, audio_tx_handler, NULL);
    audio_mic_analog_mute(&audio_dev, false);
    audio_headphone_analog_mute(&audio_dev, false);
    audio_mic_analog_gain(&audio_dev, true, MIC_20DB);
    // audio_adc_digital_vol(&audio_dev, 0x9F);
    audio_dac_digital_vol(&audio_dev, DVOL_DAC_0DB); // 初始音量 (約 -24dB), DVOL_DAC_0DB (0xAF) 是最大
    delay(100);
    // audio_mic_analog_gain(&audio_dev, true, MIC_20DB);
    // audio_adc_digital_vol(&audio_dev, DVOL_ADC_0DB);
    // audio_mic_bias_ctrl(&audio_dev, true, BIAS_0p9_AVDD);
    Serial.println("Audio: Starting TRX (TX and RX)...");
    audio_trx_start(&audio_dev);

    Serial.print("Audio: RX start status: ");
    Serial.println(audio_get_rx_start_status(&audio_dev));
    Serial.print("Audio: TX start status: ");
    Serial.println(audio_get_tx_start_status(&audio_dev));
    Serial.print("Audio: RX Error Count: ");
    Serial.println(audio_get_rx_error_cnt(&audio_dev));
    Serial.print("Audio: TX Error Count: ");
    Serial.println(audio_get_tx_error_cnt(&audio_dev));
}

void setup_opus_codec() {
    int err;
    encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, APPLICATION_TYPE, &err);
    if (err < 0) {
        Serial.print("创建 Opus编码器失败: ");
        Serial.println(opus_strerror(err));
        // 處理錯誤，例如停止程序
        while(1);
    }
        Serial.print("Opus: Setting bitrate to ");
    Serial.println(OPUS_BITRATE_BPS); // 將會打印 16000
    err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_BITRATE_BPS));
    if (err < 0) {
        Serial.print("Opus: 设置 Opus 比特率失败: ");
        Serial.println(opus_strerror(err));
    }
    // 根據需要可以設置其他 Opus 參數
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5)); // 嘗試降低複雜度以減少CPU負載
    opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(10)); // 如果網路丟包嚴重，可以啟用FEC
    Serial.println("Opus: 编码器创建和配置成功。");

    // ** 初始化解碼器 **
    Serial.println("Opus: Creating decoder...");
    // 解碼器創建時，最後一個參數 flags 設為 0
    decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err < 0) {
        Serial.print("Opus: 创建 Opus 解码器失败: "); 
        Serial.println(opus_strerror(err)); 
        while(1);
    }
    // 通常不需要為解碼器設置太多參數，它會從 Opus 流中獲取信息
    Serial.println("Opus: 解码器创建成功。");
}

// VAD 初始化函數 (能量檢測 VAD不需要特別初始化，但保留函數結構)
void setup_vad() {
    Serial.println("VAD: Energy-based VAD initialized (no setup needed).");
    vad_voice_active_flag = false; // 初始狀態為無語音
    vad_last_voice_time_ms = 0;
}

void getWifiMac() {
    WiFi.macAddress(mac);
}



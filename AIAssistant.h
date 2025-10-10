#ifndef AI_ASSISTANT_H
#define AI_ASSISTANT_H

#include <WiFi.h>
#include "audio_api.h"
#include "opus.h" // 確保包含 Opus 頭文件
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "wifi_html.h"

#define DEBUG_MODE false

extern byte mac[6];
// extern char ssid[];    // your network SSID (name)
// // char ssid[] = "V7idea-UniFI";    // your network SSID (name)
// extern char pass[];        // your network password
extern int status;     // Indicator of Wifi status

extern char mqttServer[];
extern char clientId[];
extern char clientUser[];
extern char clientPass[];
extern char publishTopic[];
extern char publishPhotoTopic[];
extern char publishPayload[];
extern char subscribeTopic[];
extern char subscribeAudioTopic[];
extern int mqttPort;
extern bool isMQTTJsonProcess;

#define AUDIO_DMA_PAGE_SIZE      1920  // 每個 DMA 頁面的字節數
#define PAGE_NUM                 AUDIO_PNUM_4

// Opus 相關定義 (你需要根據你的需求調整這些值)
#define SAMPLE_RATE         16000 // 必須與 audio_set_param_adv 中設置的 ASR_16KHZ 一致
#define CHANNELS            1     // 必須與 audio_set_param_adv 中設置的 A_MONO 一致
#define APPLICATION_TYPE    OPUS_APPLICATION_VOIP // 或 OPUS_APPLICATION_AUDIO
#define FRAME_SIZE_MS       60    // Opus 支持的幀大小，例如 20ms
// FRAME_SIZE 是 opus_int16 樣本的數量, 不是字節數
// FRAME_SIZE_SAMPLES = SAMPLE_RATE * FRAME_SIZE_MS / 1000
#define FRAME_SIZE_SAMPLES  (SAMPLE_RATE * FRAME_SIZE_MS / 1000)
// Opus 編碼器需要的輸入緩衝區大小（字節） = FRAME_SIZE_SAMPLES * CHANNELS * sizeof(opus_int16)
// 確保 AUDIO_DMA_PAGE_SIZE 至少能容納一個 Opus 幀，或者你能從 DMA page 中湊出 Opus 幀
// 例如，如果 FRAME_SIZE_SAMPLES = 320 (16kHz, 20ms), sizeof(opus_int16)=2,
// 那麼一個 Opus 幀需要 320 * 1 * 2 = 640 字節。
// 如果你的 AUDIO_DMA_PAGE_SIZE 是 512，你需要累積數據。
// 這裡假設 AUDIO_DMA_PAGE_SIZE 恰好是一個或多個 Opus 幀的整數倍，或者你將在 loop 中處理部分幀。
// 為了簡化，我們先假設 AUDIO_DMA_PAGE_SIZE 包含我們要處理的數據量。
// 實際上，你可能需要一個更大的中間緩衝區來組合來自 DMA 的數據。

// #define MAX_PACKET_SIZE     (FRAME_SIZE_SAMPLES * 2) // Opus 編碼後的最大字節數，可以更精確估算
// unsigned char compressed_data[MAX_PACKET_SIZE]; // 用於存放編碼後的數據
#define OPUS_BITRATE_BPS    16000
#define MAX_ENCODED_PACKET_SIZE  256
#define PCM_BUFFER_SIZE_BYTES (FRAME_SIZE_SAMPLES * CHANNELS * sizeof(opus_int16)) // 960*1*2 = 1920 bytes

typedef enum {
	ROBOT_INIT = 0,
	ROBOT_CONNECT_WIFI = 1,
    ROBOT_WAIT_SND_MODULE_INIT = 2,
    ROBOT_CONNECT_MQTT = 3,
	ROBOT_SAY_HELLO = 4,
	ROBOT_IDLE = 5,
    ROBOT_LISTEN = 6,
    ROBOT_AI_RESPONSE = 6,
    ROBOT_ERROR = 99
} robotState;   // 設定機器人的狀態

#define    minSndPacketSize    15           // 確定最小當作接收到音訊 packet數量，當作語音用，約1秒
#define    maxSndPacketSize    1700         // 最大當作接收到音訊的 packet數量，預設為100秒
#define    emptySndSize        5            // 偵測到人聲後，延續多少snd packet數量送給主機

extern WiFiClient wifiClient;
extern PubSubClient client;

// 全局變數
extern audio_t audio_dev;

extern __attribute__((aligned(32))) uint8_t rx_buf[AUDIO_DMA_PAGE_SIZE * PAGE_NUM];
extern __attribute__((aligned(32))) uint8_t tx_buf[AUDIO_DMA_PAGE_SIZE * PAGE_NUM];

extern OpusEncoder *encoder; // Opus 編碼器實例 **
extern OpusDecoder *decoder; // Opus 解碼器實例 **

extern unsigned char opus_encoded_buffer[MAX_ENCODED_PACKET_SIZE]; // 存放本地編碼後的數據
extern opus_int16 pcm_decoded_buffer[FRAME_SIZE_SAMPLES * CHANNELS]; // ** 存放從 MQTT 解碼後的 PCM 數據 **


// ISR 和主循環通訊
extern volatile bool new_audio_data_ready_for_opus_encoding;
extern volatile uint8_t* dma_filled_rx_buffer_ptr;
extern volatile bool isTTSProcess;

// --- 播放環形緩衝區 ---
#define PLAYBACK_RING_BUFFER_FRAMES 4 // 緩衝區可以容納的幀數 (可調整)
extern opus_int16 playback_ring_buffer[PLAYBACK_RING_BUFFER_FRAMES][FRAME_SIZE_SAMPLES * CHANNELS];
extern volatile int playback_write_idx; // 主循環寫入解碼數據的位置
extern volatile int playback_read_idx;  // audio_tx_handler 讀取數據播放的位置
extern volatile int playback_frames_in_buffer; // 緩衝區中實際可播放的幀數


// ** 新增：用於標記是否有從 MQTT 來的數據需要播放 **
extern volatile bool new_opus_data_received_for_decoding;
extern volatile uint8_t mqtt_received_opus_payload[MAX_ENCODED_PACKET_SIZE]; // 假設 MQTT 包不會超過此大小
extern volatile unsigned int mqtt_received_opus_length;

// --- VAD (能量檢測) 相關定義 ---
extern volatile bool vad_voice_active_flag; // VAD 檢測到的語音活動狀態 (考慮 hangover 後)
extern unsigned long vad_last_voice_time_ms;   // 上次檢測到語音的時間戳
extern const unsigned long VAD_HANGOVER_DURATION_MS; // 語音結束後的掛起時間 (毫秒)
// ** VAD 能量閾值：這個值需要根據你的麥克風靈敏度和環境噪聲進行仔細調試！**
// ** 可以先設置一個較低的值，然後觀察安靜和有語音時的能量輸出來調整。**

extern const long long VAD_ENERGY_THRESHOLD; // 示例值 (總能量，不是平均能量)
                                                // 對於 960 個樣本，每個樣本幅值約 228 (228^2 * 960 ~= 50M)
                                                // 如果 PCM 幅度在 +/- 32768 之間
extern bool vad_debug_print_energy; // 設置為 true 以打印每幀能量用於調試閾值
// ------------------------------------

/// --- JSON 相關 ---
// 直接指定 JSON 文檔的最大容量 (以字節為單位)
// 這個大小需要包含 JSON 結構本身以及所有字串的總長度。
// 你可以從一個較大的值開始，然後用 ArduinoJson Assistant (在官網上) 估算精確值。
// extern const int JSON_STATUS_CAPACITY; // 示例大小，請根據你的 JSON 內容調整
#define JSON_STATUS_CAPACITY 256
extern StaticJsonDocument<JSON_STATUS_CAPACITY> jsonStatusDoc;

#define JSON_MAX_BUFFER 512
extern char jsonOutputBuffer[JSON_MAX_BUFFER]; // 用於序列化 JSON 字串 (這個可以保留或根據需要調整)

#define JSON_COMMAND_CAPACITY 1024
extern StaticJsonDocument<JSON_COMMAND_CAPACITY> jsonCommandDoc;

#define JSON_AUDIO_PARAMS_CAPACITY 512
extern StaticJsonDocument<JSON_AUDIO_PARAMS_CAPACITY> jsonAudioParamsDoc;

// --------------------

// 定期發送 JSON 狀態的時間間隔
extern const unsigned long JSON_STATUS_INTERVAL_MS; // 每 5 秒發送一次
extern unsigned long last_json_status_sent_time_ms;

extern bool isRobotReady;                       // 機器人是否已經啟動完成;
extern bool isRobotListen;                      // 機器人是否正在等待回應;
extern int currentRobotState;

extern int opus_encode_packet_count;
extern int opus_decode_packet_count;
extern int mqtt_sent_packet_count;
extern unsigned long last_stats_time;
extern int last_encoded_len;                    // 用於統計
extern int last_decoded_samples;                // 用於統計
extern long long last_frame_energy_for_debug;   // 用於調試時打印能量

extern int lightGPIO;
extern int fanGPIO;

// wifi functions
void getWifiMac();

// mqtt functions
void callback(char* topic, byte* payload, unsigned int length);   // MQTT Message received call back process.
void reconnectMQTT();   // MQTT Reconnect process;

// audio functions
void rx_irq_handler(uint32_t arg, uint8_t *pbuf);   // receive MIC audio stream handler;
void audio_tx_handler(uint32_t arg, uint8_t *pbuf);   // process speak stream handler;
void setup_audio_mic_input(); // setup audio;
void setup_opus_codec();  // setup opus coder;
void setup_vad(); // setup vad process;
bool sendListenStateToServer(char* state, char* mode, char* text);
bool sendAbortToServer(char* reason);
bool sendHelloToServer();

#endif

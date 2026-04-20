#include <Wire.h>
#include <DHT.h>
#include <esp_now.h>
#include <WiFi.h>
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
// #include "camping_guard_model.h"
#include "camping_guard_model_v2.h"
// --- [추가] OLED 및 MQ 라이브러리 ---
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MQUnifiedsensor.h>

// --- 1. 상수 및 변수 설정 ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MQ9_PIN 34
#define BUZZER_PIN 25

// OLED 설정
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MQ-9 센서 설정 (CO 가스 기준)
#define Board ("ESP-32")
#define Voltage_Resolution (3.3)
#define ADC_Bit_Resolution (12)
#define RatioMQ9CleanAir (9.6)
MQUnifiedsensor MQ9(Board, Voltage_Resolution, ADC_Bit_Resolution, MQ9_PIN, "MQ-9");

// 파이썬에서 추출한 실제 값 적용 (가스 단일 특성)
const float means[] = { 2027.71 };
const float stds[] = { 1535.26 };

const int N_STEPS = 60;
const int N_FEATURES = 1;
float input_buffer[N_STEPS * N_FEATURES] = {0,};

// TFLite 관련
const int kArenaSize = 16 * 1024;
uint8_t tensor_arena[kArenaSize];
tflite::AllOpsResolver resolver;
const tflite::Model* model;
tflite::MicroInterpreter* interpreter;
TfLiteTensor* input;
TfLiteTensor* output;

// ESP-NOW 송신용 주소
uint8_t broadcastAddress[] = {0xE8, 0x3D, 0xC1, 0x81, 0xE1, 0xBC};
typedef struct struct_message {
    float temp; float humid; int gas; int status; // 구조체의 gas는 이제 PPM 값을 담습니다.
} struct_message;
struct_message myData;
esp_now_peer_info_t peerInfo;
// 1. 송신 상태 확인용 콜백 함수 추가
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
// --- 2. 초기화 함수 ---
void setup() {


    Serial.begin(115200);
    dht.begin();
    pinMode(BUZZER_PIN, OUTPUT);

    


    // OLED 초기화
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("OLED 초기화 실패"));
    }
    display.clearDisplay();
    display.setTextColor(WHITE);
    
    // MQ-9 초기화 및 캘리브레이션 (깨끗한 공기에서 영점 잡기)
    MQ9.setRegressionMethod(1); // _PPM =  a*ratio^b
    MQ9.setA(599.65); MQ9.setB(-2.244); // 일산화탄소(CO) 공식 설정
    MQ9.init();
    
    Serial.print("MQ-9 캘리브레이션 중...");
    float calcR0 = 0;
    for(int i = 1; i <= 10; i++) {
        MQ9.update();
        calcR0 += MQ9.calibrate(RatioMQ9CleanAir);
        delay(100);
    }
    MQ9.setR0(calcR0 / 10);
    Serial.println(" 완료!");

    // TFLite 초기화
    static tflite::MicroErrorReporter micro_error_reporter;
    model = tflite::GetModel(camping_guard_model);
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kArenaSize, &micro_error_reporter, nullptr, nullptr
    );
    interpreter = &static_interpreter;
    interpreter->AllocateTensors();
    input  = interpreter->input(0);
    output = interpreter->output(0);
    
    // WiFi & ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_send_cb(OnDataSent);
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        peerInfo.channel = 0; 
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
}

// --- 3. 메인 루프 ---
void loop() {
    // 예열 타이머 (OLED 및 시리얼에 표시)
    static const unsigned long WARMUP_TIME = 60000; 
    if (millis() < WARMUP_TIME) {
        // [추가된 코드] 시리얼 모니터에도 남은 시간을 출력합니다!
        Serial.printf("예열 중... 남은 시간: %lu 초\n", (WARMUP_TIME - millis()) / 1000);
        
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 10);
        display.println("Warming Up Sensor...");
        display.setCursor(0, 30);
        display.printf("Wait: %lu sec", (WARMUP_TIME - millis()) / 1000);
        display.display();
        delay(1000);
        return; // 60초가 지나기 전까지는 알람/통신/AI 판단을 강제로 막음
    }

    // 데이터 읽기
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    // [핵심 1] AI를 위한 Raw ADC 읽기
    int gas_raw = analogRead(MQ9_PIN);
    
    // [핵심 2] 사용자를 위한 PPM 읽기
    MQ9.update();
    float gas_ppm = MQ9.readSensor(); 

    // 슬라이딩 윈도우 업데이트
    for (int i = 0; i < (N_STEPS - 1) * N_FEATURES; i++) {
        input_buffer[i] = input_buffer[i + N_FEATURES];
    }
    
    // [버그 수정 완료!] t가 아니라 gas_raw를 넣어야 합니다.
    input_buffer[(N_STEPS - 1) * N_FEATURES + 0] = (gas_raw - means[0]) / stds[0];

    // 입력 텐서에 복사
    for (int i = 0; i < N_STEPS * N_FEATURES; i++) {
        input->data.f[i] = input_buffer[i];
    }
// --- AI 추론 및 하이브리드 판단 ---
    if (interpreter->Invoke() == kTfLiteOk) {
        float prediction = output->data.f[0];
        int currentStatus = 0;

        // 1. 하드웨어적 절대 안전망 (PPM 기준 Failsafe)
        if (gas_ppm >= 400.0) {
            currentStatus = 2; // 400 PPM 이상: 구토/실신 위험 (즉시 알람)
        } 
        else if (gas_ppm >= 50.0) {
            currentStatus = 1; // 50 PPM 이상: 장시간 노출 시 두통 (주의)
            // AI가 가파른 상승을 감지했다면 상태 격상
            if (prediction > 0.8) currentStatus = 2; 
        }
        // 2. 평상시 AI 사전 예측 (Early Warning) - 데드존(Deadzone) 적용
        else {
            // [핵심 수정] 가스가 최소 15 PPM 이상 감지될 때만 AI의 예측을 신뢰합니다.
            // 15 PPM 미만은 물리적으로 완벽히 안전하므로 AI의 헛것(유령 알람)을 무시합니다.
            if (gas_ppm > 15.0) { 
                if (prediction > 0.8) currentStatus = 2;      // 위험 (가파른 상승세)
                else if (prediction > 0.5) currentStatus = 1; // 주의
                else currentStatus = 0;                       // 정상
            } else {
                currentStatus = 0; // 완벽한 안전 구역 (AI 무시)
            }
        }

        // 결과 전송 (ESP-NOW)
        myData.temp = t; 
        myData.humid = h;
        myData.gas = (int)gas_ppm; 
        myData.status = currentStatus;
        esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

        Serial.printf("T:%.1f H:%.1f PPM:%d | AI Prob: %.2f | Stat: %d\n", t, h, (int)gas_ppm, prediction, currentStatus);

        // 로컬 알람
        if (currentStatus == 2) tone(BUZZER_PIN, 2000); 
        else noTone(BUZZER_PIN);

        // --- OLED 디스플레이 출력 ---
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.println("--- CAMPING GUARD ---");
        
        display.setCursor(0, 15);
        display.printf("Temp : %.1f C", t);
        display.setCursor(0, 25);
        display.printf("Humid: %.1f %%", h);
        
        display.setCursor(0, 45);
        display.setTextSize(2);
        // 상태에 따라 출력 문구 변경
        if(currentStatus == 2)      display.printf("CO: %d PPM !", (int)gas_ppm);
        else if(currentStatus == 1) display.printf("CO: %d PPM *", (int)gas_ppm);
        else                        display.printf("CO: %d PPM", (int)gas_ppm);
        
        display.display();
    }
    delay(2000); 
}
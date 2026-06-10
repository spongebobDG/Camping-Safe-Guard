#include <esp_now.h>
#include <WiFi.h>
#include <driver/gpio.h>

#define BUZZER_PIN    7   
#define VIBRATOR_PIN  10  
#define LED_PIN       8   
#define BUTTON_PIN    3   

// RTC 메모리 변수: 필요한 경우 유지
RTC_DATA_ATTR int missCount = 0;          

typedef struct struct_message {
    float temp;
    float humid; 
    int gas; 
    int status;
} struct_message;

struct_message incomingReadings;
volatile bool isNewData = false;

// 타임아웃 및 상태 관리 변수들
unsigned long lastReceivedTime = 0;
unsigned long initialWaitTimeout = 75000; // 최초 가동 시 본체 예열(60초)을 기다려주기 위한 75초 타임아웃
bool hasConnectedOnce = false;            // 본체와 최초 연결 성공 여부 플래그

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
    isNewData = true;
}

void allStop() {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(VIBRATOR_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
}

// 위험 알람 (10초 작동) - 비상 상황이므로 이 동안은 블로킹 루프를 유지해도 무방합니다.
void dangerAlarm() {
    for (int i = 0; i < 20; i++) {
        digitalWrite(VIBRATOR_PIN, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        delay(300); 
        allStop();
        delay(200);
    }
}

// 안전하게 딥슬립으로 진입시키는 함수
void goToDeepSleep(int sleepSec) {
    allStop();
    // 딥슬립 중 핀 상태 유지 (유령 알람 방지)
    gpio_hold_en((gpio_num_t)BUZZER_PIN);
    gpio_hold_en((gpio_num_t)VIBRATOR_PIN);
    gpio_hold_en((gpio_num_t)LED_PIN);

    esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
    esp_deep_sleep_enable_gpio_wakeup(1 << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);

    Serial.printf("Going to Deep Sleep for %d sec...\n", sleepSec);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Device Awakened");
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    // 핀 고정 해제 및 초기화
    gpio_hold_dis((gpio_num_t)BUZZER_PIN);
    gpio_hold_dis((gpio_num_t)VIBRATOR_PIN);
    gpio_hold_dis((gpio_num_t)LED_PIN);
    
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(VIBRATOR_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP); 
    allStop();

    // 버튼으로 깨어났거나 스위치가 켜졌을 때 피드백 (LED 빠르게 3번 깜빡)
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println("Button Pressed! Starting Real-time Monitor...");
        for(int i=0; i<3; i++) {
            digitalWrite(LED_PIN, HIGH); delay(50);
            digitalWrite(LED_PIN, LOW); delay(50);
        }
    }

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
    }

    // 타임아웃 기준점 초기화
    lastReceivedTime = millis();
}

void loop() {
    unsigned long currentMillis = millis();

    // [1] 새로운 ESP-NOW 데이터가 수신되었을 때 (실시간 처리)
    if (isNewData) {
        isNewData = false;
        hasConnectedOnce = true;        // 본체와 연결됨을 확인
        lastReceivedTime = currentMillis; // 최신 수신 시간 갱신

        Serial.printf("[REALTIME] CO: %d PPM | Stat: %d\n", incomingReadings.gas, incomingReadings.status);

        if (incomingReadings.status == 2) {
            // 위험 상황: 즉시 강력한 경보 작동
            dangerAlarm();      
        } 
        else if (incomingReadings.status == 1) {
            // 주의 상황: 부저 짧게 3번 경고음
            for(int i=0; i<3; i++){ 
                digitalWrite(BUZZER_PIN, HIGH); delay(100); 
                digitalWrite(BUZZER_PIN, LOW); delay(50); 
            }
        } 
        else {
            // 정상 상황: 살아있다는 연결 표시로 LED만 아주 짧게(20ms) 깜빡임 (하트비트)
            digitalWrite(LED_PIN, HIGH); delay(20); digitalWrite(LED_PIN, LOW);
        }
    }

    // [2] 연결 상태 모니터링 및 타임아웃 처리
    if (!hasConnectedOnce) {
        // 최초 구동 후 아직 본체로부터 첫 패킷을 받지 못한 상태 (본체 예열 대기 중)
        if (currentMillis > initialWaitTimeout) {
            Serial.println("Main unit warmup timeout. No signal for 75s.");
            // 75초 동안 본체가 안 켜지면 꺼진 것으로 보고 5분간 딥슬립
            goToDeepSleep(300); 
        }
    } 
    else {
        // 본체와 잘 연결되어 통신하다가 갑자기 신호가 끊긴 경우 (본체 송신 주기 2초 고려)
        // 30초 동안 아무 신호도 오지 않으면 본체 전원이 꺼졌거나 통신 이탈로 판단
        if (currentMillis - lastReceivedTime > 30000) {
            Serial.println("Connection lost with main unit for 30s.");
            hasConnectedOnce = false;
            goToDeepSleep(300); // 배터리 세이브를 위해 5분간 딥슬립 후 재시도
        }
    }

    delay(10); // 루프 과열 방지용 미세 딜레이
}
#include <esp_now.h>
#include <WiFi.h>
#include <driver/gpio.h>

#define BUZZER_PIN    7   
#define VIBRATOR_PIN  10  
#define LED_PIN       8   
#define BUTTON_PIN    3   // 버튼 연결 (누르면 GND와 연결됨)

// RTC 메모리 변수: 딥슬립 중에도 값이 지워지지 않고 유지됨
RTC_DATA_ATTR int nextSleepInterval = 60; // 기본 60초
RTC_DATA_ATTR int missCount = 0;          // 통신 실패 횟수 카운트

typedef struct struct_message {
    float temp; float humid; int gas; int status;
} struct_message;

struct_message incomingReadings;
volatile bool isNewData = false;

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
    isNewData = true;
}

void allStop() {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(VIBRATOR_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
}

// 위험 알람 (10초 작동)
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

void setup() {
    Serial.begin(115200);
    while(!Serial); // 시리얼 모니터가 열릴 때까지 대기 (테스트 시 필수)
    Serial.println("Device Awakened");
    // 깨어난 원인 확인
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    // 핀 설정
    gpio_hold_dis((gpio_num_t)BUZZER_PIN);
    gpio_hold_dis((gpio_num_t)VIBRATOR_PIN);
    gpio_hold_dis((gpio_num_t)LED_PIN);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(VIBRATOR_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP); // 내부 풀업 저항 사용
    allStop();

    // 버튼으로 깨어났을 때의 피드백 (LED 빠르게 3번 깜빡)
    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println("Button Pressed! Manual Check...");
        for(int i=0; i<3; i++) {
            digitalWrite(LED_PIN, HIGH); delay(50);
            digitalWrite(LED_PIN, LOW); delay(50);
        }
        // 수동 확인 시 통신 실패 카운트 초기화
        missCount = 0; 
    }

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
    }

    // 신호 수신 대기 (최대 4초)
    unsigned long startWait = millis();
    while (millis() - startWait < 4000) { 
        if (isNewData) break;
        delay(10);
    }

    int sleepSec = nextSleepInterval; 

    if (isNewData) {
        // 통신 성공 시 카운트 초기화
        missCount = 0;
        
        // V4 본체에 맞춰 출력문 수정 (gas 변수에는 이제 PPM 값이 들어있음)
        Serial.printf("Received CO: %d PPM | Stat: %d\n", incomingReadings.gas, incomingReadings.status);

        if (incomingReadings.status == 2) {
            dangerAlarm();      
            sleepSec = 2;       // 위험 시 2초 뒤 즉시 다시 확인
        } else if (incomingReadings.status == 1) {
            // 주의 시 부저 짧게 3번
            for(int i=0; i<3; i++){ 
                digitalWrite(BUZZER_PIN, HIGH); delay(200); 
                digitalWrite(BUZZER_PIN, LOW); delay(100); 
            }
            sleepSec = 10;      // 주의 시 10초 뒤 확인
        } else {
            // 정상: LED만 길게 1번 (확인용)
            digitalWrite(LED_PIN, HIGH); delay(500); digitalWrite(LED_PIN, LOW);
            sleepSec = 60;      // 정상일 땐 1분 수면
        }
    } else {
        missCount++; // 실패 횟수 증가
        Serial.printf("No Signal... Miss Count: %d\n", missCount);
        
        // 신호를 못 잡았을 때 LED를 아주 짧게 2번 깜빡여서 "신호없음" 표시
        digitalWrite(LED_PIN, HIGH); delay(50); digitalWrite(LED_PIN, LOW); delay(100);
        digitalWrite(LED_PIN, HIGH); delay(50); digitalWrite(LED_PIN, LOW);
        
        if (missCount >= 3) {
            Serial.println("Main unit seems offline. Going to long sleep.");
            sleepSec = 2592000; // 3번 연속 실패 시 캠핑 종료로 간주하고 30일 수면
        } else {
            sleepSec = 10; // 전파 방해일 수 있으므로 10초 뒤 재시도
        }
    }

    allStop();
    // 딥슬립 중 핀 상태 유지 (유령 알람 방지)
    gpio_hold_en((gpio_num_t)BUZZER_PIN);
    gpio_hold_en((gpio_num_t)VIBRATOR_PIN);
    gpio_hold_en((gpio_num_t)LED_PIN);

    // --- 딥슬립 설정 ---
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
    
    // 버튼(GPIO 3)이 LOW가 되면 즉시 깨어나도록 설정
    esp_deep_sleep_enable_gpio_wakeup(1 << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);

    Serial.printf("Sleeping for %d sec\n", sleepSec);
    esp_deep_sleep_start();
}

void loop() {}
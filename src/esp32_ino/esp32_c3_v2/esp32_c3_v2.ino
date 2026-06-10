#include <esp_now.h>
#include <WiFi.h>
#include <driver/gpio.h>

#define BUZZER_PIN    7   
#define VIBRATOR_PIN  10  
#define LED_PIN       8   
#define BUTTON_PIN    3   // 버튼 연결 (누르면 GND와 연결됨)

RTC_DATA_ATTR int nextSleepInterval = 60; // 기본 60초 (배터리 절약용)

typedef struct struct_message {
    float temp; float humid; int gas; int status;
} struct_message;

struct_message incomingReadings;
volatile bool isNewData = false;

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
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
    }

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
    }

    // 신호 수신 대기 (4초)
    unsigned long startWait = millis();
    while (millis() - startWait < 4000) { 
        if (isNewData) break;
        delay(10);
    }

    int sleepSec = nextSleepInterval; 

    if (isNewData) {
        Serial.printf("Received Gas: %d, Stat: %d\n", incomingReadings.gas, incomingReadings.status);

        if (incomingReadings.status == 2) {
            dangerAlarm();      
            sleepSec = 2;       // 위험 시 자주 확인
        } else if (incomingReadings.status == 1) {
            // 주의 시 부저 짧게 3번
            for(int i=0; i<3; i++){ digitalWrite(BUZZER_PIN, HIGH); delay(200); digitalWrite(BUZZER_PIN, LOW); delay(100); }
            sleepSec = 10;
        } else {
            // 정상: LED만 길게 1번 (확인용)
            digitalWrite(LED_PIN, HIGH); delay(500); digitalWrite(LED_PIN, LOW);
            sleepSec = 60;      // 정상일 땐 1분 수면
        }
    } else {
        Serial.println("No Signal...");
        // 신호를 못 잡았을 때 LED를 아주 짧게 2번 깜빡여서 "신호없음" 표시
        digitalWrite(LED_PIN, HIGH); delay(50); digitalWrite(LED_PIN, LOW); delay(100);
        digitalWrite(LED_PIN, HIGH); delay(50); digitalWrite(LED_PIN, LOW);
        sleepSec = 5; 
    }

    allStop();
    gpio_hold_en((gpio_num_t)BUZZER_PIN);
    gpio_hold_en((gpio_num_t)VIBRATOR_PIN);

    // --- 딥슬립 설정 ---
    // 1. 타이머 깨우기 (주기적 검사)
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
    
    // 2. 버튼(GPIO) 깨우기 설정 (버튼을 누르면 즉시 깨어남)
    // GPIO 2번 핀이 LOW(누름)가 되면 깨어나도록 설정
    esp_deep_sleep_enable_gpio_wakeup(1 << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);

    Serial.printf("Sleeping for %d sec\n", sleepSec);
    esp_deep_sleep_start();
}

void loop() {}
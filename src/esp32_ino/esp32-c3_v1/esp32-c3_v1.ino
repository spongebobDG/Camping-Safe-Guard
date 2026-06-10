#include <esp_now.h>
#include <WiFi.h>
#include <driver/gpio.h>

#define BUZZER_PIN    7   
#define VIBRATOR_PIN  10  
#define LED_PIN       8   

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

// 위험 알람: 10초 동안 반복 (더 길게 수정)
void dangerAlarm() {
    Serial.println("!!! DANGER ALARM START !!!");
    for (int i = 0; i < 20; i++) { // 0.5초 패턴을 20번 = 10초간 작동
        digitalWrite(VIBRATOR_PIN, HIGH);
        digitalWrite(BUZZER_PIN, HIGH); // 능동 부저 HIGH
        digitalWrite(LED_PIN, HIGH);
        delay(300); 
        
        digitalWrite(VIBRATOR_PIN, LOW);
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        delay(200);
    }
}

// 주의 알람: 3초 동안 반복
void warningAlarm() {
    Serial.println("Warning Alarm...");
    for (int i = 0; i < 5; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(400);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
    }
}

void setup() {
    Serial.begin(115200);
    // USB 직렬 포트 안정화 대기
    delay(2000); 

    gpio_hold_dis((gpio_num_t)BUZZER_PIN);
    gpio_hold_dis((gpio_num_t)VIBRATOR_PIN);
    gpio_hold_dis((gpio_num_t)LED_PIN);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(VIBRATOR_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    allStop();

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
    }

    // --- 신호 수신 대기 시간을 4초로 확장 (송신 주기와 맞춤) ---
    unsigned long startWait = millis();
    while (millis() - startWait < 4000) { 
        if (isNewData) break;
        delay(10);
    }

    int sleepSec = 10; // 기본 잠자기 시간

    if (isNewData) {
        Serial.printf("Received Gas: %d, Status: %d\n", incomingReadings.gas, incomingReadings.status);

        if (incomingReadings.status == 2) {
            dangerAlarm();      // 10초간 알람
            sleepSec = 1;       // 위험할 땐 거의 바로 깨서 다시 확인
        } else if (incomingReadings.status == 1) {
            warningAlarm();     // 주의 알람
            sleepSec = 5;
        } else {
            // 정상: LED만 깜빡
            digitalWrite(LED_PIN, HIGH); delay(100); digitalWrite(LED_PIN, LOW);
            sleepSec = 15;
        }
    } else {
        Serial.println("No Signal... (Searching)");
        sleepSec = 2; // 신호 못 잡으면 금방 다시 깨서 찾기
    }

    allStop();
    // 딥슬립 중 핀 상태 LOW로 고정
    gpio_hold_en((gpio_num_t)BUZZER_PIN);
    gpio_hold_en((gpio_num_t)VIBRATOR_PIN);

    Serial.printf("Next Check in %d sec\n", sleepSec);
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {}
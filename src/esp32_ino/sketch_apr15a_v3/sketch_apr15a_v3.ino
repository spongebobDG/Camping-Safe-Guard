#include <esp_now.h>
#include <WiFi.h>
#include <driver/gpio.h>  // ★ gpio_hold 함수용

#define BUZZER_PIN    5
#define VIBRATOR_PIN  4
#define LED_PIN       6

RTC_DATA_ATTR int gasHistory[3] = {0, 0, 0};
RTC_DATA_ATTR int nextSleepInterval = 10;

typedef struct struct_message {
    float temp; float humid; int gas; int status;
} struct_message;

struct_message incomingReadings;
volatile bool isNewData = false;

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  isNewData = true;
}

void dangerAlarm() {
  for (int repeat = 0; repeat < 4; repeat++) {
    digitalWrite(VIBRATOR_PIN, HIGH);
    for (int f = 2700; f <= 3200; f += 50) { tone(BUZZER_PIN, f); delay(15); }
    for (int f = 3200; f >= 2700; f -= 50) { tone(BUZZER_PIN, f); delay(15); }
    digitalWrite(VIBRATOR_PIN, LOW);
    noTone(BUZZER_PIN);
    delay(80);
  }
}

void warningAlarm() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 3000); delay(500);
    noTone(BUZZER_PIN);     delay(250);
  }
}

void setup() {

    // 슬립 전 확실히 끄기
  digitalWrite(VIBRATOR_PIN, LOW);
  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN, LOW);

  
  // ★ 1순위: hold 해제 → 이전 슬립에서 잠긴 핀을 먼저 풀어야 제어 가능
  gpio_hold_dis((gpio_num_t)VIBRATOR_PIN);
  gpio_hold_dis((gpio_num_t)LED_PIN);

  // ★ 즉시 LOW 고정
  gpio_reset_pin((gpio_num_t)VIBRATOR_PIN);
  gpio_set_direction((gpio_num_t)VIBRATOR_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)VIBRATOR_PIN, 0);
  gpio_pulldown_en((gpio_num_t)VIBRATOR_PIN);

  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATOR_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(VIBRATOR_PIN, LOW);
  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
  }

  unsigned long startWait = millis();
  while (millis() - startWait < 2500) {
    if (isNewData) break;
    delay(10);
  }

  if (isNewData) {
    int receivedStatus = incomingReadings.status;

    gasHistory[2] = gasHistory[1];
    gasHistory[1] = gasHistory[0];
    gasHistory[0] = incomingReadings.gas;

    Serial.printf("Raw: %d, Status(from main): %d\n",
                  incomingReadings.gas, receivedStatus);

    if (receivedStatus == 2) {
      Serial.println("LEVEL 2: DANGER");
      digitalWrite(LED_PIN, HIGH);
      nextSleepInterval = 3;
      dangerAlarm();
      digitalWrite(LED_PIN, LOW);
    }
    else if (receivedStatus == 1) {
      Serial.println("LEVEL 1: WARNING");
      nextSleepInterval = 7;
      warningAlarm();
    }
    else {
      Serial.println("LEVEL 0: SAFE");
      nextSleepInterval = 15;
    }
  } else {
    Serial.println("No Signal. Sleep...");
    nextSleepInterval = 10;
  }

  // 슬립 전 확실히 끄기
  digitalWrite(VIBRATOR_PIN, LOW);
  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN, LOW);

  // ★ LOW 상태 그대로 잠그기 → 슬립 중 + 다음 부팅 첫 순간까지 LOW 유지
  gpio_hold_en((gpio_num_t)VIBRATOR_PIN);
  gpio_hold_en((gpio_num_t)LED_PIN);

  Serial.printf("Deep Sleep: %d sec\n", nextSleepInterval);
  esp_sleep_enable_timer_wakeup((uint64_t)nextSleepInterval * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {}

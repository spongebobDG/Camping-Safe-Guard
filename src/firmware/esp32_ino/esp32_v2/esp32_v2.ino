#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <esp_now.h>
#include <WiFi.h>

// 수신측 MAC 주소 (ESP32-C3 목걸이 주소)
uint8_t broadcastAddress[] = {0xE0, 0x72, 0xA1, 0x6D, 0x97, 0x88};

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define MQ9_PIN 34     
#define BUZZER_PIN 25  

typedef struct struct_message {
    float temp;
    float humid;
    int gas;
    int status;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

const int SAMPLES = 50; 

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED 초기화 실패"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(WHITE); 
  display.display(); 
  
  dht.begin();
  pinMode(MQ9_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  
  Serial.println("System Ready!");
}

void loop() {
  // --- 1. 데이터 수집 (Input) ---
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  long adc_sum = 0;
  for(int i = 0; i < SAMPLES; i++) {
    adc_sum += analogRead(MQ9_PIN);
    delay(2);
  }
  int currentGas = adc_sum / SAMPLES;

  // --- 2. 상태 판별 및 데이터 패키징 (Process) ---
  int currentStatus = 0;
  if (currentGas > 800) currentStatus = 2;      // 위험
  else if (currentGas > 600) currentStatus = 1;  // 주의
  else currentStatus = 0;                        // 정상

  // [수정 사항] 송신 전 struct에 데이터 먼저 채우기
  myData.temp = t;
  myData.humid = h;
  myData.gas = currentGas;
  myData.status = currentStatus;

  // --- 3. 데이터 송신 (Output - Burst Send) ---
  Serial.print("Sending Burst... Stat: "); Serial.println(currentStatus);
  for(int i = 0; i < 5; i++) {
    // 이제 데이터가 채워진 myData를 보냅니다.
    esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    delay(100); 
  }

  // --- 4. 알람 및 시각화 ---
  // 본체 부저 알람
  if (currentStatus == 2) {
    tone(BUZZER_PIN, 2000); 
  } else {
    noTone(BUZZER_PIN);
  }

  // 시리얼 출력
  Serial.printf("T: %.1f, H: %.1f, Gas: %d, Stat: %d\n", t, h, currentGas, currentStatus);

  // OLED 출력
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
  display.printf("CO:%d", currentGas);
  
  display.display(); 

  // (기존 하단의 중복 esp_now_send 제거 완료)

  delay(1500); 
}
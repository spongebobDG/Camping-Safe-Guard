#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <MHZ19.h>
// #include <DHT.h> // 센서 복구 시 주석 해제

// // --- BLE 라이브러리 추가 ---
// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>
// #include <BLE2902.h>

// --- 핀 정의 ---
#define PIN_MQ9        34
#define PIN_BUZZER     19
#define PIN_SERVO      18
#define RX2            16
#define TX2            17

// --- 객체 설정 ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Servo myServo;
MHZ19 myMHZ19;
HardwareSerial mySerial(2);

// --- 변수 및 임계값 ---
int mq9_baseline = 0;
int threshold_offset = 800; 
bool is_danger = false;
bool is_muted = false;
unsigned long muteStartTime = 0;

unsigned long previousMillis = 0;
const long interval = 1000; // 1초마다 BLE 데이터 전송

// // --- BLE 설정 ---
// BLEServer* pServer = NULL;
// BLECharacteristic* pTxCharacteristic = NULL;
// bool deviceConnected = false;

// #define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// #define CHARACTERISTIC_TX_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
// #define CHARACTERISTIC_RX_UUID "8a798533-c917-48f5-a7b6-3c224f8d689b"

// // BLE 연결 콜백
// class MyServerCallbacks: public BLEServerCallbacks {
//     void onConnect(BLEServer* pServer) { deviceConnected = true; }
//     void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
// };

// // 스마트폰에서 보낸 명령어(OPEN, MUTE) 수신 콜백
// class MyCallbacks: public BLECharacteristicCallbacks {
//     void onWrite(BLECharacteristic *pCharacteristic) {
//       String rxValue = pCharacteristic->getValue().c_str();
//       if (rxValue.length() > 0) {
//         Serial.print("Received Value: "); Serial.println(rxValue);
        
//         if (rxValue == "OPEN") {
//           // 스마트폰에서 수동 지퍼 개방 명령을 보냈을 때
//           myServo.write(180);
//           delay(3000);
//           myServo.write(90);
//         } 
//         else if (rxValue == "MUTE") {
//           // 5분간 음소거 모드 진입
//           is_muted = true;
//           muteStartTime = millis();
//           noTone(PIN_BUZZER);
//         }
//       }
//     }
// };

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, RX2, TX2); 
  myMHZ19.begin(mySerial);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  display.clearDisplay();
  display.setTextColor(WHITE);

  myServo.attach(PIN_SERVO);
  myServo.write(90); 
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  // // --- BLE 서버 생성 ---
  // BLEDevice::init("CampingSafeGuard"); // 블루투스 이름
  // pServer = BLEDevice::createServer();
  // pServer->setCallbacks(new MyServerCallbacks());

  // BLEService *pService = pServer->createService(SERVICE_UUID);

  // pTxCharacteristic = pService->createCharacteristic(
  //                       CHARACTERISTIC_TX_UUID,
  //                       BLECharacteristic::PROPERTY_NOTIFY
  //                     );
  // pTxCharacteristic->addDescriptor(new BLE2902());

  // BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
  //                                          CHARACTERISTIC_RX_UUID,
  //                                          BLECharacteristic::PROPERTY_WRITE
  //                                        );
  // pRxCharacteristic->setCallbacks(new MyCallbacks());

  // pService->start();
  // pServer->getAdvertising()->start();
  // Serial.println("Waiting for a client connection to notify...");

  // 캘리브레이션
  display.setCursor(0,0); display.println("Calibrating..."); display.display();
  long sum = 0;
  for(int i=0; i<50; i++) { sum += analogRead(PIN_MQ9); delay(200); }
  mq9_baseline = sum / 50; 
}

void loop() {
  int raw_mq9 = analogRead(PIN_MQ9);
  int co2 = 0; // myMHZ19.getCO2();
  float temp = 0.0; // dht.readTemperature();
  float hum = 0.0;  // dht.readHumidity();

  // MUTE 타이머 해제 (5분 = 300,000ms)
  if (is_muted && (millis() - muteStartTime > 300000)) {
    is_muted = false;
  }

  // 위험 판단
  if (raw_mq9 > (mq9_baseline + threshold_offset)) {
    is_danger = true;
    if (!is_muted) tone(PIN_BUZZER, 1000); 
  } else {
    is_danger = false;
    noTone(PIN_BUZZER);
  }

  // 1초마다 BLE 데이터 전송 및 OLED 갱신
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    // BLE로 스마트폰에 보낼 데이터 조합 (포맷: MQ9,CO2,TEMP,HUMI,STATE,BASELINE)
    String stateStr = is_danger ? "DANGER" : "SAFE";
    String bleMessage = String(raw_mq9) + "," + String(co2) + "," + 
                        String(temp, 1) + "," + String(hum, 1) + "," + 
                        stateStr + "," + String(mq9_baseline);

    // if (deviceConnected) {
    //     pTxCharacteristic->setValue(bleMessage.c_str());
    //     pTxCharacteristic->notify(); // 스마트폰으로 발사!
    // }

    updateDisplay(raw_mq9, co2, temp, hum);
  }
}

void updateDisplay(int mq9, int co2, float t, float h) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  
  display.print("GAS: "); display.println(mq9);
  display.print("BLE: "); display.println(deviceConnected ? "ON" : "OFF");
  
  if(is_danger) {
    display.setCursor(0, 40);
    display.setTextSize(2);
    display.println("! DANGER !");
  }
  display.display();
}
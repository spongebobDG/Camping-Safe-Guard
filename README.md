# Camping Safe Guard

ESP32·가스·온습도 센서와 TinyML을 이용해 캠핑 환경의 일산화탄소 위험을 감지하고 사용자에게 전달하는 캡스톤 프로젝트입니다.

> An embedded safety prototype that combines ESP32 sensor nodes, MQ-9 gas sensing, wireless telemetry, and TensorFlow Lite Micro experiments.

## 프로젝트 요약

| 구분 | 내용 |
|---|---|
| 목적 | 캠핑 공간의 CO·온습도 상태 감지와 위험 단계 전달 |
| MCU | ESP32, ESP32-C3 |
| 센서 | MQ-9, DHT11 |
| 통신 실험 | ESP-NOW, BLE |
| Edge AI | TensorFlow Lite Micro 기반 시계열 위험도 추론 실험 |
| 기타 | OLED 표시, 모바일 HTML prototype, 3D printed enclosure, 배터리 승압 효율 자료 |

## 시스템 구조

```text
MQ-9 / DHT11
      │
      ▼
ESP32 sensor node ── calibration / TinyML inference ── status decision
      │                                                   │
      ├── OLED local display                              │
      └── ESP-NOW / BLE ──> ESP32-C3 receiver / mobile prototype
```

## 저장소에서 확인할 수 있는 구현

- MQ-9 clean-air calibration과 CO PPM 변환
- 온도·습도·가스 측정값을 결합한 상태 데이터
- sliding window 입력과 TensorFlow Lite Micro inference
- 정상·주의·위험 단계 판단 및 OLED 표시
- ESP-NOW 기반 센서 노드–수신 노드 통신 실험
- ESP32/ESP32-C3 케이스 STL과 18650 배터리 승압 효율 자료

가장 발전된 TinyML 실험 firmware는 [`src/esp32_ino/esp32_v4_ai/esp32_v4_ai.ino`](src/esp32_ino/esp32_v4_ai/esp32_v4_ai.ino)에 있습니다. 저장소에는 개발 과정의 여러 hardware revision이 함께 남아 있으므로 모든 `.ino` 파일을 최종 firmware로 해석하면 안 됩니다.

## TinyML 경로

```text
MQ-9 raw sequence
    └── normalization
        └── 20-step input buffer
            └── TFLite Micro inference
                └── probability + calibrated PPM safety rule
```

현재 저장소에는 모델과 학습 CSV가 포함되어 있지만, 학습 notebook·분할 방법·정확도·MCU 추론 시간·메모리 사용량을 한 번에 재현하는 평가 보고서는 없습니다. 따라서 수치 성능을 추정해 제시하지 않습니다.

## 학술 성과

- 캡스톤 프로젝트
- KCC 학술대회 논문 심사 통과
- KCC poster session 발표

논문 제목·전체 저자·발표 연도·공식 프로그램 링크는 원본 발표 자료와 공식 기록을 확인한 뒤 추가합니다. 배포 권한이 확인되지 않은 학회 PDF는 저장소에 복제하지 않습니다.

## 디렉터리 안내

```text
Camping-Safe-Guard/
├── src/esp32_ino/        # ESP32/ESP32-C3 firmware revisions, model, training CSV
├── src/firmware/         # 초기 정리 과정에서 복제된 firmware tree
├── src/mobile-app/       # mobile display prototype
├── hardware/3d-models/   # enclosure STL revisions
└── docs/                 # battery boost efficiency data
```

`src/esp32_ino/`와 `src/firmware/esp32_ino/`의 중복은 후속 정리 대상입니다. 면접에서는 기준 firmware 경로와 revision별 목적을 먼저 설명합니다.

## 현재 한계

- 기준 hardware revision과 final firmware가 release로 고정되어 있지 않습니다.
- 센서 교정 환경과 실제 CO 기준 장비를 사용한 비교 시험이 문서화되지 않았습니다.
- 모델 평가와 MCU resource benchmark가 재현 가능한 형태로 정리되지 않았습니다.
- 모바일 화면은 제품 앱이 아니라 prototype입니다.

## 안전 고지

이 프로젝트는 교육·연구용 prototype입니다. 인증된 일산화탄소 경보기, 환기 설비 또는 상용 안전 장치를 대체하지 않습니다.

## 면접용 요약

- **30초:** “가스·온습도 센서부터 ESP32 edge inference, 무선 전달, 케이스 제작까지 연결한 캡스톤이며 KCC poster session에서 발표했습니다.”
- **기술 질문:** 센서 calibration, raw ADC와 PPM의 차이, TinyML 입력 정규화, 통신 장애 시 로컬 경보 유지 여부를 중심으로 설명합니다.

## Related Portfolio

[ROS 2 Robot Systems Software Portfolio](https://github.com/spongebobDG/robotics-software-portfolio)

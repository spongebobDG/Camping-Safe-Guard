# Camping Safe Guard

ESP32·가스·온습도 센서와 TinyML을 이용해 캠핑 환경의 일산화탄소 위험을 감지하고 사용자에게 전달하는 캡스톤 프로젝트입니다.

> An embedded safety prototype that combines ESP32 sensor nodes, MQ-9 gas sensing, wireless telemetry, and TensorFlow Lite Micro experiments.

| 구분 | 내용 |
|---|---|
| 기간 | 2025.09–2025.12 |
| 구성 | 4인 개발팀 · 팀장 |
| 담당 | 시스템 설계, 모델 경량화, ESP32 펌웨어, ESP-NOW, 회로·3D 프린팅 케이스 제작 |
| 학술 성과 | KCC 2026 논문 심사 통과·포스터 세션 발표, 제1저자 |
| 논문 | 「제한된 엣지 환경에서의 일산화탄소 조기 경보를 위한 경량 1D-CNN 기반 웨어러블 시스템 설계」 |

## 프로젝트 요약

| 구분 | 내용 |
|---|---|
| 목적 | 캠핑 공간의 CO·온습도 상태 감지와 위험 단계 전달 |
| MCU | ESP32, ESP32-C3 |
| 센서 | MQ-9, DHT11 |
| 통신 | ESP-NOW 중심, BLE·모바일 UI prototype 실험 |
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
        └── 60-step input buffer
            └── TFLite Micro inference
                └── probability + calibrated PPM safety rule
```

기준 firmware의 `N_STEPS = 60`이며, 2초 주기 샘플을 가정하면 최근 120초의 흐름을 입력으로 사용합니다.

## MCU 메모리 문제와 모델 전환

초기 LSTM 모델은 약 174KB로, 센서·ESP-NOW·OLED가 함께 동작하는 ESP32 환경에서 텐서 메모리 할당 문제가 발생했습니다. 캠핑 사고의 전조에서 중요한 것은 장기 상태보다 단시간의 농도 상승 패턴이라고 판단해 순환 구조를 1D-CNN으로 바꿨습니다. KCC 논문과 지원 포트폴리오에는 약 10KB까지 줄인 결과를 보고했습니다.

공개 저장소에는 개발 과정의 174KB 모델과 3–4KB대 실험 artifact 등 여러 revision이 함께 있습니다. 논문의 174KB→약 10KB 비교를 한 번에 재현하는 학습 notebook과 release는 아직 고정되어 있지 않으므로, 이 값은 **논문 보고 결과**로 표시하며 저장소 benchmark처럼 표현하지 않습니다.

## 하이브리드 안전 판단

| CO 구간 | 판단 방식 |
|---|---|
| 15 PPM 미만 | 센서 잡음 구간으로 보고 AI 결과를 사용하지 않음 |
| 15–50 PPM | AI 확률 0.5 초과 시 주의, 0.8 초과 시 위험 |
| 50–400 PPM | 규칙이 주의를 기본 보장하고 AI가 위험으로 격상 |
| 400 PPM 이상 | AI 결과와 무관하게 위험 상태 강제 |

합성 데이터 기반 HIL 급성 누출 시나리오에서는 절대 안전망 도달보다 약 8.5초 먼저 최고 경보가 발생했습니다. 이 값은 실제 CO 환경이나 인증 시험에서 얻은 성능이 아닙니다.

## 학술 성과

- 캡스톤 프로젝트
- 한국컴퓨터종합학술대회(KCC 2026) 논문 심사 통과
- KCC 2026 포스터 세션 발표
- 4인 개발팀 팀장·제1저자, 멘토·지도교수 포함 총 6인 저자

논문 제목은 「제한된 엣지 환경에서의 일산화탄소 조기 경보를 위한 경량 1D-CNN 기반 웨어러블 시스템 설계」입니다. 수상 실적은 없으며, 배포 권한이 확인되지 않은 학회 PDF는 저장소에 복제하지 않습니다.

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
- 약 8.5초 조기 경보는 합성 데이터 기반 HIL 결과이며 실환경 인증 성능이 아닙니다.
- 모바일 화면은 제품 앱이 아니라 prototype입니다.

## 안전 고지

이 프로젝트는 교육·연구용 prototype입니다. 인증된 일산화탄소 경보기, 환기 설비 또는 상용 안전 장치를 대체하지 않습니다.

## 면접용 요약

- **30초:** “4인 팀장으로 가스·온습도 센서부터 ESP32 edge inference, ESP-NOW, 회로와 케이스 제작까지 연결했고, 제1저자로 KCC 2026 포스터 세션에서 발표했습니다.”
- **기술 질문:** 센서 calibration, raw ADC와 PPM의 차이, TinyML 입력 정규화, 통신 장애 시 로컬 경보 유지 여부를 중심으로 설명합니다.

## Related Portfolio

[ROS 2 Robot Systems Software Portfolio](https://github.com/spongebobDG/robotics-software-portfolio)

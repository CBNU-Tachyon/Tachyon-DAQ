# Tachyon DAQ

## 개요
Tachyon DAQ는 ESP32 기반의 Formula Student 차량용 고속 데이터 로깅 및 실시간 텔레메트리 시스템입니다. BMI088 IMU 센서를 활용하여 차량의 가속도, 자이로스코프, 온도 데이터를 수집하고, CAN 버스 데이터를 통합하여 SD 카드에 로깅하고 LTE Cat.M1을 통해 MQTT로 전송합니다. FreeRTOS를 사용하여 멀티코어 아키텍처를 구현하여 실시간성과 I/O 효율성을 최적화했습니다.

## 기능
- **센서 데이터 수집**: BMI088 IMU를 통해 가속도, 자이로, 온도 데이터를 실시간으로 수집 (50Hz).
- **CAN 인터페이스**: 차량 데이터 (RPM, TPS, 배터리 전압 등)를 CAN 버스를 통해 수신 (현재 스텁 구현).
- **GPS 통합**: 위치 데이터 수집 (현재 가상 데이터).
- **데이터 로깅**: SD 카드에 CSV 형식으로 데이터를 저장 (20Hz).
- **텔레메트리 전송**: LTE Cat.M1 모듈을 통해 MQTT로 JSON 데이터를 전송 (10Hz).
- **멀티태스크 아키텍처**: FreeRTOS를 사용하여 코어 1에서 센서 수집, 코어 0에서 I/O 작업을 분리.

## 아키텍처
- **코어 1 (CORE_SENSING)**: SensorTask - 고우선순위 센서 데이터 수집 및 CAN 데이터 처리.
- **코어 0 (CORE_IO)**: LoggerTask - SD 카드 로깅, MqttTask - LTE MQTT 전송.
- **데이터 구조**: VehicleData 구조체를 사용하여 timestamp, RPM, TPS, 배터리 전압, GPS, 가속도, 자이로, 온도를 통합 관리.
- **통신**: SPI를 사용하여 BMI088과 연결 (Accel CS: 핀 5, Gyro CS: 핀 18).

## 하드웨어 요구사항
- ESP32-S3 DevKitC-1 (또는 호환 보드)
- BMI088 IMU 센서 (SPI 인터페이스)
- SD 카드 모듈
- LTE Cat.M1 모듈 (TSCM-LM BG770A-GL, 현재 주석 처리)
- CAN 트랜시버 (SN65HVD230, 현재 스텁)

## 소프트웨어 요구사항
- PlatformIO IDE
- Arduino Framework
- FreeRTOS
- 라이브러리: BMI088, ArduinoJson, SD, SPI

## 설치 및 실행
1. PlatformIO를 설치하고 프로젝트를 엽니다.
2. `platformio.ini` 파일을 확인하여 환경 설정을 맞춥니다.
3. 하드웨어를 연결하고 핀 설정을 확인합니다.
4. `pio run`으로 빌드하고 `pio run --target upload`로 업로드합니다.
5. 시리얼 모니터에서 로그를 확인합니다.

## 데이터 구조
```cpp
struct VehicleData {
    uint32_t timestamp;  // 밀리초 타임스탬프
    uint16_t rpm;        // 엔진 RPM
    uint8_t tps;         // 스로틀 포지션 (%)
    float v_batt;        // 배터리 전압 (V)
    float temp;          // 온도 (°C)
    struct { float lat, lng; } gps;  // GPS 좌표
    struct { float x, y, z; } acc;   // 가속도 (m/s²)
    struct { float x, y, z; } gyro;  // 자이로 (rad/s)
};
```

## 기여
기여를 환영합니다! 저장소를 포크하고 풀 리퀘스트를 제출해 주세요.

## 라이선스
이 프로젝트는 MIT 라이선스 하에 라이선스가 부여됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.
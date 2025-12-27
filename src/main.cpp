#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <FreeRTOS.h>
#include "BMI088.h"

/** * [Architecture Configuration]
 * Core 1: High-priority Sensor Data Acquisition (Real-time)
 * Core 0: I/O Intensive Tasks (SD Card, LTE Cat.M1 Comm)
 */
#define CORE_SENSING 1
#define CORE_IO 0
#define AccelCS 5
#define GyroCS 18

// MQTT Topic Design: formula/[car_id]/[session_id]/telemetry
const char *mqtt_topic = "formula/TF-26/session_01/telemetry";

// Data Structure: CSV와 JSON 필드 구성을 동일하게 유지하여 일관성 확보
struct VehicleData
{
    uint32_t timestamp;
    uint16_t rpm;
    uint8_t tps;
    float v_batt, temp;
    struct
    {
        float lat;
        float lng;
    } gps;
    struct
    {
        float x;
        float y;
        float z;
    } acc;
    struct
    {
        float x;
        float y;
        float z;
    } gyro;
};

// RTOS Queues for Inter-task communication
QueueHandle_t loggingQueue;
QueueHandle_t telemetryQueue;

// BMI088 IMU 센서 인스턴스
Bmi088 imu(SPI, AccelCS, GyroCS); // SPI, Accel/Gyro CS 핀 설정

// --- [CAN Bus Interface Placeholder] ---
// 향후 DBC 파싱 및 실제 CAN 구현 시 이 부분을 확장함
void can_init()
{
    Serial.println("[CAN] Initializing SN65HVD230...");
}

void can_receive_stub(VehicleData &data)
{
    // TODO: 실제 CAN 프레임 수신 및 DBC 기반 스케일링 로직 구현
    data.rpm = random(1000, 12000);
    data.tps = random(0, 100);
}

// --- [LTE Cat.M1 MQTT Interface Placeholder] --- [cite: 41, 612]
void mqtt_publish_json(const VehicleData &data)
{
    StaticJsonDocument<256> doc;
    doc["ts"] = data.timestamp;
    doc["rpm"] = data.rpm;
    doc["tps"] = data.tps;
    doc["v_bat"] = data.v_batt;
    doc["lat"] = data.gps.lat;
    doc["lng"] = data.gps.lng;

    String jsonOutput;
    serializeJson(doc, jsonOutput);

    // TSCM-LM(BG770A-GL) AT 커맨드 시퀀스 실행 [cite: 614, 622]
    // Serial2.print("AT+QMTPUB=0,0,0,0,\"");
    // Serial2.print(mqtt_topic);
    // Serial2.println("\"");
    // Wait for '>'... then Serial2.print(jsonOutput); Serial2.write(0x1A);
}

// --- [FreeRTOS Tasks] ---

/** * SensorTask (Core 1): 최우선 순위 데이터 수집
 * CAN 데이터 및 내장 GNSS/IMU 데이터를 주기적으로 읽음 [cite: 474, 475]
 */
void SensorTask(void *pvParameters)
{
    can_init();
    VehicleData currentData;

    for (;;)
    {
        currentData.timestamp = millis();
        can_receive_stub(currentData);

        // 가상 센서 데이터 업데이트
        currentData.v_batt = 12.6; // [cite: 104, 703]
        currentData.gps = {37.1234, 127.1234};
        
        imu.readSensor();
        currentData.acc={imu.getAccelX_mss(), imu.getAccelY_mss(), imu.getAccelZ_mss()};
        currentData.gyro={imu.getGyroX_rads(), imu.getGyroY_rads(), imu.getGyroZ_rads()};
        currentData.temp=imu.getTemperature_C();

        // 로거 및 텔레메트리 큐로 데이터 복사 (Non-blocking)
        xQueueSend(loggingQueue, &currentData, 0);
        xQueueSend(telemetryQueue, &currentData, 0);

        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz 수집
    }
}

/** * LoggerTask (Core 0): SD 카드 CSV 저장
 * I/O 병목이 발생해도 센서 수집에 영향을 주지 않음
 */
void LoggerTask(void *pvParameters)
{
    File logFile;
    if (SD.begin())
    {
        logFile = SD.open("/log_01.csv", FILE_WRITE);
        // Header: timestamp,rpm,tps,v_batt,lat,lng,acc_x,acc_y,acc_z
    }
    else
    {
        Serial.println("[SD] Initialization failed!");
        // 경고 led 점등 등 추가 조치 가능
        vTaskDelete(NULL); // 태스크 종료
    }

    VehicleData dataToLog;
    for (;;)
    {
        if (xQueueReceive(loggingQueue, &dataToLog, portMAX_DELAY))
        {
            if (logFile)
            {
                logFile.printf("%u,%.1f,%.1f,%.2f,%.6f,%.6f,%.2f,%.2f,%.2f\n",
                               dataToLog.timestamp, dataToLog.rpm, dataToLog.tps, dataToLog.v_batt,
                               dataToLog.gps.lat, dataToLog.gps.lng, dataToLog.acc.x, dataToLog.acc.y, dataToLog.acc.z);
                logFile.flush(); // 안정적인 저장을 위해 flush 호출
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz 기록
    }
}

/** * MqttTask (Core 0): LTE Cat.M1을 통한 데이터 전송 [cite: 35, 39]
 */
void MqttTask(void *pvParameters)
{
    VehicleData dataToPub;
    for (;;)
    {
        if (xQueueReceive(telemetryQueue, &dataToPub, portMAX_DELAY))
        {
            mqtt_publish_json(dataToPub);
            vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz 전송 (대역폭 조절)
        }
    }
}


void setup()
{
    Serial.begin(115200);
    // Serial2.begin(115200); // For Cat.M1 TSCM-LM [cite: 111, 112]

    int status;

    status = imu.begin();
    if (status < 0)
    {
        Serial.println("BMI088 initialization failed!");
        Serial.print("Error code: ");
        Serial.println(status);
        while (1)
            ;
    }

    status = imu.setRange(Bmi088::ACCEL_RANGE_6G, Bmi088::GYRO_RANGE_500DPS);
    if (status < 0)
    {
        Serial.println("Failed to set BMI088 ranges!");
        Serial.print("Error code: ");
        Serial.println(status);
        while (1)
            ;
    }

    status = imu.setOdr(Bmi088::ODR_400HZ);
    if (status < 0)
    {
        Serial.println("Failed to set ODR");
        Serial.println(status);
        while (1)
        {
        }
    }
    /* specify whether to use pin 3 or pin 4 to loop back the gyro interrupt */
    status = imu.mapSync(Bmi088::PIN_3);
    if (status < 0)
    {
        Serial.println("Failed to map sync pin");
        Serial.println(status);
        while (1)
        {
        }
    }
    
    // 큐 생성
    loggingQueue = xQueueCreate(20, sizeof(VehicleData));
    telemetryQueue = xQueueCreate(10, sizeof(VehicleData));

    // 태스크 할당 (기능별 코어 분리)
    xTaskCreatePinnedToCore(SensorTask, "Sensing", 4096, NULL, 3, NULL, CORE_SENSING);
    xTaskCreatePinnedToCore(LoggerTask, "Logging", 4096, NULL, 1, NULL, CORE_IO);
    xTaskCreatePinnedToCore(MqttTask, "MQTT", 8192, NULL, 2, NULL, CORE_IO);
}

void loop()
{
    // FreeRTOS 사용 시 loop는 비워두거나 시스템 모니터링 용도로 사용
    vTaskDelete(NULL);
}
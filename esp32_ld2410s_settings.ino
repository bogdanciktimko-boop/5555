/*
  ESP32 + HLK-LD2410S
  Читання поточних налаштувань датчика у Serial Monitor.

  Потрібна бібліотека: "ld2410" (автор ncmreynolds).

  Підключення (приклад):
  LD2410S TX -> ESP32 RX2 (GPIO16)
  LD2410S RX -> ESP32 TX2 (GPIO17)
  GND -> GND
  5V/3.3V -> живлення датчика (згідно вашої плати)
*/

#include <ld2410.h>

// UART2 на ESP32
static const uint8_t RADAR_RX_PIN = 16;
static const uint8_t RADAR_TX_PIN = 17;
static const uint32_t RADAR_BAUD = 256000; // Для LD2410S зазвичай 256000

HardwareSerial RadarSerial(2);
ld2410 radar;

unsigned long lastPrintMs = 0;

void printConfiguration() {
  Serial.println();
  Serial.println(F("========== LD2410S CONFIG =========="));

  Serial.print(F("Firmware: "));
  if (radar.firmware_major_version == 0 && radar.firmware_minor_version == 0) {
    Serial.println(F("невідомо (спробуйте оновити бібліотеку/підключення)"));
  } else {
    Serial.print(radar.firmware_major_version);
    Serial.print('.');
    Serial.print(radar.firmware_minor_version);
    Serial.print('.');
    Serial.println(radar.firmware_bugfix_version);
  }

  Serial.print(F("Max moving gate: "));
  Serial.println(radar.max_gate);

  Serial.print(F("Max stationary gate: "));
  Serial.println(radar.max_gate);

  Serial.print(F("Timeout (s): "));
  Serial.println(radar.sensor_idle_time);

  Serial.println(F("\nMoving sensitivity per gate:"));
  for (uint8_t gate = 0; gate <= radar.max_gate; gate++) {
    Serial.print(F("  Gate "));
    Serial.print(gate);
    Serial.print(F(": "));
    Serial.println(radar.motion_sensitivity[gate]);
  }

  Serial.println(F("\nStationary sensitivity per gate:"));
  for (uint8_t gate = 0; gate <= radar.max_gate; gate++) {
    Serial.print(F("  Gate "));
    Serial.print(gate);
    Serial.print(F(": "));
    Serial.println(radar.stationary_sensitivity[gate]);
  }

  Serial.println(F("===================================="));
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("Старт ESP32 + LD2410S..."));

  RadarSerial.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);

  if (!radar.begin(RadarSerial)) {
    Serial.println(F("[ERR] Не вдалося ініціалізувати LD2410S."));
    Serial.println(F("Перевірте дроти RX/TX, GND та baudrate."));
    while (true) {
      delay(1000);
    }
  }

  Serial.println(F("LD2410S підключений."));
  Serial.println(F("Запитую поточну конфігурацію..."));

  // Запит поточних налаштувань
  radar.requestCurrentConfiguration();
}

void loop() {
  radar.read();

  // Коли бібліотека отримає дані конфігурації, надрукуємо їх раз на 2с
  if (millis() - lastPrintMs > 2000) {
    lastPrintMs = millis();
    printConfiguration();

    // Повторний запит налаштувань (корисно, якщо датчик "прокинувся" пізніше)
    radar.requestCurrentConfiguration();
  }
}

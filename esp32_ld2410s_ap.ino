#include <WiFi.h>
#include <WebServer.h>

// ---------- Access Point ----------
const char* AP_SSID = "ESP32_LD2410S";
const char* AP_PASS = "12345678";  // min 8 chars

// ---------- LD2410S wiring (ESP32 UART pins) ----------
// Sensor 1 -> RX:16 TX:17
// Sensor 2 -> RX:18 TX:19
// Sensor 3 -> RX:25 TX:26

struct SensorState {
  bool personDetected = false;
  int distanceCm = -1;
  uint8_t targetState = 0;
  uint32_t lastUpdateMs = 0;
  bool online = false;
};

class LD2410SParser {
 public:
  LD2410SParser(HardwareSerial& serialRef) : serial(serialRef) {}

  void begin(uint32_t baud, int8_t rxPin, int8_t txPin) {
    serial.begin(baud, SERIAL_8N1, rxPin, txPin);
  }

  void update(SensorState& state) {
    while (serial.available() > 0) {
      uint8_t b = static_cast<uint8_t>(serial.read());
      parseByte(b, state);
    }

    if (millis() - state.lastUpdateMs > 3000) {
      state.online = false;
      state.personDetected = false;
      state.distanceCm = -1;
      state.targetState = 0;
    }
  }

 private:
  static constexpr uint8_t HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
  static constexpr uint8_t TAIL[4] = {0xF8, 0xF7, 0xF6, 0xF5};
  static constexpr size_t MAX_FRAME = 64;

  HardwareSerial& serial;
  uint8_t frame[MAX_FRAME]{};
  size_t framePos = 0;

  void resetFrame() { framePos = 0; }

  static uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
  }

  void decodeFrame(const uint8_t* buf, size_t n, SensorState& state) {
    if (n < 16) return;

    uint16_t dataLen = readU16LE(&buf[4]);
    if (dataLen + 10 != n) {
      return;
    }

    const uint8_t* payload = &buf[6];
    if (dataLen < 10) return;

    uint8_t targetState = payload[1];
    uint16_t movingDist = readU16LE(&payload[2]);
    uint16_t stationaryDist = readU16LE(&payload[5]);
    uint16_t detectDist = readU16LE(&payload[8]);

    bool detected = (targetState != 0);
    int distance = -1;

    if (detected) {
      if (detectDist > 0) {
        distance = static_cast<int>(detectDist);
      } else {
        distance = static_cast<int>(max(movingDist, stationaryDist));
      }
    }

    state.personDetected = detected;
    state.distanceCm = distance;
    state.targetState = targetState;
    state.lastUpdateMs = millis();
    state.online = true;
  }

  void parseByte(uint8_t b, SensorState& state) {
    if (framePos >= MAX_FRAME) {
      resetFrame();
    }

    frame[framePos++] = b;

    if (framePos <= 4) {
      for (size_t i = 0; i < framePos; ++i) {
        if (frame[i] != HEADER[i]) {
          resetFrame();
          return;
        }
      }
      return;
    }

    if (framePos >= 6) {
      uint16_t dataLen = readU16LE(&frame[4]);
      size_t expectedLen = static_cast<size_t>(dataLen) + 10;

      if (expectedLen > MAX_FRAME) {
        resetFrame();
        return;
      }

      if (framePos == expectedLen) {
        if (frame[expectedLen - 4] == TAIL[0] &&
            frame[expectedLen - 3] == TAIL[1] &&
            frame[expectedLen - 2] == TAIL[2] &&
            frame[expectedLen - 1] == TAIL[3]) {
          decodeFrame(frame, expectedLen, state);
        }
        resetFrame();
      }
    }
  }
};

HardwareSerial Radar1(1);
HardwareSerial Radar2(2);
HardwareSerial Radar3(0);

LD2410SParser parser1(Radar1);
LD2410SParser parser2(Radar2);
LD2410SParser parser3(Radar3);

SensorState sensors[3];
WebServer server(80);

String htmlPage() {
  return R"HTML(
<!doctype html>
<html lang="uk">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>ESP32 + 3x LD2410S</title>
  <style>
    body{font-family:Arial,sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:16px}
    h1{margin:0 0 16px 0}
    .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}
    .card{background:#1e293b;border-radius:14px;padding:14px;box-shadow:0 3px 10px rgba(0,0,0,.2)}
    .state{font-size:1.1rem;font-weight:700}
    .ok{color:#34d399}.no{color:#f87171}.off{color:#fbbf24}
    .meta{opacity:.85;margin-top:8px}
  </style>
</head>
<body>
  <h1>Дані LD2410S (3 датчики)</h1>
  <div class="grid" id="grid"></div>

<script>
async function refresh(){
  const r = await fetch('/api/sensors');
  const data = await r.json();
  const grid = document.getElementById('grid');
  grid.innerHTML = '';
  data.sensors.forEach((s, i) => {
    let cls = 'off', state = 'Немає зв\'язку';
    if (s.online) {
      if (s.personDetected) { cls = 'ok'; state = 'Людину виявлено'; }
      else { cls = 'no'; state = 'Людину не виявлено'; }
    }
    const dist = (s.distanceCm >= 0) ? `${s.distanceCm} см` : '—';
    grid.innerHTML += `
      <div class="card">
        <h3>Датчик ${i+1}</h3>
        <div class="state ${cls}">${state}</div>
        <div class="meta">Дистанція: <b>${dist}</b></div>
        <div class="meta">targetState: <b>${s.targetState}</b></div>
        <div class="meta">Оновлено: <b>${s.ageMs} мс тому</b></div>
      </div>`;
  });
}
setInterval(refresh, 500);
refresh();
</script>
</body>
</html>
)HTML";
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", htmlPage());
}

void handleApiSensors() {
  String json = "{\"sensors\":[";
  for (int i = 0; i < 3; ++i) {
    if (i) json += ",";
    uint32_t age = (sensors[i].lastUpdateMs == 0) ? 999999 : (millis() - sensors[i].lastUpdateMs);
    json += "{";
    json += "\"online\":" + String(sensors[i].online ? "true" : "false") + ",";
    json += "\"personDetected\":" + String(sensors[i].personDetected ? "true" : "false") + ",";
    json += "\"distanceCm\":" + String(sensors[i].distanceCm) + ",";
    json += "\"targetState\":" + String(sensors[i].targetState) + ",";
    json += "\"ageMs\":" + String(age);
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  // Changed sensor UART baud to 115200
  parser1.begin(115200, 16, 17);
  parser2.begin(115200, 18, 19);
  parser3.begin(115200, 25, 26);

  // Start ESP32 as Wi‑Fi access point instead of STA client mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/api/sensors", handleApiSensors);
  server.begin();
}

void loop() {
  parser1.update(sensors[0]);
  parser2.update(sensors[1]);
  parser3.update(sensors[2]);
  server.handleClient();
}

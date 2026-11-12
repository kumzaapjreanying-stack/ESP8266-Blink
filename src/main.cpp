#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// --- ตั้งค่า Wi-Fi Access Point (บอร์ดปล่อย Wi-Fi เอง) ---
const char *SSID_AP = "SmartParking-WiFi";
const char *PASSWORD_AP = "12345678"; // รหัสผ่าน 8 ตัวขึ้นไป (หรือปล่อยว่าง "" ถ้าไม่ต้องการรหัส)

// สร้าง Web Server ที่พอร์ต 80
ESP8266WebServer server(80);

// --- กำหนดขาเซนเซอร์และ LED ---
const int TRIG_PIN = D1; // HC-SR04 Trig
const int ECHO_PIN = D2; // HC-SR04 Echo
const int RED_LED_PIN   = D5; // LED แดง (มีรถจอด / ไม่ว่าง)
const int GREEN_LED_PIN = D6; // LED เขียว (ว่าง)

const int DISTANCE_THRESHOLD = 15; // ระยะตรวจจับ (ซม.)

// ตัวแปรเก็บสถานะช่องจอด A1 (เซนเซอร์จริง)
bool isOccupiedA1 = false;
float currentDistance = 0.0;

// ฟังก์ชันวัดระยะทางจาก HC-SR04
void measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms (~5m)
  if (duration == 0) {
    currentDistance = 999.0;
  } else {
    currentDistance = (duration * 0.034) / 2.0;
  }

  // ประเมินสถานะช่อง A1
  if (currentDistance > 0 && currentDistance < DISTANCE_THRESHOLD) {
    isOccupiedA1 = true;
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    isOccupiedA1 = false;
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  }
}

// หน้าเว็บ HTML สไตล์ Modern Dashboard สวยงาม รองรับมือถือและคอม
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Parking Dashboard</title>
  <link href="https://fonts.googleapis.com/css2?family=Kanit:wght@300;400;600&display=swap" rel="stylesheet">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Kanit', sans-serif; }
    body { background: #0f172a; color: #f8fafc; min-height: 100vh; padding: 20px; }
    .header { text-align: center; margin-bottom: 25px; }
    .header h1 { font-size: 2rem; color: #38bdf8; display: flex; align-items: center; justify-content: center; gap: 10px; }
    .header p { color: #94a3b8; font-size: 0.95rem; margin-top: 5px; }
    
    .stats-bar {
      display: flex; justify-content: center; gap: 15px; margin-bottom: 30px; flex-wrap: wrap;
    }
    .stat-card {
      background: #1e293b; padding: 15px 25px; border-radius: 12px; border: 1px solid #334155;
      min-width: 130px; text-align: center;
    }
    .stat-card .num { font-size: 1.8rem; font-weight: 600; }
    .stat-card .label { font-size: 0.85rem; color: #94a3b8; }
    .stat-free { color: #22c55e; }
    .stat-busy { color: #ef4444; }
    .stat-total { color: #38bdf8; }

    .parking-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 20px; max-width: 1000px; margin: 0 auto;
    }
    .slot {
      background: #1e293b; border-radius: 16px; padding: 20px;
      border: 2px solid #334155; position: relative;
      transition: all 0.3s ease; display: flex; flex-direction: column; justify-content: space-between;
      min-height: 200px;
    }
    .slot.real-sensor {
      box-shadow: 0 0 15px rgba(56, 189, 248, 0.25);
      border-color: #38bdf8;
    }
    .slot-tag {
      position: absolute; top: 12px; right: 12px;
      background: #0284c7; color: white; font-size: 0.7rem;
      padding: 3px 8px; border-radius: 6px; font-weight: 600;
    }
    .slot-tag.mock { background: #475569; }

    .slot-header { display: flex; align-items: center; justify-content: space-between; }
    .slot-name { font-size: 1.6rem; font-weight: 600; color: #f1f5f9; }
    
    .slot-body { text-align: center; padding: 15px 0; }
    .slot-icon { font-size: 3.5rem; line-height: 1; transition: transform 0.3s; }
    .slot-status-text { font-size: 1.25rem; font-weight: 600; margin-top: 10px; }

    .slot-footer {
      border-top: 1px solid rgba(255,255,255,0.08); padding-top: 10px;
      display: flex; justify-content: space-between; font-size: 0.85rem; color: #94a3b8;
    }

    /* สไตล์ตามสถานะ */
    .slot.free { border-color: #22c55e; background: linear-gradient(145deg, #1e293b 0%, rgba(34, 197, 94, 0.08) 100%); }
    .slot.free .slot-status-text { color: #22c55e; }
    .slot.free .slot-icon { filter: drop-shadow(0 0 8px rgba(34, 197, 94, 0.4)); }

    .slot.busy { border-color: #ef4444; background: linear-gradient(145deg, #1e293b 0%, rgba(239, 68, 68, 0.08) 100%); }
    .slot.busy .slot-status-text { color: #ef4444; }
    .slot.busy .slot-icon { filter: drop-shadow(0 0 8px rgba(239, 68, 68, 0.4)); }

    .wifi-info {
      text-align: center; margin-top: 30px; font-size: 0.85rem; color: #64748b;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>🚗 Smart Parking System</h1>
    <p>ระบบตรวจจับและแสดงสถานะช่องจอดรถแบบเรียลไทม์ (ESP8266)</p>
  </div>

  <div class="stats-bar">
    <div class="stat-card">
      <div class="num stat-total">6</div>
      <div class="label">ช่องทั้งหมด</div>
    </div>
    <div class="stat-card">
      <div class="num stat-free" id="free-count">-</div>
      <div class="label">ช่องว่าง</div>
    </div>
    <div class="stat-card">
      <div class="num stat-busy" id="busy-count">-</div>
      <div class="label">มีรถจอด</div>
    </div>
  </div>

  <div class="parking-grid" id="grid">
    <!-- กล่องช่องจอดรถ 6 ช่อง จะสร้างผ่าน JavaScript -->
  </div>

  <div class="wifi-info">
    เชื่อมต่อผ่าน Access Point: <strong>SmartParking-WiFi</strong> (IP: 192.168.4.1)
  </div>

  <script>
    // ข้อมูลช่องจำลอง A2 - A6
    const mockSlots = [
      { id: 'A2', busy: true },
      { id: 'A3', busy: false },
      { id: 'A4', busy: true },
      { id: 'A5', busy: false },
      { id: 'A6', busy: false }
    ];

    async function updateStatus() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        
        let freeCount = 0;
        let busyCount = 0;

        let html = '';

        // ช่อง A1: เซนเซอร์จริง
        const a1Busy = data.a1_busy;
        if (a1Busy) busyCount++; else freeCount++;

        html += `
          <div class="slot real-sensor ${a1Busy ? 'busy' : 'free'}">
            <span class="slot-tag">เซนเซอร์จริง (HC-SR04)</span>
            <div class="slot-header">
              <span class="slot-name">ช่อง A1</span>
            </div>
            <div class="slot-body">
              <div class="slot-icon">${a1Busy ? '🚘' : '🟢'}</div>
              <div class="slot-status-text">${a1Busy ? 'ไม่ว่าง (มีรถจอด)' : 'ว่าง (จอดได้)'}</div>
            </div>
            <div class="slot-footer">
              <span>ระยะวัดได้:</span>
              <strong>${data.a1_distance > 500 ? '> 500' : data.a1_distance} cm</strong>
            </div>
          </div>
        `;

        // ช่อง A2 - A6: Mock Slots
        mockSlots.forEach(s => {
          if (s.busy) busyCount++; else freeCount++;
          html += `
            <div class="slot ${s.busy ? 'busy' : 'free'}">
              <span class="slot-tag mock">ช่องจำลอง</span>
              <div class="slot-header">
                <span class="slot-name">ช่อง ${s.id}</span>
              </div>
              <div class="slot-body">
                <div class="slot-icon">${s.busy ? '🚙' : '🟢'}</div>
                <div class="slot-status-text">${s.busy ? 'ไม่ว่าง' : 'ว่าง (จอดได้)'}</div>
              </div>
              <div class="slot-footer">
                <span>สถานะระบบ:</span>
                <span>${s.busy ? 'ตรวจพบยานพาหนะ' : 'พร้อมใช้งาน'}</span>
              </div>
            </div>
          `;
        });

        document.getElementById('grid').innerHTML = html;
        document.getElementById('free-count').innerText = freeCount;
        document.getElementById('busy-count').innerText = busyCount;

      } catch (err) {
        console.error('Fetch error:', err);
      }
    }

    // อัปเดตทุก ๆ 800ms ให้ตอบสนองไวตามเซนเซอร์จริง
    setInterval(updateStatus, 800);
    updateStatus();
  </script>
</body>
</html>
)rawliteral";

// Handler ส่งหน้าหลัก
void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
}

// Handler API คืนค่า JSON สถานะเซนเซอร์ให้ JavaScript นำไปแสดงผล
void handleStatus() {
  String json = "{";
  json += "\"a1_busy\":" + String(isOccupiedA1 ? "true" : "false") + ",";
  json += "\"a1_distance\":" + String(currentDistance, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);

  // เริ่มต้นสร้าง Wi-Fi Access Point
  Serial.println("\nกำลังเริ่มต้น Wi-Fi Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID_AP, PASSWORD_AP);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("ชื่อ Wi-Fi: ");
  Serial.println(SSID_AP);
  Serial.print("รหัสผ่าน: ");
  Serial.println(PASSWORD_AP);
  Serial.print("IP Address สำหรับเปิดหน้าเว็บ: http://");
  Serial.println(myIP);

  // ตั้งค่า Route ของ Web Server
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.begin();
  Serial.println("Web Server เริ่มทำงานเรียบร้อยแล้ว!");
}

void loop() {
  server.handleClient(); // จัดการคำขอหน้าเว็บ

  // วัดระยะทุก ๆ 200ms
  static unsigned long lastMeasure = 0;
  if (millis() - lastMeasure > 200) {
    lastMeasure = millis();
    measureDistance();
  }
}

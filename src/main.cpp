#include <Arduino.h>

void setup() {
  // เริ่มต้นการสื่อสาร Serial ด้วย baud rate 115200
  Serial.begin(115200);

  // ตั้งค่าพิน LED ในตัวบอร์ดให้เป็นขา Output
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);  // เปิดไฟ LED (Active LOW)
  Serial.println("ไฟติด");
  delay(200);                      // สลับเร็วขึ้น (0.2 วินาที)

  digitalWrite(LED_BUILTIN, HIGH); // ปิดไฟ LED
  Serial.println("ไฟดับ");
  delay(200);                      // สลับเร็วขึ้น (0.2 วินาที)
}

#include <SPI.h>
#include <MFRC522.h>
#include "BluetoothSerial.h"

// Cấu hình RFID
#define SS_PIN 21   // GPIO21
#define RST_PIN 22  // GPIO22
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Cấu hình MQ3
const int MQ3_PIN = 34;  // GPIO34 (ADC1_CH6)
float alcoholPPM = 0;

// Cài đặt bluetooth
BluetoothSerial ESP_BT;
String DeviceName = "ESP_32";

void setup() {
  Serial.begin(9600);

  // Khoi chay bluetooth
  ESP_BT.begin(DeviceName);
  Serial.println("Phát kết nối bluetooth!");


  SPI.begin();         // Khởi động SPI
  mfrc522.PCD_Init();  // Khởi động RFID
  pinMode(MQ3_PIN, INPUT);

  // Chờ cảm biến MQ3 khởi động (20-30 giây)
  Serial.println("Đang khởi động MQ3...");
  // delay(30000);
  Serial.println("Hệ thống sẵn sàng!");

  xTaskCreate(communicationTask, "communicationTask", 4096, NULL, 200, NULL);
}

void communicationTask(void *parameters) {
  while (true) {
    if (ESP_BT.available()) {
      String message = ESP_BT.readStringUntil('\n');
      Serial.println("Received: " + message);
      ESP_BT.println(message);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

void loop() {
  // Đọc RFID
  

}
void getInfo() {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.print("UID thẻ: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();

    // Đọc giá trị MQ3
    int rawValue = analogRead(MQ3_PIN);
    alcoholPPM = map(rawValue, 0, 4095, 0, 500);  // ESP32 có ADC 12-bit (0-4095)

    Serial.print("Giá trị analog: ");
    Serial.print(rawValue);
    Serial.print(" | Nồng độ cồn ước tính: ");
    Serial.print(alcoholPPM);
    Serial.println(" ppm");

    mfrc522.PICC_HaltA();
    delay(1000);
  }
}
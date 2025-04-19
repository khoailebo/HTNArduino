#include <SPI.h>
#include <MFRC522.h>
#include "BluetoothSerial.h"

// Cấu hình RFID
#define SS_PIN 21   // GPIO21
#define RST_PIN 22  // GPIO22
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key = {
  .keyByte = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
};
// Cấu hình MQ3
const int MQ3_PIN = 34;  // GPIO34 (ADC1_CH6)
float alcoholPPM = 0;

// Cài đặt bluetooth
BluetoothSerial ESP_BT;
String DeviceName = "ESP_32";
bool reading = true;
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
  // xTaskCreate(readCardTask, "readcard", 4096, NULL, 100, NULL);
}

void communicationTask(void *parameters) {
  while (true) {
    if (ESP_BT.available()) {
      String message = ESP_BT.readStringUntil('\n');
      Serial.println("Received: " + message);

      int separatorIndex = message.indexOf('|');
      if (separatorIndex != -1) {
        String eventName = message.substring(0, separatorIndex);
        String evenData = message.substring(separatorIndex + 1);
        Serial.print("Event name: ");
        Serial.println(eventName);
        Serial.print("Event data: ");
        Serial.println(evenData);

        if (eventName == "TestConnection") {
          ESP_BT.println(eventName + "|" + "Connection ok");
        } else if (eventName == "GetDriverId") {
          xTaskCreate(readCardTask, "readCard", 4096, NULL, 300, NULL);
          // readCard = true;
        } else if (eventName == "GetAlcoholLevel"){
          xTaskCreate(detectAlcoholTask,"detectAlcoholTask",4096,NULL,1,NULL);
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void readCardTask(void *parameters) {
  byte data[18];
  byte block = 4;
  bool read = false;
  long startTime = millis();
  String sendMsg = String("GetDriverId|");
  while (!read && (millis() - startTime) < 10000) {

    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      vTaskDelay(100 / portTICK_RATE_MS);
      continue;
    }
    // xTaskCreate(readCardTask, "readcard",4096, NULL,100,NULL);
    byte block = 4;
    byte data[18];

    if (readRFIDBlock(block, data)) {
      for (int i = 0; i < 16; i++) {
        sendMsg += (char)data[i];
      }
      read = true;
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  ESP_BT.println(sendMsg);
  vTaskDelete(NULL);
}

void readCard() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;
  // xTaskCreate(readCardTask, "readcard",4096, NULL,100,NULL);
  byte block = 4;
  byte data[18];
  if (readRFIDBlock(block, data)) {
    for (int i = 0; i < 16; i++) {
      Serial.write(data[i]);
    }
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void loop() {
  // // Đọc RFID
  // if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;
  // // xTaskCreate(readCardTask, "readcard",4096, NULL,100,NULL);
  // byte block = 4;
  // byte data[18];
  // readRFIDBlock(block, data);
  // for (int i = 0; i < 16; i++) {
  //   Serial.write(data[i]);
  // }

  // mfrc522.PICC_HaltA();
  // mfrc522.PCD_StopCrypto1();

  // delay(2000);  // Tránh ghi nhiều lần liên tiếp
}
#define RL 10000.0  // 10k ohm
#define R0 6000.0   // cần đo khi cảm biến ở không khí sạch
#define m -0.41
#define b 0.57

void detectAlcoholTask(void *parameters){
  float alcoholLevel = 0;
  long startTime = millis();
  while((millis() - startTime) < 5000){
    float tempAlcoholLevel = detectAlcohol();
    if(tempAlcoholLevel > alcoholLevel)alcoholLevel = tempAlcoholLevel;
    vTaskDelay(100/portTICK_RATE_MS);
  }
  String msg = String("GetAlcoholLevel|");
  ESP_BT.println(msg + String(alcoholLevel,4));
  vTaskDelete(NULL);

}

float detectAlcohol() {
  int rawValue = analogRead(MQ3_PIN);                    // Đọc giá trị ADC (0-4095)
  float voltage = rawValue * (3.3 / 4095.0);             // Chuyển đổi sang điện áp (giả sử dùng 3.3V)
  float RS = (3.3 - voltage) / voltage * RL;             // RL là điện trở tải (thường 10k)
  float ratio = RS / R0;                                 // R0 là điện trở cảm biến trong không khí sạch (phải hiệu chuẩn)
  float alcoholPPM = pow(10, ((log10(ratio) - b) / m));  // m và b là hệ số từ đồ thị datasheet
  return alcoholPPM * 0.0004;
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

/// Hàm ghi dữ liệu 16 byte vào block chỉ định trên thẻ RFID
bool writeRFIDBlock(byte blockNumber, byte *data) {
  MFRC522::StatusCode status;

  // Xác thực block
  status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    blockNumber,
    &key,
    &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print("Lỗi xác thực: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Ghi dữ liệu
  status = mfrc522.MIFARE_Write(blockNumber, data, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Ghi lỗi: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  return true;
}

/// Hàm đọc dữ liệu từ block chỉ định trên thẻ RFID
bool readRFIDBlock(byte blockNumber, byte *buffer) {
  MFRC522::StatusCode status;

  // Xác thực block trước khi đọc
  status = mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A,
    blockNumber,
    &key,
    &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    Serial.print("Lỗi xác thực khi đọc: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  byte size = 18;  // 16 bytes data + 2 bytes CRC
  status = mfrc522.MIFARE_Read(blockNumber, buffer, &size);

  if (status != MFRC522::STATUS_OK) {
    Serial.print("Lỗi đọc: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  return true;
}

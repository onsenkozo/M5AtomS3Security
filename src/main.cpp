#include <thread>
#include <chrono>
#include <fstream>
#include <Arduino.h>
#include <M5AtomS3.h>
#include <BLEServer.h>
#include <BLEDevice.h>
#include "FS.h"
#include "SPIFFS.h"

#define DEVICE_NAME "M5AtomS3SecurityNode" // デバイス名

const char *storage = "/data/settings.txt";

std::shared_ptr<std::thread> th = nullptr;
BLEServer *server = nullptr;
BLEAdvertising *advertising = nullptr;

void setAdvertisementData(BLEAdvertising* pAdvertising, uint8_t& device_no, bool& state) {
  // string領域に送信情報を連結する
  std::string strData = "";
  strData += (char)0xff;                  // Manufacturer specific data
  strData += (char)0xff;                  // manufacturer ID low byte
  strData += (char)0xff;                  // manufacturer ID high byte
  strData += (char)device_no;             // サーバー識別番号
  strData += (char)state ? 1 : 0;         // lock state
  strData = (char)strData.length() + strData; // 先頭にLengthを設定

  // デバイス名とフラグをセットし、送信情報を組み込んでアドバタイズオブジェクトに設定する
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  oAdvertisementData.setName(DEVICE_NAME);
  oAdvertisementData.setFlags(0x06);      // LE General Discoverable Mode | BR_EDR_NOT_SUPPORTED
  oAdvertisementData.addData(strData);
  pAdvertising->setAdvertisementData(oAdvertisementData);
}

void setupBLE(uint8_t& device_no, bool& state) {
  USBSerial.println("Starting BLE");
  BLEDevice::init("my-peripheral");
  server = BLEDevice::createServer();
  advertising = server->getAdvertising();
  th = std::make_shared<std::thread>([&]() {
    while (true) {
      setAdvertisementData(advertising, device_no, state);
      USBSerial.print("Starting Advertisement: ");
      USBSerial.println(state);
      advertising->start();
      std::this_thread::sleep_for(std::chrono::seconds(2));
      advertising->stop();
      USBSerial.println("Stop Advertisement. ");
    }
  });
}

#define KEY_PIN 1  // Define Limit Pin.
bool longPress = false;       // ボタン長押しフラグ
bool reverse = false;         // 条件反転フラグ
bool state = false;           // 本体ボタン状態格納用
bool initializeState = false; // ボタン初期表示完了フラグ

static uint8_t device_no = 1;    // デバイス識別番号（1～99）
static const char run = 0;
static const char reverse_setting = 1;
static const char node_no_10 = 2;
static const char node_no_1 = 3;
static char config_state = run;
static const bool FORMAT_SPIFFS_IF_FAILED = true;

void setup() {
    USBSerial.begin(115200);
    M5.begin(true,true,false,false);
    M5.Lcd.setRotation(2);

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
        M5.Lcd.println("Failed to mount SPIFFS");
        return;
    }

    File fp = SPIFFS.open(storage, FILE_READ);
    // 書き込み
    fp.readBytes((char*)&device_no, 1);
    fp.close();
    Serial.println("SPIFFS FILE WRITE");
    if (device_no == 0) {
      device_no = 1;
    }

    pinMode(KEY_PIN, INPUT_PULLUP);
    setupBLE(device_no, state);
}

// 液晶画面変更処理関数
void lcdChange()  {
  char digit[3];
  switch (config_state) {
    case run:
    case reverse_setting:
      if ((state xor reverse) == true) {
        M5.Lcd.fillRect(0, 0, 128, 128, M5.Lcd.color565(0, 128, 0));   // 塗り潰し（緑）
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(M5.Lcd.color565(0, 0, 0));         // 文字色指定
        M5.Lcd.setCursor(30, 54);                              // 表示開始位置左上角（X,Y）
        M5.Lcd.print("LOCK");                                  // ON表示
        USBSerial.println("LOCK");
      } else {
        M5.Lcd.fillRect(0, 0, 128, 128, M5.Lcd.color565(128, 0, 0));   // 塗り潰し（赤）
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(M5.Lcd.color565(128, 128, 128));   // 文字色指定
        M5.Lcd.setCursor(29, 57);                              // 表示開始位置左上角（X,Y）
        M5.Lcd.print("UNLOCK");                                // OFF表示
        USBSerial.println("UNLOCK");
      }
      if (config_state == reverse_setting) {
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(M5.Lcd.color565(255, 255, 255));   // 文字色指定
        M5.Lcd.setCursor(0, 0);                                // 表示開始位置左上角（X,Y）
        M5.Lcd.print("REVERSE\nSETTING");                       // ConfigState表示
      }
      break;
    case node_no_10:
      M5.Lcd.fillRect(0, 0, 128, 128, M5.Lcd.color565(0, 0, 128));   // 塗り潰し（青）
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(M5.Lcd.color565(255, 255, 255));   // 文字色指定
      M5.Lcd.setCursor(0, 0);                                // 表示開始位置左上角（X,Y）
      M5.Lcd.print("NODE NO 10");                            // ConfigState表示
      M5.Lcd.setTextColor(M5.Lcd.color565(128, 128, 128));   // 文字色指定
      M5.Lcd.setTextSize(5);
      M5.Lcd.setCursor(38, 48);                              // 表示開始位置左上角（X,Y）
      sprintf(digit, "%02d", device_no);                     // ConfigState表示
      M5.Lcd.print(digit);                                   // ConfigState表示
      break;
    case node_no_1:
      M5.Lcd.fillRect(0, 0, 128, 128, M5.Lcd.color565(0, 0, 128));   // 塗り潰し（青）
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(M5.Lcd.color565(255, 255, 255));   // 文字色指定
      M5.Lcd.setCursor(0, 0);                                // 表示開始位置左上角（X,Y）
      M5.Lcd.print("NODE NO 1");                             // ConfigState表示
      M5.Lcd.setTextColor(M5.Lcd.color565(128, 128, 128));   // 文字色指定
      M5.Lcd.setTextSize(5);
      M5.Lcd.setCursor(38, 48);                              // 表示開始位置左上角（X,Y）
      sprintf(digit, "%02d", device_no);                     // ConfigState表示
      M5.Lcd.print(digit);                                   // ConfigState表示
      break;
  }
}

void changeConfigState() {
  switch (config_state) {
    case run:
      config_state = reverse_setting;
      break;
    case reverse_setting:
      config_state = node_no_10;
      break;
    case node_no_10:
      config_state = node_no_1;
      break;
    case node_no_1:
      config_state = run;
      File fp = SPIFFS.open(storage, FILE_WRITE);
      // 書き込み
      fp.write(&device_no, 1);
      fp.close();
      Serial.println("SPIFFS FILE WRITE");
      break;
  }
}

void loop() {
  M5.update();  // ボタン状態更新
  // 本体スイッチ処理
  if (!longPress and M5.Btn.pressedFor(3000)) {  // 3秒間ボタンが押されていれば
    changeConfigState();
    lcdChange();                  // 液晶画面表示変更
    longPress = true;
  } else if (longPress and M5.Btn.wasReleased()) {
    longPress = false;
  } else if (M5.Btn.wasReleased()) {
    switch (config_state) {
      case reverse_setting:
        USBSerial.print("REVERSE STATE: ");
        USBSerial.print(reverse);
        reverse = !reverse;           // ON/OFF状態反転
        USBSerial.print(" to ");
        USBSerial.println(reverse);
        lcdChange();                  // 液晶画面表示変更
        break;
      case node_no_10:
        device_no = (device_no + 10) % 100;
        USBSerial.print("Device NO: ");
        USBSerial.println(device_no);
        lcdChange();                  // 液晶画面表示変更
        break;
      case node_no_1:
        device_no = (device_no / 10) * 10 + ((device_no % 10) + 1) % 10;
        if (device_no == 0) {
          device_no = 1;
        }
        USBSerial.print("Device NO: ");
        USBSerial.println(device_no);
        lcdChange();                  // 液晶画面表示変更
        break;
    }
  }
  // 外部スイッチ処理
  if (digitalRead(KEY_PIN) == LOW && (state == false || !initializeState)) {
    initializeState = true;
    state = true;             // ON/OFF状態をtrueへ
    lcdChange();              // 液晶画面表示変更
  } else if (digitalRead(KEY_PIN) == HIGH && (state == true || !initializeState)) {
    initializeState = true;
    state = false;            // ON/OFF状態をfalseへ
    lcdChange();              // 液晶画面表示変更
  }
  delay(100);   // 遅延時間
}

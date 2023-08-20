#include <thread>
#include <chrono>
#include <fstream>
#include <Arduino.h>
#include <M5AtomS3.h>
#include <BLEServer.h>
#include <BLEDevice.h>
#include "FS.h"
#include "SPIFFS.h"

#define DEVICE_NAME "M5Sense" // デバイス名
#define KEY_PIN 1  // Define Limit Pin.

const char *storage = "/data/settings.txt";

std::shared_ptr<std::thread> th = nullptr;
BLEServer *server = nullptr;
BLEAdvertising *advertising = nullptr;

bool longPress = false;       // ボタン長押しフラグ
bool reverse = false;         // 条件反転フラグ
bool state = false;           // 本体ボタン状態格納用
bool initializeState = false; // ボタン初期表示完了フラグ

static uint8_t device_no = 1; // デバイス識別番号（1～99）
bool old_reverse = false;     // 変更前条件反転フラグ
uint8_t old_device_no = 1;    // 変更前デバイス識別番号（1～99）

enum class configState : uint8_t {
  run,
  reverse_setting,
  node_no_10,
  node_no_1,
  confirm_save,
};
static configState config_state = configState::run;
bool confirm_save_flag = false;

static const bool FORMAT_SPIFFS_IF_FAILED = true;

void setAdvertisementData(BLEAdvertising* pAdvertising, uint8_t& device_no, bool& state) {
  // string領域に送信情報を連結する
  std::string strData = "";
  strData += (char)0xff;                  // Manufacturer specific data
  strData += (char)0xff;                  // manufacturer ID low byte
  strData += (char)0xff;                  // manufacturer ID high byte
  strData += (char)device_no;             // デバイス番号
  strData += (char)state ? 1 : 0;         // lock state
  strData = (char)strData.length() + strData; // 先頭にLengthを設定

  // デバイス名とフラグをセットし、送信情報を組み込んでアドバタイズオブジェクトに設定する
  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  oAdvertisementData.setName(DEVICE_NAME);
  oAdvertisementData.setFlags(0x06);      // LE General Discoverable Mode | BR_EDR_NOT_SUPPORTED
  oAdvertisementData.addData(strData);
  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->setAdvertisementType(esp_ble_adv_type_t::ADV_TYPE_NONCONN_IND);
}

void setupBLE(uint8_t& device_no, bool& state) {
  USBSerial.println("Starting BLE");
  BLEDevice::init(DEVICE_NAME);
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

void setup() {
    USBSerial.begin(115200);
    M5.begin(true,true,false,false);
    M5.Lcd.setRotation(2);

    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
        M5.Lcd.println("Failed to mount SPIFFS");
        return;
    }

    // 設定読み込み
    File fp = SPIFFS.open(storage, FILE_READ);
    device_no = fp.read();
    reverse = fp.read() > 0 ? true : false;
    fp.close();
    USBSerial.println("SPIFFS FILE READ");
    if (device_no == 0) {
      device_no = 1;
    }

    pinMode(KEY_PIN, INPUT_PULLUP);
    setupBLE(device_no, state);
}

// 液晶画面変更処理関数
void lcdChange()  {
  char digit[4];
  switch (config_state) {
    case configState::run:
    case configState::reverse_setting:
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
      if (config_state == configState::reverse_setting) {
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(M5.Lcd.color565(255, 255, 255));   // 文字色指定
        M5.Lcd.setCursor(0, 0);                                // 表示開始位置左上角（X,Y）
        M5.Lcd.print("REVERSE\nSETTING");                       // ConfigState表示
      }
      break;
    case configState::node_no_10:
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
    case configState::node_no_1:
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
    case configState::confirm_save:
      M5.Lcd.fillRect(0, 0, 128, 128, M5.Lcd.color565(0, 128, 128));   // 塗り潰し（黄）
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(M5.Lcd.color565(0 , 0, 0));        // 文字色指定
      M5.Lcd.setCursor(0, 0);                                // 表示開始位置左上角（X,Y）
      M5.Lcd.print("CONFIRM\nSAVE");                          // ConfigState表示
      M5.Lcd.setTextSize(5);
      if (confirm_save_flag) {
        M5.Lcd.setCursor(22, 48);                            // 表示開始位置左上角（X,Y）
      } else {
        M5.Lcd.setCursor(38, 48);                            // 表示開始位置左上角（X,Y）
      }
      sprintf(digit, "%s", confirm_save_flag ? "YES" : "NO");// ConfigState表示
      M5.Lcd.print(digit);                                   // ConfigState表示
      break;
  }
}

void changeConfigState() {
  switch (config_state) {
    case configState::run:
      config_state = configState::reverse_setting;
      old_reverse = reverse;
      USBSerial.println("Enter to Config mode.");
      USBSerial.println("Reverse Setting.");
      break;
    case configState::reverse_setting:
      config_state = configState::node_no_10;
      old_device_no = device_no;
      USBSerial.println("Node No first order.");
      break;
    case configState::node_no_10:
      config_state = configState::node_no_1;
      USBSerial.println("Node No second order.");
      break;
    case configState::node_no_1:
      config_state = configState::confirm_save;
      USBSerial.println("Confirm save.");
      break;
    case configState::confirm_save:
      config_state = configState::run;
      if (confirm_save_flag) {
        // 設定書き込み
        File fp = SPIFFS.open(storage, FILE_WRITE);
        fp.write((uint8_t)device_no);
        fp.write((uint8_t)(reverse ? 1 : 0));
        fp.close();
        USBSerial.println("SPIFFS FILE WRITE");
        confirm_save_flag = false;
      } else {
        device_no = old_device_no;
        reverse = old_reverse;
        USBSerial.println("Discard changes.");
      }
      USBSerial.println("Exit from Config mode.");
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
      case configState::reverse_setting:
        USBSerial.print("REVERSE STATE: ");
        USBSerial.print(reverse);
        reverse = !reverse;           // ON/OFF状態反転
        USBSerial.print(" to ");
        USBSerial.println(reverse);
        lcdChange();                  // 液晶画面表示変更
        break;
      case configState::node_no_10:
        device_no = (device_no + 10) % 100;
        USBSerial.print("Device NO: ");
        USBSerial.println(device_no);
        lcdChange();                  // 液晶画面表示変更
        break;
      case configState::node_no_1:
        device_no = (device_no / 10) * 10 + ((device_no % 10) + 1) % 10;
        if (device_no == 0) {
          device_no = 1;
        }
        USBSerial.print("Device NO: ");
        USBSerial.println(device_no);
        lcdChange();                  // 液晶画面表示変更
        break;
      case configState::confirm_save:
        USBSerial.print("SAVE? ");
        USBSerial.print(confirm_save_flag);
        confirm_save_flag = !confirm_save_flag;           // ON/OFF状態反転
        USBSerial.print(" to ");
        USBSerial.println(confirm_save_flag);
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

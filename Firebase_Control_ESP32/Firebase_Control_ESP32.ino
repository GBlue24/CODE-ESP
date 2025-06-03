
#include "ACS712.h"
#include <Wire.h>
#include <FirebaseESP32.h>
#include <addons/RTDBHelper.h>
#include <WiFiManager.h>
#include <ZMPT101B.h>
#include <LiquidCrystal_I2C.h>

// Device pins
#define fan 4
#define pump 2
#define bulb 5

// Firebase config
#define DATABASE_URL "project1-ee980-default-rtdb.firebaseio.com"
#define API_KEY "AIzaSyBgG9M6LrqzQEngs0Xt6A5QDg5LS-UzN2A"

FirebaseData FirebaseStatus;
FirebaseData FirebaseData3;
FirebaseData FirebaseData4;
FirebaseData FirebaseData5;
FirebaseData FirebaseDataVA1;
FirebaseData FirebaseDataVA2;
FirebaseData FirebaseDataVA3;
FirebaseData FirebaseDataSensor1;
FirebaseData FirebaseDataSensor2;
FirebaseData FirebaseDataSensor3;

FirebaseAuth auth;
FirebaseConfig config;

// Sensors
#define ACTUAL_VOLTAGE 220.0f

ACS712  ACS_bulb(35, 3.3, 4095, 66.0);
ACS712  ACS_fan(36, 3.3, 4095, 66.0);
ACS712  ACS_pump(32, 3.3, 4095, 66.0);

ZMPT101B voltageSensor_bulb(34, 50.0);
ZMPT101B voltageSensor_pump(33, 50.0);
ZMPT101B voltageSensor_fan(39, 50.0);

LiquidCrystal_I2C lcd(0x27, 16, 2);

int calibration_factor_b = 30;
int calibration_factor_f = 30;
int calibration_factor_p = 30;
int displayState = 0;  // 0: hiển thị temp & humi, 1: hiển thị ánh sáng
unsigned long lastDisplayTime = 0;
const unsigned long displayInterval = 2000; // thời gian chuyển đổi giữa các màn hình (ms)


unsigned long lastSensorRead = 0;
unsigned long lastUploadTime = 0;
unsigned long sensorInterval = 1000;
unsigned long uploadInterval = 3000;

void setup() {
  Serial.begin(115200);

  pinMode(pump, OUTPUT);
  pinMode(fan, OUTPUT);
  pinMode(bulb, OUTPUT);

  ACS_fan.autoMidPoint();
  ACS_bulb.autoMidPoint();
  ACS_pump.autoMidPoint();

  lcd.init();
  lcd.backlight();

  WiFiManager wm;
  bool res = wm.autoConnect("AutoConnectAP", "password");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("WIFI connected");
  }

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);
}

void loop() {
  checkMCUStatus();
  controlDevices();

  unsigned long now = millis();
  if (now - lastSensorRead >= sensorInterval) {
    lastSensorRead = now;
    readAndDisplaySensors();
  }

  if (now - lastUploadTime >= uploadInterval) {
    lastUploadTime = now;
    measureAndUploadVoltageCurrent();
  }
}


//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>CHECK HOẠT ĐỘNG CỦA ESP32<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
void checkMCUStatus() {
  String ESP_status;
  if (Firebase.getString(FirebaseStatus, "/CheckMCU/esp32")) {
    ESP_status = FirebaseStatus.stringData();
    Serial.println("Nhận lệnh kiểm tra hoạt động");
    if (ESP_status == "OFF") {
      Serial.println("Xác nhận hoạt động");
      Firebase.setString(FirebaseStatus, "/CheckMCU/esp32", "ON");
    }
  } else {
    Serial.println("MCU check error: " + FirebaseStatus.errorReason());
  }
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>ĐIỀU KHIỂN TỪ XA <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
void controlDevices() {
  String pump_web, bulb_web, fan_web;

  if (Firebase.getString(FirebaseData3, "/Device/Pump"))
    pump_web = FirebaseData3.stringData();

  if (Firebase.getString(FirebaseData4, "/Device/Bulb"))
    bulb_web = FirebaseData4.stringData();

  if (Firebase.getString(FirebaseData5, "/Device/Fan"))
    fan_web = FirebaseData5.stringData();

  digitalWrite(pump, pump_web == "ON" ? HIGH : LOW);
  Serial.print("Điều khiển Bơm từ xa: ");
  Serial.println(pump_web);
  
  digitalWrite(bulb, bulb_web == "ON" ? HIGH : LOW);
  Serial.print("Điều khiển Đèn từ xa: ");
  Serial.println(bulb_web);
  
  digitalWrite(fan, fan_web == "ON" ? HIGH : LOW);
  Serial.print("Điều khiển Quạt từ xa: ");
  Serial.println(fan_web);
}


//...............................................HIỂN THỊ MÔI TRƯỜNG....................................
void readAndDisplaySensors() {
  int humi = 0, temp = 0, light = 0;

  if (Firebase.getString(FirebaseDataSensor1, "/Sensor/Humidity"))
    humi = FirebaseDataSensor1.intData();

  if (Firebase.getString(FirebaseDataSensor2, "/Sensor/Temperature"))
    temp = FirebaseDataSensor2.intData();

  if (Firebase.getString(FirebaseDataSensor3, "/Sensor/Bright"))
    light = FirebaseDataSensor3.intData();

  Serial.print("Nhiệt độ: ");
  Serial.print(temp);
  Serial.print(" Độ ẩm: ");
  Serial.print(humi);
  Serial.print(" Ánh sáng: ");
  Serial.println(light); // println để xuống dòng

  unsigned long currentMillis = millis();
  if (currentMillis - lastDisplayTime > displayInterval) {
    lastDisplayTime = currentMillis;

    lcd.clear();
    lcd.setCursor(0, 0);

    if (displayState == 0) {
      // Hiển thị nhiệt độ và độ ẩm
      lcd.print("Temperature:");
      lcd.print(temp);
      lcd.print("oC");

      lcd.setCursor(0, 1);
      lcd.print("Humidity:");
      lcd.print(humi);
      lcd.print("%");
    } else {
      // Hiển thị ánh sáng
      lcd.print("Bright:");
      lcd.print(light);
      lcd.print("lux");
    }

    // Đổi trạng thái cho lần sau
    displayState = 1 - displayState;
  }
}

//......................................................ÁP DÒNG.......................................
void measureAndUploadVoltageCurrent() {
  float volt_fan = voltageSensor_fan.getRmsVoltage();
  float mA_fan = averageCurrent(ACS_fan, calibration_factor_f);
  Serial.print("Ap_fan: ");
  Serial.print(volt_fan);
  Serial.print(" V");
  Serial.print("  mA_fan : ");
  Serial.println(mA_fan);

  float volt_bulb = voltageSensor_bulb.getRmsVoltage();
  float mA_bulb = averageCurrent(ACS_bulb, calibration_factor_b);
  Serial.print("Ap_Bulb: ");
  Serial.print(volt_bulb);
  Serial.print(" V");
  Serial.print("  mA_Bulb: ");
  Serial.println(mA_bulb);

  float volt_pump = voltageSensor_pump.getRmsVoltage();
  float mA_pump = averageCurrent(ACS_pump, calibration_factor_p);
  Serial.print("Ap_pump: ");
  Serial.print(volt_pump);
  Serial.print(" V");
  Serial.print("  mA_pump: ");
  Serial.println(mA_pump);

  Firebase.setInt(FirebaseDataVA1, "/VoltAmpe/Fan/Volt", volt_fan);
  Firebase.setInt(FirebaseDataVA2, "/VoltAmpe/Fan/Ampe", mA_fan);
  Firebase.setInt(FirebaseDataVA3, "/VoltAmpe/Bulb/Volt", volt_bulb);
  Firebase.setInt(FirebaseDataVA1, "/VoltAmpe/Bulb/Ampe", mA_bulb);
  Firebase.setInt(FirebaseDataVA2, "/VoltAmpe/Pump/Volt", volt_pump);
  Firebase.setInt(FirebaseDataVA3, "/VoltAmpe/Pump/Ampe", mA_pump);
}

float averageCurrent(ACS712 &acs, int calibration_factor) {
  float avg = 0;
  for (int i = 0; i < 100; i++) {
    avg += acs.mA_AC();
  }
  float mA = (abs(avg / 100.0) - calibration_factor);
  if (mA <= 5) mA = 0;
  return mA;
}

#include "ACS712.h"
#include <Wire.h>
#include <FirebaseESP32.h>
#include <addons/RTDBHelper.h>
#include <WiFiManager.h>
#include <ZMPT101B.h>
#include <LiquidCrystal_I2C.h>

//define device
#define fan 4
#define pump 2
#define bulb 5
//define FireBase
#define DATABASE_URL "project1-ee980-default-rtdb.firebaseio.com"
#define API_KEY "AIzaSyBgG9M6LrqzQEngs0Xt6A5QDg5LS-UzN2A"

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

//define đo áp dòng
#define ACTUAL_VOLTAGE 220.0f // Change this based on actual voltage

//----------------------------------------------đo dòng ---------------------------------------------
ACS712  ACS_bulb(35, 3.3, 4095, 66.0);
ACS712  ACS_fan(36, 3.3, 4095, 66.0);
ACS712  ACS_pump(32, 3.3, 4095, 66.0);
//------------------------------------------------đo áp---------------------------------------------
ZMPT101B voltageSensor_bulb(34, 50.0);
ZMPT101B voltageSensor_pump(33, 50.0);
ZMPT101B voltageSensor_fan(39, 50.0);
int lcdColumns =16 ;
int lcdRows = 2;
LiquidCrystal_I2C lcd (0x27, lcdColumns,lcdRows);

int calibration_factor_b =30;
int calibration_factor_f =30;
int calibration_factor_p =30;
void setup(){

  Serial.begin(115200);
//....................Pinmode Device................
  pinMode(pump, OUTPUT);
  pinMode(fan, OUTPUT);
  pinMode(bulb, OUTPUT);
//.....................đo áp dòng...................................
  ACS_fan.autoMidPoint();
  ACS_bulb.autoMidPoint();
  ACS_pump.autoMidPoint();

  lcd.init();
  // turn on LCD backlight
  lcd.backlight();


//--------------------------------------------Setup wifi----------------------------------------------------//
    WiFiManager wm;
    //wm.resetSettings();   //Xác định việc có cài đặt lại wifi hay không
    bool res;
    res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi
        Serial.println("WIFI connected");
    }

//----------------------------------------------Firebase----------------------------------------------------//
    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION); //Khởi tạo kết nối tới Firebase
    config.database_url = DATABASE_URL;
    config.signer.test_mode = true;
    Firebase.reconnectWiFi(true);
    Firebase.begin(&config, &auth);

}

void loop(){
//........................................ KIỂM TRA HOẠT ĐỘNG........................................
String ESP_status;
  if (Firebase.getString(FirebaseData3, "/CheckMCU/esp32")) {
    ESP_status = FirebaseData3.stringData();
    if (ESP_status == "OFF") {
      Serial.println(ESP_status);
      ESP_status = "ON";
      Serial.println(ESP_status);
    }
    Firebase.setString(FirebaseData3, "/CheckMCU/esp32", ESP_status);
  } else {
    Serial.println(FirebaseData3.errorReason());
  }



  //-----------------------------------------HIỂN THỊ LCD
  int humi_value, temp_value;
  int light_value;
  Firebase.getString(FirebaseDataSensor1, "/Sensor/Humidity");
  humi_value = FirebaseDataSensor1.intData();
  Firebase.getString(FirebaseDataSensor2, "/Sensor/Temperature");
  temp_value = FirebaseDataSensor2.intData();
  Firebase.getString(FirebaseDataSensor3, "/Sensor/Bright");
  light_value = FirebaseDataSensor3.intData();
  String temp, humi, light;

  temp = String(temp_value);
  humi = String(humi_value);
  light = String(light_value);


  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Humidity:");
  lcd.print(humi);
  lcd.print(" %");

  lcd.setCursor(0, 0);
  lcd.print("Temperature:");
  lcd.print(temp);
  lcd.print(" oC");
  delay(300);


  //........................................ĐO ÁP DÒNG........................................
  //........................................fan...................................................
  float voltageNow_fan = voltageSensor_fan.getRmsVoltage();
  Serial.print("Ap_fan: ");
  Serial.print(voltageNow_fan);
  Serial.print(" V");
  float average_fan = 0;
  for (int i = 0; i < 100; i++)
  {
    average_fan += ACS_fan.mA_AC();
  }
  float mA_fan = (abs(average_fan / 100.0) - calibration_factor_f);
  if (mA_fan <=5) mA_fan=0;
  Serial.print("  mA_fan : ");
  Serial.println(mA_fan);
  delay(500);

  //----------------------------------------------------Bulb-----------------------------------------------
  float voltageNow_bulb = voltageSensor_bulb.getRmsVoltage();
  Serial.print("Ap_Bulb: ");
  Serial.print(voltageNow_bulb);
  Serial.print(" V");
  float average_bulb = 0;
  for (int i = 0; i < 100; i++)
  {
    average_bulb += ACS_bulb.mA_AC();
  }
  float mA_bulb = (abs(average_bulb / 100.0) - calibration_factor_b);
  if (mA_bulb <=5) mA_bulb=0;
  Serial.print("  mA_Bulb: ");
  Serial.println(mA_bulb);
  delay(500);


  //---------------------------------------------------Pump---------------------------------------------------
  float voltageNow_pump = voltageSensor_pump.getRmsVoltage();
  Serial.print("Ap_pump: ");
  Serial.print(voltageNow_pump);
  Serial.print(" V");
  float average_pump = 0;
  for (int i = 0; i < 100; i++)
  {
    average_pump += ACS_pump.mA_AC();
  }
  float mA_pump = (abs(average_pump / 100.0) - calibration_factor_p);
  if (mA_pump <=5) mA_pump=0;
  Serial.print("  mA_pump: ");
  Serial.println(mA_pump);
  delay(500);


//........................................UPLOAD........................................
  Firebase.setInt(FirebaseDataVA1, "/VoltAmpe/Fan/Volt", voltageNow_fan);
  delay(500);

  Firebase.setInt(FirebaseDataVA2, "/VoltAmpe/Fan/Ampe", mA_fan);
  delay(500);

  Firebase.setInt(FirebaseDataVA3, "/VoltAmpe/Bulb/Volt", voltageNow_bulb);
  delay(500);

  Firebase.setInt(FirebaseDataVA1, "/VoltAmpe/Bulb/Ampe", mA_bulb);
  delay(500);

  Firebase.setInt(FirebaseDataVA2, "/VoltAmpe/Pump/Volt", voltageNow_pump);
  delay(500);

  Firebase.setInt(FirebaseDataVA3, "/VoltAmpe/Pump/Ampe", mA_pump);
  delay(500);


//........................................hiển thị ánh sáng........................................
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print("Bright: ");
  lcd.print(light);
  lcd.print(" lux");
  delay(300);


//........................................ ĐIỀU KHIỂN THIẾT BỊ........................................
  static String pump_web, bulb_web,fan_web;
  Firebase.getString(FirebaseData3, "/Device/Pump");
  pump_web = FirebaseData3.stringData();
  Firebase.getString(FirebaseData4, "/Device/Bulb");
  bulb_web = FirebaseData4.stringData();
  Firebase.getString(FirebaseData5, "/Device/Fan");
  fan_web = FirebaseData5.stringData();

  if (bulb_web == "ON") {
    digitalWrite(bulb,HIGH);
  }
  else{
    digitalWrite(bulb,LOW);
  }

  if (fan_web == "ON") {
    digitalWrite(fan,HIGH);
  }
  else{
    digitalWrite(fan,LOW);
  }

  if (pump_web == "ON") {
    digitalWrite(pump,HIGH);
  }
  else{
    digitalWrite(pump,LOW);
  }
}



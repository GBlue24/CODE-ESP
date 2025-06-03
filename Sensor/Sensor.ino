#include <BH1750.h>
#include <Wire.h>
#include <DHT.h>
#include <FirebaseESP32.h>
#include <addons/RTDBHelper.h>
#include <WiFiManager.h>
#include "DHT.h"

//--------------------------------------------------Sensor--------------------------------------------------------//
#define DHTPIN 19
#define DHTTYPE DHT11

BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

#define DATABASE_URL "project1-ee980-default-rtdb.firebaseio.com"
#define API_KEY "AIzaSyBgG9M6LrqzQEngs0Xt6A5QDg5LS-UzN2A"

// FirebaseData FirebaseData; //Khai báo object fbdo để lưu trữ dữ liệu Firebase
// FirebaseData FirebaseData2; //Khai báo object fbdo để lưu trữ dữ liệu Firebase
FirebaseData fbdo;

FirebaseAuth auth; //Khai báo object auth để xác thực với Firebase
FirebaseConfig config;
void setup() {
  Serial.begin(115200);
  dht.begin();

  
  Wire.begin();
  lightMeter.begin();
  WiFiManager wm;
       //Xác định việc có cài đặt lại wifi hay không
    bool res;
    res = wm.autoConnect("espcambien","password"); // password protected ap
    if(!res) {
        Serial.println("Failed to connect");
        wm.resetSettings();
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }
//----------------------------------------------Firebase----------------------------------------------------//   
    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION); //Khởi tạo kết nối tới Firebase
    config.database_url = DATABASE_URL;
    config.signer.test_mode = true;
    Firebase.reconnectWiFi(true);
    Firebase.begin(&config, &auth);
}

void loop() {
String ESP_ENV_status;

  if (Firebase.getString(fbdo, "/CheckMCU/esp32_env")) {
    ESP_ENV_status = fbdo.stringData();
    if (ESP_ENV_status == "OFF") {
      Serial.println(ESP_ENV_status);
      ESP_ENV_status = "ON";
      Serial.println(ESP_ENV_status);
    }
    Firebase.setString(fbdo, "/CheckMCU/esp32_env", ESP_ENV_status);
  } else {
    Serial.println(fbdo.errorReason());
  }

  int t = dht.readTemperature(); //Đọc nhiệt độ từ DHT11
  int h = dht.readHumidity(); //Đọc độ ẩm từ DHT11
  int lux = lightMeter.readLightLevel(); //đọc giá trị ánh sáng từ BH1750
  Serial.print("nhiệt độ:");
  Serial.println(t);
  Serial.print("độ ẩm:");
  Serial.println(h);
  Serial.print("ánh sáng:");
  Serial.println(lux);
  Firebase.setInt(fbdo, "/Sensor/Temperature", t);
  delay(400);
  Firebase.setInt(fbdo, "/Sensor/Humidity", h);
  delay(400);
  Firebase.setInt(fbdo, "/Sensor/Bright", lux);
  delay(400);
}
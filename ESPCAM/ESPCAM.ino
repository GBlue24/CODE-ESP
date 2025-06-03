#include "Arduino.h"
#include "WiFi.h"
#include "esp_camera.h"
#include <WiFiManager.h>
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Insert Firebase project API Key
#define API_KEY "AIzaSyBgG9M6LrqzQEngs0Xt6A5QDg5LS-UzN2A"
#define DATABASE_URL "https://project1-ee980-default-rtdb.firebaseio.com/"
// Insert Firebase storage bucket ID e.g bucket-name.appspot.com
#define STORAGE_BUCKET_ID "project1-ee980.appspot.com"

// Photo File Name to save in LittleFS
#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO "/data/photo.jpg"

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

unsigned long previousMillis = 0; // Stores last time a photo was taken
int captureInterval;      // Initial capture interval (30 seconds)
int captureTime;
bool captureNow;

// Define Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

void fcsUploadCallback(FCS_UploadStatusInfo info);

bool taskCompleted = false;

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS() {
  camera_fb_t* fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }
    
  // Take a new photo
  fb = NULL;  
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }  

  // Photo file name
  Serial.printf("Picture file name: %s\n", FILE_PHOTO_PATH);
  File file = LittleFS.open(FILE_PHOTO_PATH, FILE_WRITE);

  // Insert the data in the photo file
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.print("The picture has been saved in ");
    Serial.print(FILE_PHOTO_PATH);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
}

void initWiFi() {
  WiFiManager wm;
  bool res;
  res = wm.autoConnect("AutoConnectESPCam","password"); // password protected ap
  if (!res) {
    Serial.println("Failed to connect");
    wm.resetSettings();
  } else {
    // if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
  }
}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  } else {
    delay(500);
    Serial.println("LittleFS mounted successfully");
  }
}

void initCamera() {
  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  } 
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  initWiFi();
  initLittleFS();
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  initCamera();

  // Firebase
  // Assign the api key
  configF.api_key = API_KEY;
  configF.database_url = DATABASE_URL;
  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  // Assign the callback function for the long running token generation task
  configF.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {

//........................................ KIỂM TRA HOẠT ĐỘNG........................................
String path = "/CheckMCU/camera";
String camStatus;

  // 🔽 Lấy giá trị từ Firebase
  if (Firebase.RTDB.getString(&fbdo, path)) {
    camStatus = fbdo.stringData();
    Serial.print("📥 Trạng thái hiện tại: ");
    Serial.println(camStatus);

    if (camStatus == "OFF") {
      // 🔼 Gửi ngược lại giá trị "ON"
      if (Firebase.RTDB.setString(&fbdo, path, "ON")) {
        Serial.println("✅ Đã gửi 'ON' lên Firebase");
      } else {
        Serial.print("❌ Gửi thất bại: ");
        Serial.println(fbdo.errorReason());
      }
    }
  } else {
    Serial.print("❌ Lỗi khi đọc dữ liệu: ");
    Serial.println(fbdo.errorReason());
  }

  delay(3000);  // Chờ 3s rồi đọc lại



  bool getCapTime = Firebase.RTDB.getInt(&fbdo, F("/CaptureTime"), &captureTime);
  if (getCapTime) {
    captureInterval = captureTime * 1000;
    Serial.print(captureInterval);
  }

  if (Firebase.RTDB.getInt(&fbdo, F("/CaptureNow"), &captureNow)) {
      Serial.print(captureNow);
    // If there is a request, capture photo immediately and send to Firebase
    if(captureNow == 1) {
      capturePhotoSaveLittleFS();
    // Clear the processed request
      Firebase.RTDB.setInt(&fbdo, F("/CaptureNow"), 0);
      uploadPhoto();
    }
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= captureInterval) {
    previousMillis = currentMillis;
    capturePhotoSaveLittleFS();
    uploadPhoto();
  }
}

void uploadPhoto() {
  if (Firebase.ready() && !taskCompleted) {
    taskCompleted = true;
    Serial.print("Uploading picture... ");
    if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, FILE_PHOTO_PATH, mem_storage_type_flash, BUCKET_PHOTO, "image/jpeg", fcsUploadCallback)) {
      Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
    } else {
      Serial.println(fbdo.errorReason());
      taskCompleted = false;
    }
  }
}

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info) {
  if (info.status == firebase_fcs_upload_status_init) {
    Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
  } else if (info.status == firebase_fcs_upload_status_upload) {
    Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
  } else if (info.status == firebase_fcs_upload_status_complete) {
    Serial.println("Upload completed\n");
    FileMetaInfo meta = fbdo.metaData();
    Serial.printf("Name: %s\n", meta.name.c_str());
    Serial.printf("Bucket: %s\n", meta.bucket.c_str());
    Serial.printf("contentType: %s\n", meta.contentType.c_str());
    Serial.printf("Size: %d\n", meta.size);
    Serial.printf("Generation: %lu\n", meta.generation);
    Serial.printf("Metageneration: %lu\n", meta.metageneration);
    Serial.printf("ETag: %s\n", meta.etag.c_str());
    Serial.printf("CRC32: %s\n", meta.crc32.c_str());
    Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
    Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
    taskCompleted = false;
  } else if (info.status == firebase_fcs_upload_status_error) {
    Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    taskCompleted = false;
  }
}

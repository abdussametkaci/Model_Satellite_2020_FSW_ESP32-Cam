#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "esp_camera.h"

#include "FS.h"
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"

#define CAMERA_MODEL_AI_THINKER // we will use camera model
#include "cam_pins.h"

const char* ssid = "Talia";
const char* password = "12345678";

AsyncWebServer server(80);
camera_fb_t * frame = NULL; // camera object
File UploadFile;
File dataFile;
File fpassword;

String telemetry = "";
String gelenData = "";

bool isO = false;
bool isOK = false;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(9600);


  if (!initCamera()) {
    //Serial.printf("Failed to initialize camera...");
    //return;
  }

  if (!SD_MMC.begin()) {
    //Serial.println("SD Card Mount Failed");
    //return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    //Serial.println("No SD Card attached");
    //return;
  }

  // SD card init
  fs::FS &fs = SD_MMC;
  if (!fs.exists("/data.csv")) {
    dataFile = fs.open("/data.csv", FILE_APPEND);
    dataFile.print("takim_no,paket_numarasi,gonderme_zamani,basinc,yukseklik,inis_hizi,sicaklik,pil_gerilimi,gps_latitude,gps_longitude,gps_altitude,uydu_statusu,pitch,roll,yaw,donus_sayisi,video_aktarim_bilgisi");
    dataFile.close();
  }

  //END SD card init

  WiFi.softAP(ssid, password);

  //Serial.println("AP mode IP: ");
  //Serial.println(WiFi.softAPIP());

  server.on("/picture", HTTP_GET, [](AsyncWebServerRequest * request) {

    // burada while ile donup stream olarak video atamıyoruz
    if (frame)
      //return the frame buffer back to the driver for reuse
      esp_camera_fb_return(frame);

    frame = esp_camera_fb_get();

    request->send_P(200, "image/jpeg", (const uint8_t *)frame->buf, frame->len);

  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest * request) {

    request->send_P(200, "text/plain", telemetry.c_str());

  });

  server.on("/file", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200);

  }, handleUpload);

  server.on(
    "/command",
    HTTP_POST,
  [](AsyncWebServerRequest * request) {
    request->send(200);
  },
  NULL,
  [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    //request->send(200); // karsiya hemen istek atilmali yoksa server iste hemen alamadigi icin resetleniyor
    /*
      for (size_t i = 0; i < len; i++) {
      Serial.write(data[i]);
      }
    */

    Serial.write(data[0]);

  });

  server.on(
    "/password",
    HTTP_POST,
  [](AsyncWebServerRequest * request) {
    request->send(200);
  },
  NULL,
  [](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index) {
      fs::FS &fs = SD_MMC;
      fpassword = fs.open("password.txt", FILE_WRITE);
    }

    if (fpassword) {
      fpassword.write(data, len);
      fpassword.close();
      Serial.print('p');
    }

  });

  server.begin();
}

void loop() {

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'c') {
      fs::FS &fs = SD_MMC;
      if (!fs.exists("/password.txt")) {
        Serial.print('p');
      }
    }

    if (isOK) {
      //eğer durum ok ise data okumaya başla
      readData(c);
    }
    else {
      //eğer durum ok değil ise arduinodan gelen ok değerine bakarak durumu değiştir.
      if (isO == false) {
        if (c == 'O') {
          isO = true;
        }
      }
      else {
        if (c == 'K') {
          isOK = true;
        }
      }
    }

  }

}

void readData(char c) {
  if (c != '>') {
    gelenData += String(c);

  } else if (c == '>') {
    telemetry = gelenData;
    fs::FS &fs = SD_MMC;
    dataFile = fs.open("/data.csv", FILE_APPEND);
    dataFile.print(telemetry);
    dataFile.close();
    gelenData = "";
    isO = false;
    isOK = false;
  }
}


void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    fs::FS &fs = SD_MMC;
    String f = "/" + filename;
    UploadFile = fs.open(f.c_str(), FILE_WRITE); // calismaz ise basina / koymayi dene !!!
    //Serial.printf("UploadStart: %s\n", filename.c_str());
  }
  /*
    for(size_t i=0; i<len; i++){
    Serial.write(data[i]);
    }
  */
  if (UploadFile) UploadFile.write(data, len);

  if (final) {
    if (UploadFile)         // If the file was successfully created
    {
      UploadFile.close();   // Close the file again
      //Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);

      //request->send_P(200, "text/plain", "File was uploaded succesfully");
      Serial.print('1'); // video iletim bilgisi Evet arduinoya gönderilir
    }
  }
}

bool initCamera() {

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
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_err_t result = esp_camera_init(&config);

  if (result != ESP_OK) {
    return false;
  }

  return true;
}

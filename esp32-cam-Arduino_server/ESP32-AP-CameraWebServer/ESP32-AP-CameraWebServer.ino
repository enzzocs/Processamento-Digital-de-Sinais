/**************************************************************************************** 
 ESP32-CAM Camera Web Server + Access Point Wifi

 Para visualizar as imagens, acesse http://192.168.4.1 (não é https)

 Para ver o log, conecte via serial (Monitor Serial no Arduino IDE) com velocidade de 115200 bps
   
 IMPORTANTE!!! Quando for gravar na placa usando o Arduino IDE:
  - Em Ferramentas/Placa selecione "ESP32 Wrover Module"
  - Em Partition Scheme selecione "Huge APP (3MB No OTA)"
  - Se estiver usando um conversor serial-USB separado, 
       para carregar o código é necessário que o GPIO 0 esteja conectado ao GND
       Se usar ESP32-CAM MB (Motherboard), não precisa ou pode pressionar o botão IO0
  
 Informações em 
 https://randomnerdtutorials.com/esp32-cam-access-point-ap-web-server/
 https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/
 https://randomnerdtutorials.com/esp32-cam-troubleshooting-guide/
 https://dronebotworkshop.com/esp32-cam-intro/
 
***************************************************************************************/

#include "esp_camera.h"
#include <WiFi.h>

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//

// Select camera model
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#include "camera_pins.h"

const char* ssid = "Enzzo ESP32-CAM AP"; // Aqui voce pode escolher outro nome e senha
const char* password = "12344321";

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.softAP(ssid, password);

  startCameraServer();

  Serial.println("Camera Ready! Use 'http://192.168.4.1' to connect");
}

void loop() {
  Serial.println("ESP32-CAM Access Point. Connect at http://192.168.4.1"); 
  delay(10000);
}

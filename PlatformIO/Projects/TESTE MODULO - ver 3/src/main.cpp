#include "esp_camera.h"
#include <WiFi.h>

// ======== CONFIGURAÇÕES DA REDE WiFi ========
const char* ssid = "OFFICE";
const char* password = "Pass_0ffice";

// ======== CONFIGURAÇÃO DO IP FIXO ========
IPAddress local_IP(192, 168, 0, 150);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 252, 0);

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  // ======== CONFIGURAÇÃO DOS PINOS DO MÓDULO AI THINKER ========
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // ======== INICIALIZA A CÂMERA ========
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Erro ao iniciar a câmera: 0x%x", err);
    return;
  }

  // ======== CONECTA AO WiFi ========
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  // ======== INICIA O SERVIDOR WEB ========
  startCameraServer();

  Serial.println("Servidor iniciado!");
  Serial.print("Acesse: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(10000);
}
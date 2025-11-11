#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"

// ======== CONFIGURAÇÕES DA SUA REDE Wi-Fi ========
const char* ssid = "SUPERVISAO";
const char* password = "Pass_v1sitantePr0";
// =================================================

// Pinos do modelo AI Thinker ESP32-CAM
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

httpd_handle_t stream_httpd = NULL;
unsigned long lastGoodFrame = 0;  // para detectar travamentos

// =============================
// Reinicia o ESP32 se travar
// =============================
void safeRestart(const char* motivo) {
  Serial.println();
  Serial.println("======================================");
  Serial.printf("⚠️  Reiniciando ESP32: %s\n", motivo);
  Serial.println("======================================");
  delay(1000);
  ESP.restart();
}

// =============================
// Handler do streaming
// =============================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char * part_buf[64];
  
  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if(res != ESP_OK){
    safeRestart("Erro ao iniciar stream HTTP");
    return res;
  }

  while(true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Falha na captura da câmera");
      safeRestart("Falha ao capturar frame");
      return ESP_FAIL;
    }

    res = httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n"));
    if(res != ESP_OK) break;

    sprintf((char *)part_buf, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned int)fb->len);
    res = httpd_resp_send_chunk(req, (const char *)part_buf, strlen((char *)part_buf));
    if(res != ESP_OK) break;

    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if(res != ESP_OK) break;

    res = httpd_resp_send_chunk(req, "\r\n", strlen("\r\n"));
    esp_camera_fb_return(fb);
    fb = NULL;

    if(res != ESP_OK){
      Serial.println("⚠️ Erro de transmissão, reiniciando...");
      safeRestart("Erro de transmissão HTTP");
      break;
    }

    lastGoodFrame = millis();
    delay(30);  // controla taxa de quadros (~30ms = 30 fps aprox)
  }

  return res;
}

// =============================
// Servidor HTTP
// =============================
void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
  }
}

// =============================
// Setup principal
// =============================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(2000); // tempo pra estabilizar energia

  // Configuração da câmera
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
  config.frame_size = FRAMESIZE_QVGA; // mais fluido
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 15; // 10 melhor qualidade, 30 mais rápido
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Falha ao inicializar a câmera: 0x%x\n", err);
    safeRestart("Erro de inicialização da câmera");
  }

  // Conexão Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - wifiStart > 20000) safeRestart("Timeout Wi-Fi");
  }

  Serial.println();
  Serial.println("✅ WiFi conectado!");
  Serial.print("📶 Endereço IP: ");
  Serial.println(WiFi.localIP());

  // Inicia o servidor
  startCameraServer();
  Serial.println("🚀 Servidor iniciado!");
  Serial.println("Acesse no navegador:");
  Serial.printf("👉 http://%s/\n", WiFi.localIP().toString().c_str());

  lastGoodFrame = millis();
}

// =============================
// Loop principal com watchdog
// =============================
void loop() {
  // Se ficar mais de 5 segundos sem frames, reinicia
  if (millis() - lastGoodFrame > 5000) {
    safeRestart("Sem frames recentes — reiniciando...");
  }
  delay(10);
}

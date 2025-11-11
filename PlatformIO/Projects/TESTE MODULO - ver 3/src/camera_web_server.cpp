#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"

#define LED_PIN 4  // Flash da ESP32-CAM AI Thinker
bool flashState = false;

httpd_handle_t stream_httpd = NULL;

// =============================
// Função de stream contínuo
// =============================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Erro ao capturar frame!");
      continue;
    }

    res = httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n"));
    if (res != ESP_OK) break;

    snprintf(part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    res = httpd_resp_send_chunk(req, part_buf, strlen(part_buf));
    if (res != ESP_OK) break;

    res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res != ESP_OK) break;

    res = httpd_resp_send_chunk(req, "\r\n", 2);
    if (res != ESP_OK) break;

    esp_camera_fb_return(fb);
    delay(30);
  }

  return res;
}

// =============================
// Página HTML
// =============================
static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>ESP32-CAM Stream</title>
    <style>
      body { text-align:center; background-color:#101010; color:white; font-family:Arial; }
      img { width:90%; border-radius:10px; margin-top:10px; }
      button {
        background:#0078ff; color:white; padding:12px 24px; border:none;
        border-radius:10px; font-size:16px; cursor:pointer; margin-top:15px;
      }
      button.on { background:#00b400; }
      button:hover { opacity:0.8; }
    </style>
  </head>
  <body>
    <h2>ESP32-CAM - Live Stream</h2>
    <img src="/stream" id="stream">
    <br>
    <button id="flashBtn" onclick="toggleFlash()">Flash 🔦</button>

    <script>
      let flashOn = false;
      function toggleFlash(){
        fetch('/toggle_flash')
          .then(() => {
            flashOn = !flashOn;
            document.getElementById('flashBtn').classList.toggle('on', flashOn);
          });
      }
    </script>
  </body>
</html>
)rawliteral";

  httpd_resp_send(req, html, strlen(html));
  return ESP_OK;
}

// =============================
// Controle do flash via PWM (funciona com stream ativo)
// =============================
static esp_err_t toggle_flash_handler(httpd_req_t *req) {
  flashState = !flashState;

  if (flashState) {
    ledcAttachPin(LED_PIN, 4);       // Canal 4 do PWM
    ledcSetup(4, 5000, 8);           // 5kHz, resolução de 8 bits
    ledcWrite(4, 255);               // Brilho máximo (acende)
  } else {
    ledcWrite(4, 0);                 // Desliga LED
    ledcDetachPin(LED_PIN);
  }

  Serial.printf("Flash %s\n", flashState ? "ON" : "OFF");
  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// =============================
// Inicializa servidor
// =============================
void startCameraServer() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Apaga no início

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL };
  httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
  httpd_uri_t flash_uri = { .uri = "/toggle_flash", .method = HTTP_GET, .handler = toggle_flash_handler, .user_ctx = NULL };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &flash_uri);
  }

  Serial.println("📸 Servidor iniciado com controle de flash (PWM).");
}

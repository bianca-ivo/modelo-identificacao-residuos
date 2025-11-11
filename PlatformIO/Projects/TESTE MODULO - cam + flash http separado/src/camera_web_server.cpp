#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "driver/gpio.h"

#define LED_PIN 4  // pino do flash (GPIO4 no ESP32-CAM)
static bool flashState = false;
static httpd_handle_t stream_httpd = NULL;

// Muitos módulos ESP32-CAM têm flash ativo-baixo.
// Se o LED acender sozinho no boot, troque ACTIVE_LOW para false.
static const bool ACTIVE_LOW = false;

// Função utilitária: garante controle direto do pino
static void force_gpio_output_level(int pin, int level) {
  ledcDetachPin(pin);  // desanexa PWM se estiver ativo
  gpio_pad_select_gpio((gpio_num_t)pin);
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)pin, level);
}

// =========================
// Stream MJPEG contínuo
// =========================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  char part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) continue;

    if (httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n")) != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }

    int hlen = snprintf(part_buf, sizeof(part_buf),
                        "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)fb->len);
    if (httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }

    if (httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len) != ESP_OK) {
      esp_camera_fb_return(fb);
      break;
    }

    esp_camera_fb_return(fb);

    if (httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK) break;

    delay(30); // ajuste de FPS
  }

  return res;
}

// =========================
// Página HTML simples
// =========================
static esp_err_t index_handler(httpd_req_t *req) {
  const char* html = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>ESP32-CAM</title>
    <style>
      body { text-align:center; background:#111; color:#fff; font-family:Arial; }
      img { width:90%; max-width:640px; border-radius:8px; margin-top:12px; }
      button { margin-top:12px; padding:10px 20px; font-size:16px; border-radius:8px;
               border:none; background:#0078ff; color:#fff; cursor:pointer; }
      button.on { background:#00b400; }
    </style>
  </head>
  <body>
    <h2>ESP32-CAM</h2>
    <img id="stream">
    <script>
      const stream = document.getElementById('stream');
      stream.src = "http://192.168.0.150:81/stream"; // <-- IP fixo e porta do vídeo
    </script>
    <br>
    <button id="flashBtn" onclick="toggleFlash()">Flash</button>
    <script>
      let flashOn = false;
      function toggleFlash(){
        fetch('/toggle_flash').then(() => {
          flashOn = !flashOn;
          document.getElementById('flashBtn').classList.toggle('on', flashOn);
        });
      }
    </script>
  </body>
</html>
)rawliteral";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html, strlen(html));
  return ESP_OK;
}


// =========================
// Handler de toggle do flash
// =========================
static esp_err_t toggle_flash_handler(httpd_req_t *req) {
  flashState = !flashState;

  int phys_level = (flashState ? (ACTIVE_LOW ? 0 : 1)
                               : (ACTIVE_LOW ? 1 : 0));

  force_gpio_output_level(LED_PIN, phys_level);
  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// =========================
// Inicialização do servidor
// =========================
void startCameraServer() {
  int initial_level = (ACTIVE_LOW ? 1 : 0);
  force_gpio_output_level(LED_PIN, initial_level);
  flashState = false;

  // Servidor principal (porta 80)
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
  httpd_uri_t flash_uri = { .uri = "/toggle_flash", .method = HTTP_GET, .handler = toggle_flash_handler };

  httpd_handle_t main_httpd = NULL;
  if (httpd_start(&main_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(main_httpd, &index_uri);
    httpd_register_uri_handler(main_httpd, &flash_uri);
  }

  // Servidor de streaming (porta 81)
  httpd_handle_t stream_httpd2 = NULL;
  httpd_config_t config2 = HTTPD_DEFAULT_CONFIG();
  config2.server_port = 81;
  config2.ctrl_port = 32769; // evita conflito interno

  httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };

  if (httpd_start(&stream_httpd2, &config2) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd2, &stream_uri);
  }
}

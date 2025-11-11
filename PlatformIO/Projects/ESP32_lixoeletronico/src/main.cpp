#include "esp_camera.h"
#include <WiFi.h>

// ==== CONFIGURAÇÕES DA REDE ====
// Coloque aqui o nome e senha do seu Wi-Fi
const char* ssid = "OFFICE";
const char* password = "Pass_0ffice";

// IP fixo desejado
IPAddress local_IP(192, 168, 3, 21);
// Gateway (normalmente termina em .1)
IPAddress gateway(192, 168, 0, 254);
// Máscara de sub-rede
IPAddress subnet(255, 255, 252, 0);

void startCameraServer();

void setup() {
  Serial.begin(115200);

  // ==== CONFIGURAÇÃO DA CÂMERA ====
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
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // Inicializa a câmera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Erro ao iniciar câmera: 0x%x", err);
    return;
  }

  // ==== CONEXÃO COM WI-FI ====
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Falha ao configurar IP fixo");
  }
  WiFi.begin(ssid, password);

  Serial.println("Conectando ao Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Serial.println("Conectado!");
  Serial.print("Acesse a câmera em: ");
  Serial.println(WiFi.localIP());

  startCameraServer(); // inicia o servidor da câmera
}

void loop() {
  delay(10000);
}

// ==== Servidor da câmera ====
#include <WebServer.h>
WebServer server(80);

void handle_jpg_stream(void) {
  WiFiClient client = server.client();
  camera_fb_t * fb = NULL;
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) break;

    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    client.write(fb->buf, fb->len);
    client.write("\r\n");
    esp_camera_fb_return(fb);

    if (!client.connected()) break;
  }
}

void startCameraServer() {
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Location", "/stream");
    server.send(302, "text/plain", "");
  });
  server.on("/stream", HTTP_GET, handle_jpg_stream);
  server.begin();
}

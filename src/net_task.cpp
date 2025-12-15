#include <Arduino.h>
#include <WiFi.h>      // <-- isolated here

static TaskHandle_t s_netTaskHandle = nullptr;
static uint16_t s_port = 23;

// Store credentials safely (avoid dangling pointers)
static char s_ssid[64] = {0};
static char s_pass[64] = {0};

static void NetTask(void *pv) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(s_ssid, s_pass);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }

  WiFiServer server(s_port);
  server.begin();
  server.setNoDelay(true);

  WiFiClient client;

  for (;;) {
    if (!client || !client.connected()) {
      client = server.accept();   // new API (avoids deprecation warning)
    }

    if (client && client.connected()) {
      client.print("core0 alive\r\n");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void netTask_start(const char *ssid, const char *pass, uint16_t port) {
  if (s_netTaskHandle != nullptr) {
    return; // already started
  }
  s_port = port;

  // Copy creds (defensive)
  strncpy(s_ssid, ssid ? ssid : "", sizeof(s_ssid) - 1);
  strncpy(s_pass, pass ? pass : "", sizeof(s_pass) - 1);

  xTaskCreatePinnedToCore(
    NetTask,
    "NetTask",
    8192,          // stack bytes
    nullptr,
    1,             // modest priority
    &s_netTaskHandle,
    0              // core 0
  );
}

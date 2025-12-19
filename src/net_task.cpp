#include <Arduino.h>
#include <WiFi.h>

static TaskHandle_t s_netTaskHandle = nullptr;
static uint16_t s_port = 23;

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

  constexpr char kHeartbeat[] = "core0 alive\r\n";
  constexpr uint32_t HEARTBEAT_MS = 1000;
  constexpr uint32_t CONGESTION_KICK_MS = 5000; // if client can't accept data for this long, drop it

  uint32_t lastHb = 0;
  uint32_t congestedSince = 0;

  for (;;) {
    // If WiFi drops, close client and wait for reconnection
    if (WiFi.status() != WL_CONNECTED) {
      if (client) client.stop();
      congestedSince = 0;
      while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(250));
      }
    }

    // Accept a client if needed
    if (!client || !client.connected()) {
      client = server.accept();
      congestedSince = 0;
      lastHb = millis();
      if (client) {
        client.setNoDelay(true);
      }
    }

    // Periodic heartbeat, but NEVER block on write
    if (client && client.connected()) {
      uint32_t now = millis();
      if ((uint32_t)(now - lastHb) >= HEARTBEAT_MS) {
        const int need = (int)sizeof(kHeartbeat) - 1;
        int room = client.availableForWrite();   // key to avoiding blocking

        if (room >= need) {
          client.write((const uint8_t*)kHeartbeat, need);
          congestedSince = 0;
          lastHb = now;
        } else {
          // Client isn't keeping up: don't block, just track congestion
          if (congestedSince == 0) congestedSince = now;
          if ((uint32_t)(now - congestedSince) >= CONGESTION_KICK_MS) {
            client.stop(); // drop the slow/stalled client
            congestedSince = 0;
          }
          // still advance lastHb so we don't spin trying to send
          lastHb = now;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20)); // short service loop
  }
}

void netTask_start(const char *ssid, const char *pass, uint16_t port) {
  if (s_netTaskHandle != nullptr) return;

  s_port = port;
  strncpy(s_ssid, ssid ? ssid : "", sizeof(s_ssid) - 1);
  strncpy(s_pass, pass ? pass : "", sizeof(s_pass) - 1);

  xTaskCreatePinnedToCore(
    NetTask,
    "NetTask",
    8192,
    nullptr,
    1,
    &s_netTaskHandle,
    0
  );
}

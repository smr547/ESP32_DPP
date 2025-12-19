// net_task.cpp
//
// Core-0 networking task: WiFi STA + simple Telnet server.
//
// Key goals (ESP32 + Arduino WiFiClient quirks):
//  - Keep QP world (core 1) isolated from networking.
//  - Avoid calling flash/unsafe code from ISRs (handled elsewhere).
//  - Make Telnet reliable on “weird” networks (e.g., iPhone hotspot).
//
// IMPORTANT CHANGE vs earlier versions:
//  - Do NOT use availableForWrite() as a gating signal. On some stacks it can
//    report 0 for long periods even though writes succeed.
//  - Instead, attempt a small write and use its return value as the truth.
//  - Only "kick for congestion" if repeated writes return 0 for CONGESTION_KICK_MS.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

static TaskHandle_t s_netTaskHandle = nullptr;
static uint16_t     s_port          = 23;

static char s_ssid[64] = {0};
static char s_pass[64] = {0};

static void printWifiInfoOnce() {
  Serial.println();
  Serial.println("=== WiFi connected ===");
  Serial.print("SSID: ");     Serial.println(WiFi.SSID());
  Serial.print("Hostname: "); Serial.println(WiFi.getHostname());
  Serial.print("IP: ");       Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");  Serial.println(WiFi.gatewayIP());
  Serial.print("Netmask: ");  Serial.println(WiFi.subnetMask());
  Serial.print("RSSI: ");     Serial.println(WiFi.RSSI());
  Serial.print("MAC: ");      Serial.println(WiFi.macAddress());
  Serial.println("======================");
  Serial.println();
}

static void startMdnsOnce(uint16_t port) {
  // Optional. On iPhone hotspot it may or may not resolve from your laptop.
  if (MDNS.begin("esp32")) {
    MDNS.addService("telnet", "tcp", port);
    Serial.println("mDNS active: esp32.local");
  } else {
    Serial.println("mDNS failed");
  }
}

static void NetTask(void *pv) {
  (void)pv;

  // ---- WiFi bring-up ----
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-qpcore0");
  WiFi.begin(s_ssid, s_pass);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  printWifiInfoOnce();
  startMdnsOnce(s_port);

  // ---- Telnet server ----
  WiFiServer server(s_port);
  server.begin();
  server.setNoDelay(true);

  WiFiClient client;

  constexpr uint32_t HEARTBEAT_MS       = 1000;
  constexpr uint32_t CONGESTION_KICK_MS = 5000;

  uint32_t lastHb         = 0;
  uint32_t congestedSince = 0;
  bool     bannerSent     = false;

  const char *banner = "QPESP32 telnet ready\r\n";
  const char *hb     = "core0 alive\r\n";

  auto closeClient = [&]() {
    if (client) {
      client.stop();
    }
    lastHb = 0;
    congestedSince = 0;
    bannerSent = false;
  };

  // Attempt a small write; return bytes written. 0 means "couldn't write now"
  // (treat as backpressure / congestion). This avoids flaky availableForWrite().
  auto tryWrite = [&](const char *s) -> int {
    if (!client) return 0;
    const int need = (int)strlen(s);
    return (int)client.write((const uint8_t*)s, need);
  };

  for (;;) {
    // If WiFi drops, close client and wait for reconnection
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi dropped; closing telnet client");
      closeClient();
      while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(250));
      }
      printWifiInfoOnce();
      startMdnsOnce(s_port);
    }

    // Accept new client if present (Arduino-ESP32: available() renamed to accept()).
    WiFiClient newClient = server.accept();
    if (newClient) {
      closeClient();
      client = newClient;
      client.setNoDelay(true);

      // Reset state; banner will be sent by service section below.
      bannerSent = false;
      congestedSince = 0;
      lastHb = 0;

      Serial.println("Telnet client accepted");
    }

    // Service client (banner -> heartbeat)
    if (client) {
      uint32_t now = millis();

      // Stage 1: Send banner once.
      // Do NOT start congestion timer until after banner is successfully written.
      if (!bannerSent) {
        int n = tryWrite(banner);
        if (n > 0) {
          bannerSent = true;
          lastHb = now;            // start heartbeat schedule from here
          congestedSince = 0;
          Serial.println("Telnet banner sent");
        }
      }
      // Stage 2: periodic heartbeat; kick if we can't write for a long time.
      else {
        if ((uint32_t)(now - lastHb) >= HEARTBEAT_MS) {
          int n = tryWrite(hb);
          if (n > 0) {
            lastHb = now;
            congestedSince = 0;
          } else {
            if (congestedSince == 0) congestedSince = now;
            if ((uint32_t)(now - congestedSince) >= CONGESTION_KICK_MS) {
              Serial.println("Telnet client kicked (write stalled)");
              closeClient();
            }
            // IMPORTANT: don't advance lastHb; we want to retry soon.
          }
        }
      }

      // If peer disconnected, tidy up.
      // Note: connected() can be "late", but it's fine as a cleanup signal.
      if (client && !client.connected()) {
        Serial.println("Telnet client disconnected");
        closeClient();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
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
    0  // Core 0
  );
}

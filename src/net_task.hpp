#pragma once
#include <stdint.h>

// Start a minimal telnet/heartbeat server on core 0.
// ssid/pass are copied immediately, so literals are fine.
void netTask_start(const char* ssid, const char* pass, uint16_t port = 23);

#pragma once
#include <cstdint>
#include <cstddef>

struct PhiloStatusMsg {
  uint8_t n;
  char    stat[24];   // keep small; "thinking"/"hungry"/"eating"
};

void qp_sensesp_bridge_init();
bool qp_sensesp_push_philo(uint8_t n, const char* stat);

// Core0 consumer API
bool qp_sensesp_pop_philo(PhiloStatusMsg& out);

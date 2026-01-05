#include "qp_sensesp_bridge.hpp"

#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static QueueHandle_t s_philoQ = nullptr;

void qp_sensesp_bridge_init() {
  if (s_philoQ == nullptr) {
    // small queue; drop-on-full semantics
    s_philoQ = xQueueCreate(/*len*/ 16, sizeof(PhiloStatusMsg));
  }
}

bool qp_sensesp_push_philo(uint8_t n, const char* stat) {
  if (!s_philoQ || !stat) return false;

  PhiloStatusMsg m{};
  m.n = n;
  std::strncpy(m.stat, stat, sizeof(m.stat) - 1);
  m.stat[sizeof(m.stat) - 1] = '\0';

  // Non-blocking send. If full, we drop (don’t ever block QP core).
  return xQueueSendToBack(s_philoQ, &m, 0) == pdTRUE;
}

bool qp_sensesp_pop_philo(PhiloStatusMsg& out) {
  if (!s_philoQ) return false;
  return xQueueReceive(s_philoQ, &out, 0) == pdTRUE;
}

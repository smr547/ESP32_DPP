#include "sensesp_task.hpp"

#include <Arduino.h>
#include <sensesp.h>
#include <sensesp_app_builder.h>
#include <sensesp/signalk/signalk_output.h>   // SKOutputNumeric<int>

#include "dpp.hpp"                 // for N_PHILO (and possibly other DPP symbols)
#include "qp_sensesp_bridge.hpp"   // queue bridge

static TaskHandle_t s_sensespTaskHandle = nullptr;

static int map_philo_state(const char* stat) {
  if (!stat) return -1;

  // DPP uses "thinking", "hungry", "eating"
  // Keep it cheap: decide by first letter.
  switch (stat[0]) {
    case 't': return 0; // thinking
    case 'h': return 1; // hungry
    case 'e': return 2; // eating
    case 'p': return 3; // paused (if you ever add it)
    default:  return 9; // unknown
  }
}

static void sensespTask(void*) {

  auto* builder = (new sensesp::SensESPAppBuilder());

  auto app = builder->set_hostname("qp-sensesp-test")
                    ->set_wifi("Bertie", "Ookie1234")
                    ->set_sk_server("10.1.1.20", 3000)
                    ->enable_free_mem_sensor()
                    ->enable_uptime_sensor()
                    ->enable_ip_address_sensor()
                    ->enable_wifi_signal_sensor()
                    ->get_app();

  if (!app) { vTaskDelay(portMAX_DELAY); }

  // event_loop() is only valid after the app/base-app exists
  auto loop = sensesp::event_loop();
  if (!loop) { vTaskDelay(portMAX_DELAY); }

  // One numeric output per philosopher:
  // 0=thinking, 1=hungry, 2=eating, 9=unknown
  static sensesp::SKOutputNumeric<int>* philo_state_out[N_PHILO];
  static int last_state[N_PHILO];

  for (uint8_t i = 0; i < N_PHILO; ++i) {
    last_state[i] = -1000; // impossible sentinel

    char path[64];
    // SignalK path (pick whatever convention you like)
    // Example: environment.qp.dpp.philo.2.state
    snprintf(path, sizeof(path), "environment.qp.dpp.philo.%u.state",
             static_cast<unsigned>(i));

    philo_state_out[i] = new sensesp::SKOutputNumeric<int>(path);
  }

  app->start();

  PhiloStatusMsg m{};
  for (;;) {
    // Drain queue quickly, publish only on change
    while (qp_sensesp_pop_philo(m)) {
      if (m.n < N_PHILO) {
        const int s = map_philo_state(m.stat);
        if (s != last_state[m.n]) {
          last_state[m.n] = s;
          philo_state_out[m.n]->set_input(s);
        }
      }
    }

    loop->tick();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void startSensESPOnCore0() {
  constexpr BaseType_t CORE0 = 0;
  xTaskCreatePinnedToCore(
      sensespTask, "SensESP",
      16384,
      nullptr, tskIDLE_PRIORITY + 2,
      &s_sensespTaskHandle,
      CORE0
  );
}

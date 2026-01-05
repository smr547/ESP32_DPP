#include "sensesp_task.hpp"

#include <Arduino.h>
#include <sensesp.h>
#include <sensesp_app_builder.h>

static TaskHandle_t s_sensespTaskHandle = nullptr;
static char latest_stat[5][24] = {};  // N_PHILO=5 in DPP
static bool dirty[5] = {};

static void sensespTask(void*) {
    auto* builder = (new sensesp::SensESPAppBuilder());

    // IMPORTANT: finalize builder into an app FIRST
    auto app = builder->set_hostname("qp-sensesp-test")
                   ->set_wifi("Bertie", "Ookie1234")
                   ->set_sk_server("10.1.1.20", 3000)
                   ->enable_free_mem_sensor()
                   ->enable_uptime_sensor()
                   ->enable_ip_address_sensor()
                   ->enable_wifi_signal_sensor()
                   ->get_app();  // or whatever the finalize call is

    // Now the base app singleton exists, so event_loop() is valid
    auto loop = sensesp::event_loop();
    // (optional belt-and-braces)
    if (!app || !loop) {
        vTaskDelay(portMAX_DELAY);
    }

    app->start();

    for (;;) {
        PhiloStatusMsg m;
        while (qp_sensesp_pop_philo(m)) {
            if (m.n < 5) {
                std::strncpy(latest_stat[m.n], m.stat,
                             sizeof(latest_stat[m.n]) - 1);
                latest_stat[m.n][sizeof(latest_stat[m.n]) - 1] = '\0';
                dirty[m.n] = true;
            }
        }

        loop->tick();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void startSensESPOnCore0() {
    constexpr BaseType_t CORE0 = 0;
    xTaskCreatePinnedToCore(sensespTask, "SensESP",
                            16384,  // give it a bit more stack
                            nullptr, tskIDLE_PRIORITY + 2, &s_sensespTaskHandle,
                            CORE0);
}

// HealthAO.cpp
#include "HealthAO.hpp"

#include <Arduino.h>

extern "C" {
#include "driver/gpio.h"
#include "esp_task_wdt.h"
}

static constexpr gpio_num_t WDT_PULSE_GPIO =
    GPIO_NUM_26;  // choose your Saleae probe pin
// static constexpr uint32_t FEED_PERIOD_TICKS = 100U;
static constexpr QP::QTimeEvtCtr FEED_PERIOD_TICKS = 100U;  // safer starter
static TaskHandle_t s_healthTask = nullptr;

static void dumpTickContext(char const* where, void* obj, uint16_t sig) {
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    Serial.printf(
        "%s: this=%p sig=%u curTask=%p(%s) expectedTask=%p(%s) core=%d\n",
        where, obj, (unsigned)sig, (void*)cur, pcTaskGetName(cur),
        (void*)s_healthTask, (s_healthTask ? pcTaskGetName(s_healthTask) : "?"),
        xPortGetCoreID());
}

HealthAO::HealthAO()
    : QActive(Q_STATE_CAST(&HealthAO::initial)),
      m_tickEvt(this, HEALTH_TICK_SIG, 0U) {}

QP::QState HealthAO::initial(HealthAO* const me, QP::QEvt const* const) {
    // configure the probe pin

    Serial.printf("HealthAO::initial this=%p task=%p\n", (void*)me,
                  (void*)xTaskGetCurrentTaskHandle());
    gpio_config_t io{};
    io.intr_type = GPIO_INTR_DISABLE;
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = 1ULL << WDT_PULSE_GPIO;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io);
    gpio_set_level(WDT_PULSE_GPIO, 0);

    return Q_TRAN(&HealthAO::active);
}

QP::QState HealthAO::active(HealthAO* const me, QP::QEvt const* const e) {
    switch (e->sig) {

    case HEALTH_START_SIG: {
        // TaskHandle_t cur = xTaskGetCurrentTaskHandle();
        Serial.printf("HEALTH_START handled in %s core=%d\n",
              pcTaskGetName(xTaskGetCurrentTaskHandle()),
              xPortGetCoreID());

        esp_err_t r = esp_task_wdt_add(nullptr);   // add CURRENT task
        if (r != ESP_OK) {
            Serial.printf("WDT add failed: %d\n", (int)r);
            return Q_HANDLED();
        }
        Serial.println("WDT add OK (Health AO task)");

        // Arm periodic feed (safe starter until you confirm QP tick rate)
        static constexpr QP::QTimeEvtCtr FEED_PERIOD_TICKS = 100U;
        me->m_tickEvt.armX(FEED_PERIOD_TICKS, FEED_PERIOD_TICKS);

        return Q_HANDLED();
    }

    case HEALTH_TICK_SIG: {

        gpio_set_level(WDT_PULSE_GPIO, 1);
        esp_err_t r = esp_task_wdt_reset();  
        gpio_set_level(WDT_PULSE_GPIO, 0);      // reset CURRENT task
        if (r != ESP_OK) {
            TaskHandle_t cur = xTaskGetCurrentTaskHandle();
            Serial.printf("WDT reset failed: %d (task=%p %s core=%d)\n",
                          (int)r, (void*)cur, pcTaskGetName(cur), xPortGetCoreID());
        }
        return Q_HANDLED();
    }

    }
    return Q_SUPER(&QP::QHsm::top);
}


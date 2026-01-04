#include "bsp_hooks.hpp"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qpcpp.hpp"

#ifdef Q_SPY

static void QSpyUartDrainTask(void* /*pvParameters*/) {
    // If Serial hasn't been begun elsewhere, do it here (but do it only once
    // globally). Serial.begin(115200);

    for (;;) {
        // ---- TX: drain QS -> UART ----
        uint16_t len = Serial.availableForWrite();
        if (len > 0U) {
            uint8_t const* buf = QP::QS::getBlock(&len);
            if (buf != nullptr && len > 0U) {
                Serial.write(buf, len);
            }
        }

        // ---- RX (optional): UART -> QS ----
        // If you don't need QS-RX, comment this whole block out.
        int rxAvail = Serial.available();
        while (rxAvail-- > 0) {
            QP::QS::rxPut(static_cast<uint8_t>(Serial.read()));
        }
        if (Serial.available() > 0) {  // if we consumed at least one byte
            QP::QS::rxParse();
        }

        // Be nice: low duty-cycle drain
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void BSP_startQSUartDrain() {
    // Core 0 drain task, low priority
    xTaskCreatePinnedToCore(&QSpyUartDrainTask, "QS-UART",
                            4096,  // stack bytes (start modest; tune if needed)
                            nullptr,
                            tskIDLE_PRIORITY + 1,  // low priority
                            nullptr,
                            0  // Core 0
    );
}
#endif

void BSPHooks::onStartup() {
    QP::ESP32_tickHookInit();
#ifdef Q_SPY
    BSP_startQSUartDrain();
#endif
}

// HealthAO.hpp
#pragma once
#include "qpcpp.hpp"
#include "dpp.hpp"

/*
enum AppSignals {
  HEALTH_TICK_SIG = QP::Q_USER_SIG,
  // (optional) HEARTBEAT_SIG, etc
};
*/

class HealthAO : public QP::QActive {
   public:
    HealthAO();

   protected:
    static QP::QState initial(HealthAO* const me, QP::QEvt const* const e);
    static QP::QState active(HealthAO* const me, QP::QEvt const* const e);

   private:
    QP::QTimeEvt m_tickEvt;
};

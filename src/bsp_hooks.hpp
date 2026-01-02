#ifndef BSP_HOOKS_HPP
#define BSP_HOOKS_HPP

namespace BSPHooks {
  void onInit();       // serial, pins, etc
  void onStartup();    // tick init, any extra tasks
  void onIdle();       // optional
}

#endif // BSP_HOOKS_HPP

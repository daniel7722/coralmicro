// Copyright 2025 Your Majesty
// Licensed under the Apache License, Version 2.0
#include <cstdio>

#include "libs/base/led.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"

namespace coralmicro {
  namespace {
  // [start-sphinx-snippet:blink-led]
  [[noreturn]] void Main() {
    printf("Blink LED Example!\r\n");
    // Turn on Status LED to show the board is on.
    LedSet(Led::kStatus, true);
  
    bool on = true;
    while (true) {
      on = !on;
      printf("Switch");
      LedSet(Led::kUser, on);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
  // [end-sphinx-snippet:blink-led]
  }  // namespace
  }  // namespace coralmicro
  
  extern "C" void app_main(void *param) {
    (void)param;
    coralmicro::Main();
  }

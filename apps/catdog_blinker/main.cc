// Copyright 2025 Your Majesty
// Licensed under the Apache License, Version 2.0

#include <cstdio>
#include <vector>

#include "libs/base/led.h"
#include "libs/base/network.h"
#include "libs/base/mutex.h"
#include "libs/camera/camera.h"
#include "libs/libjpeg/jpeg.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"


namespace coralmicro {
  namespace {
  constexpr int kWidth = 324;
  constexpr int kHeight = 240;
  constexpr int kNetworkPort = 27000;
  constexpr uint8_t kMessageTypeSetup = 0;
  constexpr uint8_t kMessageTypeImageData = 1;
  
  [[noreturn]] void Main() {
    printf("Starting Camera USB stream...\r\n");
    LedSet(Led::kStatus, true);

    CameraTask::GetSingleton()->SetPower(false); 
    vTaskDelay(pdMS_TO_TICKS(100)); 
    CameraTask::GetSingleton()->SetPower(true); 
    CameraTask::GetSingleton()->Enable(CameraMode::kStreaming); 

    // create TCP connection
    const int server_socket = SocketServer(kNetworkPort, 1); 
    if (server_socket < 0) {
      printf("Error: Could not create server socket\r\n"); 
      vTaskSuspend(nullptr); 
    }

    while (true) {
      printf("Waiting for host connection..."); 
      int client_socket = SocketAccept(server_socket); 
      if (client_socket < 0) {
        printf("Error: Failed to accept client\r\n"); 
        continue; 
      }
      printf("Host connected!!"); 

      // send initial image size
      const uint8_t init_msg[] = {
        static_cast<uint8_t>(kWidth & 0xFF), static_cast<uint8_t>((kWidth >> 8) & 0xFF),
        static_cast<uint8_t>(kHeight & 0xFF), static_cast<uint8_t>((kHeight >> 8) & 0xFF)};
      WriteMessage(client_socket, kMessageTypeSetup, init_msg, sizeof(init_msg)); 

      std::vector<uint8_t> input(kWidth * kHeight * 3); 
      std::vector<uint8_t> jpeg(1024 * 70); 

      while (true) {
        CameraFrameFormat fmt{
          CameraFormat::kRgb, CameraFilterMethod::kBilinear,
          CameraRotation::k270, kWidth, kHeight,
          /*preserve_ratio=*/false, input.data()};

          if (!CameraTask::GetSingleton()->GetFrame({fmt})) {
            printf("Frame capture failed\r\n");
            break;
          }

          size_t jpeg_size = JpegCompressRgb(input.data(), kWidth, kHeight, 75, jpeg.data(), jpeg.size()); 
          WriteMessage(client_socket, kMessageTypeImageData, jpeg.data(), jpeg_size);

          vTaskDelay(pdMS_TO_TICKS(100));
      }
      SocketClose(client_socket);
    }
  }
  
  }  // namespace
  }  // namespace coralmicro
  
  extern "C" void app_main(void* param) {
    (void)param;
    coralmicro::Main();
  }
  
#include <cstdio>
#include <vector>

#include "libs/base/filesystem.h"
#include "libs/base/led.h"
#include "libs/base/main_freertos_m4.h"
#include "libs/camera/camera.h"
#include "libs/tensorflow/utils.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_error_reporter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_interpreter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "third_party/tflite-micro/tensorflow/lite/schema/schema_generated.h"

namespace coralmicro {
namespace {

// Basic model input size
constexpr int kInputWidth = 224;
constexpr int kInputHeight = 224;
constexpr int kInputChannels = 3;

// Model input path
constexpr char kModelPath[] = "/models/mobilenetv3_cats_vs_dogs.tflite";

// Tensor arena size
constexpr int kTensorArenaSize = 512 * 1024;
STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, kTensorArenaSize);

[[noreturn]] void Main() {
  printf("Starting MobileNetV3 Cat vs Dog Detector on M4 Core...\r\n");

  // Initialize camera
  CameraTask::GetSingleton()->Init(I2C5Handle());
  CameraTask::GetSingleton()->SetPower(true);
  CameraTask::GetSingleton()->Enable(CameraMode::kStreaming);

  // Load model
  std::vector<uint8_t> model_data;
  if (!LfsReadFile(kModelPath, &model_data)) {
    printf("Error: Failed to read model: %s\r\n", kModelPath);
    vTaskSuspend(nullptr);
  }
  auto model = tflite::GetModel(model_data.data());
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    printf("Model schema version mismatch!\r\n");
    vTaskSuspend(nullptr);
  }

  // Prepare TFLite Micro interpreter
  tflite::MicroErrorReporter error_reporter;
  tflite::MicroMutableOpResolver<5> resolver;
  resolver.AddConv2D();
  resolver.AddDepthwiseConv2D();
  resolver.AddFullyConnected();
  resolver.AddReshape();
  resolver.AddAveragePool2D();

  tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize, &error_reporter);

  if (interpreter.AllocateTensors() != kTfLiteOk) {
    printf("AllocateTensors failed\r\n");
    vTaskSuspend(nullptr);
  }

  auto* input_tensor = interpreter.input(0);

  while (true) {
    // Capture image into input tensor
    CameraFrameFormat fmt{CameraFormat::kRgb,
                          CameraFilterMethod::kBilinear,
                          CameraRotation::k270,
                          kInputWidth,
                          kInputHeight,
                          /*preserve_ratio=*/false,
                          tflite::GetTensorData<uint8>(input_tensor)};
    if (!CameraTask::GetSingleton()->GetFrame({fmt})) {
      printf("Failed to capture image\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Run inference
    if (interpreter.Invoke() != kTfLiteOk) {
      printf("Inference failed\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    auto* output_tensor = interpreter.output(0);
    float score = output_tensor->data.f[0];  // Sigmoid output

    printf("Model score: %.3f\r\n", score);

    // Cat if score < 0.5
    if (score < 0.5f) {
      LedSet(Led::kUser, true);  // Turn ON LED
    } else {
      LedSet(Led::kUser, false); // Turn OFF LED
    }

    vTaskDelay(pdMS_TO_TICKS(200));  // Slow down loop a little
  }
}

}  // namespace
}  // namespace coralmicro

extern "C" void app_main(void* param) {
  (void)param;
  coralmicro::Main();
}

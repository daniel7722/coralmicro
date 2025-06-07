#include "libs/base/filesystem.h"
#include "libs/base/gpio.h"
#include "libs/base/led.h"
#include "libs/base/mutex.h"
#include "libs/base/network.h"
#include "libs/base/reset.h"
#include "libs/base/strings.h"
#include "libs/base/watchdog.h"
#include "libs/camera/camera.h"
#include "libs/libjpeg/jpeg.h"
#include "libs/rpc/rpc_http_server.h"
#include "libs/tpu/edgetpu_manager.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/semphr.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/mjson/src/mjson.h"
#include "libs/tensorflow/utils.h" // This is important as posnet.h has include things that need to manually include it here

#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace coralmicro {
namespace {
constexpr float kThreshold = 0.4;

constexpr int kCmdStart = 1;
constexpr int kCmdStop = 2;
constexpr int kCmdProcess = 3;

constexpr int kNetworkPort = 27000;

constexpr int kMessageTypeSetup = 0;
constexpr int kMessageTypeImageData = 1;

constexpr int kTensorArenaSize = 1024 * 1024 * 2;
STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, kTensorArenaSize);
constexpr char kModelPath[] =
    "/models/"
    "mobilenetv3_cats_vs_dogs_edgetpu.tflite";
constexpr int kModelWidth = 324;
constexpr int kModelHeight = 324;
constexpr int kModelSize = kModelWidth * kModelHeight * /*depth*/ 3;

constexpr int kLogInterval = 15;
bool kIsSetup = false;


// This function extract byte from value
// value is templated, it can be any integral type or even custom types for which bute-level access makes sense
// i specifies which byte to extract from 
template <typename T>
constexpr uint8_t byte(T value, int i) {
    return static_cast<uint8_t>(value >> 8 * i);
}

// Using the byte function, it encode the useful information in binary form and send the message,
// here is the setup message, towards clients. 
void WriteMessageImageInfo(int fd) {
    constexpr uint8_t msg[] = {byte(kModelWidth, 0),  byte(kModelWidth, 1),
                               byte(kModelWidth, 2),  byte(kModelWidth, 3),
                               byte(kModelHeight, 0), byte(kModelHeight, 1),
                               byte(kModelHeight, 2), byte(kModelHeight, 3)};
    WriteMessage(fd, kMessageTypeSetup, msg, sizeof(msg));
    kIsSetup = true;
}

struct TaskMessage {
    int command;
    SemaphoreHandle_t completion_semaphore;
    void* data;
};

/**
 * @brief Base class for wrapping FreeRTOS tasks using CRTP.<unnamed>
 * 
 * This class template is designed to simplify FreeRTOS task creation
 * by using the Curiously Recurring Template Pattern (CRTP). The derived 
 * class must implement a 'Run()' methodd, which will be executed inside
 * the FreeRTOS task context.<unnamed>
 * 
 * Usage: 
 *      class MyTask: private Task<MyTask> {
 *        public: 
 *          MyTask() : Task("my_task", kAppTaskPriority) {}
 *          void Run(); // Must be implemented
 *      };
 * 
 * Internally, this class: 
 * - Calls xTaskCreate to start a new FreeRTOS task
 * - Passes 'this' pointer to the static entry point 'StaticRun'
 * - Casts the pointer back to the derived class and call 'Run()'
 * 
 * This avoids the need to manually define C-style function pointers or 
 * static globals when working with C++ classes in FreeRTOS.<unnamed>
 * @tparam Derived The class that inherits from Task<Derived>
 */
template <typename Derived>
class Task {
  public:
    Task(const char* name, int priority) {
        printf("Creating FreeRTOS task: %s\r\n", name); 
        CHECK(xTaskCreate(StaticRun, name, configMINIMAL_STACK_SIZE * 30, this,
                        priority, &task_) == pdPASS);
    }

  private:
    // This param is 'this' pointer
    static void StaticRun(void* param) {
        static_cast<Derived*>(param)->Run();
    }
    TaskHandle_t task_;
};

/**
 * NetowrkTask is responsible for managing the socket server 
 * It listens for client connections, maintains a socket for commnication, 
 * and sends data (like JPEG images or setup messages) to the connected client.
 * 
 * It inherits from a templated Task<> base class that encapsulates FreeRTOS task creation. 
 * Once instantiated, NetworkTask lauches its 'Run()' method on a dedicated FreeRTOS thread. 
 * 
 * Thread safety is ensured via a FreeRTOS mutex, allowing safe access to the socket 
 * from other tasks
 */
class NetworkTask : private Task<NetworkTask> {
  public:

    // Constructor: Initialises the FreeROTS task and creates the mutex for socket access
    NetworkTask()
        : Task("catdog_network_task", kAppTaskPriority),
        mutex_(xSemaphoreCreateMutex()) {
        CHECK(mutex_);
    }

    // Send() transmits a mssage to the connected clietn.
    // It is thread-safe thanks to the mutex and will silently return
    // if there's no active client or an error occurs (e.g., client sent unexpected input).
    void Send(uint8_t type, const void* bytes, size_t size) {
        // this mutex is designed as to acquire the mutex now and automatically release it
        // when the lock object goes out of scope.
        MutexLock lock(mutex_); 

        if (client_socket_ == -1) return;

        if (SocketHasPendingInput(client_socket_)) {
            printf("Pending input detected.Resetting client.\r\n");
            ResetClientSocket();
            return;
        }

        if (WriteMessage(client_socket_, type, bytes, size) != IOStatus::kOk)
            ResetClientSocket();
    }

    // just a test function
    [[nodiscard]] bool lottery() const {
        int num = rand() % 101;
        printf("Number is: %d\r\n", num); 
        return num > 90;
    }

    // Run() is the main loop for this task.
    // It accepts incoming TCP client connections and keeps them alive
    // until they disconnect or misbehave (e.g., send input unexpectedly).
    [[noreturn]] void Run() {
        int server_socket = SocketServer(kNetworkPort, /*backlog=*/5);
        while (true) {
            printf("INFO: Waiting for clients on %d...\r\n", kNetworkPort);
            const int client_socket = SocketAccept(server_socket);
            if (client_socket == -1) {
                printf("ERROR: Cannot accept client.\r\n");
                continue;
            }

            {
                MutexLock lock(mutex_);
                ResetClientSocket(client_socket);
                printf("INFO: Client #%d connected.\r\n", client_socket);
                WriteMessageImageInfo(client_socket);
            }
        }
    }

  private:
    SemaphoreHandle_t mutex_; // protects access to client_socket_
    int client_socket_ = -1; // file descriptor for the connected client

    // Resets the current client socket.
    // If passed -1 (default), it simply closes the current connection.
    void ResetClientSocket(int sockfd = -1) {
        if (client_socket_ != -1) SocketClose(client_socket_);
        client_socket_ = sockfd;
    }
};

class InferenceTask : private Task<InferenceTask> {
  public:
    explicit InferenceTask(NetworkTask* network_task_,
                           std::shared_ptr<tflite::MicroInterpreter>& interpreter)
      : Task("catdog_mobilenet_task", kAppTaskPriority),
        network_task_(network_task_),
        queue_(xQueueCreate(1, sizeof(char))),
        interpreter_(std::move(interpreter)),
        counter_(0) {
        CHECK(queue_);
        printf("InferenceTask created\r\n");
  }

    void Put(const std::vector<uint8_t>& frame) {
        if (uxQueueMessagesWaiting(queue_) == 0) {
            CHECK(frame.size() == kModelSize);
            std::memcpy(tflite::GetTensorData<uint8_t>(interpreter_->input(0)),
                        frame.data(), kModelSize);
            char cmd = 0;
            CHECK(xQueueSend(queue_, &cmd, portMAX_DELAY) == pdTRUE);
            printf("Put() ->enqueueed inference request\r\n");
        }
    }

    [[noreturn]] void Run() {
        while (true) {
            printf("InferenceTask::Run() entered\r\n");
            char cmd;
            CHECK(xQueuePeek(queue_, &cmd, portMAX_DELAY) == pdTRUE);
            counter_++;
            CHECK(interpreter_->Invoke() == kTfLiteOk);
            TfLiteTensor* output = interpreter_->output(0);
            uint8_t* scores = output->data.uint8;
            float scale = output->params.scale;
            int zero_point = output->params.zero_point;
            float cat_score = (scores[0] - zero_point) * scale;
            float dog_score = (scores[1] - zero_point) * scale;

            //   int predicted_class = (cat_score > dog_score) ? 0 : 1;
            //   const char* predicted_label = (predicted_class == 0) ? "cat" : "dog";

            if (counter_ % kLogInterval == 0 || true) {
                printf("cat_score: %.2f, dog_score: %.2f\r\n", cat_score, dog_score);
            }
            CHECK(xQueueReceive(queue_, &cmd, portMAX_DELAY) == pdTRUE);
        }
    }

  private:
    NetworkTask* network_task_;
    QueueHandle_t queue_;
    std::shared_ptr<tflite::MicroInterpreter> interpreter_;
    size_t counter_;
};

/**
 * MainTask is responsible for managing the camera and sending image data.
 * 
 * It uses the CameraTask API to enable the camera, capture frames, compress them
 * to JPEG format, and send them over the network using NetworkTask.
 */
class MainTask : private Task<MainTask> {
  public:
    MainTask(NetworkTask* network_task, InferenceTask* inference_task)
        : Task("catdog_camera_task", kAppTaskPriority),
        network_task_(network_task),
        inference_task_(inference_task),
        queue_(xQueueCreate(2, sizeof(TaskMessage))) {
        printf("MainTask constructor done!\r\n");
        CHECK(queue_);
    };
   
    void Start() { SendCommandBlocking(kCmdStart); }

    void Stop() { SendCommandBlocking(kCmdStop); }

    void Run() const {
        printf("MainTask::Run() entered\r\n");
        bool started = false;

        std::vector<uint8_t> input(kModelSize);
        std::vector<unsigned char> jpeg(1024 * 70);

        TaskMessage message{};

        while (true) {
            CHECK(xQueueReceive(queue_, &message, portMAX_DELAY) == pdTRUE);

            switch (message.command) {
            case kCmdStart:
                configASSERT(!started);
                started = true;
                CameraTask::GetSingleton()->SetPower(true);
                CameraTask::GetSingleton()->Enable(CameraMode::kStreaming);
                printf("M7 Main Task: started\r\n");
                QueueProcess();
                break;
            case kCmdStop:
                configASSERT(started);
                CameraTask::GetSingleton()->Disable();
                started = false;
                printf("M7 Main Task: stopped\r\n");
                break;
            case kCmdProcess: {
                if (!started) {
                    printf("kCmdProcess skipped: not started\r\n");
                    continue; 
                }

                coralmicro::CameraFrameFormat fmt;
                fmt.width = kModelWidth;
                fmt.height = kModelHeight;
                fmt.fmt = CameraFormat::kRgb;
                fmt.filter = CameraFilterMethod::kBilinear;
                fmt.preserve_ratio = false;
                fmt.buffer = input.data();
                CameraTask::GetSingleton()->GetFrame({fmt});

                auto jpeg_size =
                    JpegCompressRgb(input.data(), fmt.width, fmt.height,
                                    /*quality=*/75, jpeg.data(), jpeg.size());
                network_task_->Send(kMessageTypeImageData, jpeg.data(), jpeg_size);

                inference_task_->Put(input);

                // Process next camera frame.
                QueueProcess();
            } break;

            default:
                printf("Unknown command: %d\r\n", message.command);
                break;
            }

            // Signal the command completion semaphore, if present.
            if (message.completion_semaphore) {
                CHECK(xSemaphoreGive(message.completion_semaphore) == pdTRUE);
            }
        }

        printf("Warning: Run() exited unexpectedly\r\n");
    }
   
  private:
    void SendCommandBlocking(int command) {
        TaskMessage message = {command, xSemaphoreCreateBinary()};
        CHECK(message.completion_semaphore);
        CHECK(xQueueSend(queue_, &message, portMAX_DELAY) == pdTRUE);
        CHECK(xSemaphoreTake(message.completion_semaphore, portMAX_DELAY) ==
                pdTRUE);
        vSemaphoreDelete(message.completion_semaphore);
    }

    void QueueProcess() const {
        TaskMessage message = {kCmdProcess};
        CHECK(xQueueSend(queue_, &message, portMAX_DELAY) == pdTRUE);
    }

    NetworkTask* network_task_ = nullptr;
    InferenceTask* inference_task_ = nullptr;
    QueueHandle_t queue_;
};

[[noreturn]] void Main() {
    printf("Let's Cat and dog!\r\n");
    fflush(stdout); 

    GpioConfigureInterrupt(
        Gpio::kUserButton, GpioInterruptMode::kIntModeFalling,
        [handle = xTaskGetCurrentTaskHandle()]() { xTaskResumeFromISR(handle); },
        /*debounce_interval_us=*/50 * 1e3);

    vTaskSuspend(nullptr);

    // ------------------------------------------------------------------------
    // 1. Start Watchdog
    // ------------------------------------------------------------------------
    // Watchdog ensure system reliability. If the app freezes or crashes, the 
    // board resets after 8 seconds.
    constexpr WatchdogConfig wdt_config = {
        .timeout_s = 8, // If watchdog isn't kiked for 8 seconds, it resets
        .pet_rate_s = 3, // the code should pet the watchdog every 3 seconds 
        .enable_irq = false, // interrupt would not be generated when watchdog is about to fire
    };
    WatchdogStart(wdt_config);

    // ------------------------------------------------------------------------
    // 2. Initialize the TPU
    // ------------------------------------------------------------------------
    // For TPU-based models, it's preferred to use the Edge TPU Manager
    auto tpu_context =
        EdgeTpuManager::GetSingleton()->OpenDevice(PerformanceMode::kMax);
    if (!tpu_context) {
        printf("Failed to get tpu context.\r\n");
        vTaskSuspend(nullptr);
    }

    // ------------------------------------------------------------------------
    // 3. Initialize the LED
    // ------------------------------------------------------------------------
    // Immediate feedback that the app is running.
    LedSet(Led::kStatus, true); 

    std::vector<uint8_t> catdog_tflite;
    if (!LfsReadFile(kModelPath, &catdog_tflite)) {
        printf("ERROR: Failed to read model: %s\r\n", kModelPath);
        vTaskSuspend(nullptr);
    }

    // Starts the mobilenetv3 engine.
    tflite::MicroErrorReporter error_reporter;
    tflite::MicroMutableOpResolver<9> resolver;
    resolver.AddCustom(kCustomOp, RegisterCustomOp());
    resolver.AddFullyConnected(); 
    resolver.AddDepthwiseConv2D(); 
    resolver.AddHardSwish(); 
    resolver.AddAdd(); 
    resolver.AddMean(); 
    resolver.AddLogistic(); 
    resolver.AddMul(); 
    resolver.AddConv2D();
    resolver.AddAveragePool2D();
    auto interpreter = std::make_shared<tflite::MicroInterpreter>(
        tflite::GetModel(catdog_tflite.data()), resolver, tensor_arena,
        kTensorArenaSize, &error_reporter);
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        printf("Failed to allocate tensor\r\n");
        vTaskSuspend(nullptr);
    }

    // ------------------------------------------------------------------------
    // 4. Create FreeRTOS tasks
    // ------------------------------------------------------------------------
    // NetworkTask handles TCP server + message transmission.
    // MainTask handles camera streaming and image processing.
    NetworkTask network_task;
    InferenceTask inference_task(&network_task, interpreter);
    MainTask main_task(&network_task, &inference_task);
    printf("Tasks created, now starting main loop\r\n");


    while (true) {

        // Start camera_task processing.
        main_task.Start();
        printf("Main loop: main_task.Start() done\r\n");

        for (int i = 0; i < 5; i++) {
            LedSet(Led::kUser, true); 
            vTaskDelay(pdMS_TO_TICKS(100)); 
            LedSet(Led::kUser, false); 
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }

        while (true) {
            // printf("System Running...\r\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Stop camera_task processing.
        main_task.Stop();
        printf("main loop: main_task.Stop() done\r\n"); 
    }
}
}
}


extern "C" void app_main(void* param) {
    (void)param;
    coralmicro::Main();
}
  
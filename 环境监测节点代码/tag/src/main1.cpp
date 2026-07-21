// #include "Arduino.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/queue.h"
// #include "freertos/task.h"

// // 定义数据结构体（你可以改成你的传感器数据）
// typedef struct {
//   float x;
//   float y;
//   float z;
//   int counter;
// } DataStruct;

// // 队列句柄（长度=1）
// QueueHandle_t dataQueue;
// // ==================== 线程1：不断写数据 ====================
// void writeTask(void *pvParameters) {
//   DataStruct data;
//   int count = 0;

//   while (1) {
//     // 模拟数据
//     data.x = 1.11 + count;
//     data.y = 2.22 + count;
//     data.z = 3.33 + count;
//     data.counter = count++;

//     // ✅ 覆盖入队（永远不阻塞，永远存最新值）
//     xQueueOverwrite(dataQueue, &data);

//     Serial.print("[写线程] 发送：");
//     Serial.println(data.counter);

//     vTaskDelay(100); // 模拟100ms产生一次数据（可改快）
//   }
// }

// // ==================== 线程2：不断读数据 ====================
// void readTask(void *pvParameters) {
//   DataStruct readData;

//   while (1) {
//     // 阻塞等待，不浪费CPU
//     if (xQueueReceive(dataQueue, &readData, portMAX_DELAY) == pdPASS) {
//       // 永远读到最新数据
//       Serial.print("[读线程] 收到：");
//       Serial.print(readData.x);
//       Serial.print(" | ");
//       Serial.print(readData.y);
//       Serial.print(" | ");
//       Serial.print(readData.z);
//       Serial.print(" | 计数：");
//       Serial.println(readData.counter);
//     }

//     vTaskDelay(500); // 
//   }
// }
// void setup() {
//   Serial.begin(115200);

//   // ==================== 创建队列 ====================
//   // 长度1，单元大小 = 结构体大小
//   dataQueue = xQueueCreate(1, sizeof(DataStruct));

//   // ==================== 创建两个线程 ====================
//   // 线程1：写数据（生产者）
//   xTaskCreate(
//     writeTask,      // 任务函数
//     "WriteTask",    // 名字
//     2048,           // 栈大小
//     NULL,           // 参数
//     1,              // 优先级
//     NULL            // 句柄
//   );

//   // 线程2：读数据（消费者）
//   xTaskCreate(
//     readTask,
//     "ReadTask",
//     2048,
//     NULL,
//     1,
//     NULL
//   );
// }



// void loop() {
//   // 主线程空着就行
//   vTaskDelay(1000);
// }
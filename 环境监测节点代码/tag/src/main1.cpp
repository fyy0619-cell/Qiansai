// #include <Arduino.h>

// // 增加任务堆栈大小
// #define TASK_STACK_SIZE 10240

// void Task1code(void *pvParameters) {
//   Serial.print("Task1 running on core: ");
//   Serial.println(xPortGetCoreID());

//   for(;;) {
//     Serial.println("Hello from core 0");
//     vTaskDelay(1000 / portTICK_PERIOD_MS); // 使用FreeRTOS延迟
//   }
// }

// void Task2code(void *pvParameters) {
//   Serial.print("Task2 running on core: ");
//   Serial.println(xPortGetCoreID());

//   for(;;) {
//     Serial.println("Hello from core 1");
//     vTaskDelay(2000 / portTICK_PERIOD_MS);
//   }
// }

// void setup() {
//   Serial.begin(115200);
//   while(!Serial); // 等待串口连接（仅用于调试）
  
//   // 创建任务
//   xTaskCreatePinnedToCore(
//     Task1code,
//     "Task1",
//     TASK_STACK_SIZE,
//     NULL,
//     1,
//     NULL,
//     0
//   );
  
//   xTaskCreatePinnedToCore(
//     Task2code,
//     "Task2",
//     TASK_STACK_SIZE,
//     NULL,
//     1,
//     NULL,
//     1
//   );
// }

// void loop() {
//   // 空循环
//   vTaskDelay(portMAX_DELAY);
// }
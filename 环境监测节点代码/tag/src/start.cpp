#include "uwb.h"
#include "dw3000.h"
#include "esp_task_wdt.h"  // 加入头文件
// #define INITIATOR
const int buttonPin4 = 4;   // 按键连接的GPIO
const int buttonPin25 = 25; // 按键连接的GPIO
// 核心0任务：蓝牙数据接收
// int flag = 0;
void Task_BluetoothReceive(void *pvParameters)
{
    // esp_task_wdt_add(NULL); // 注册当前任务给 watchdog
    while (1)
    {
        if (digitalRead(buttonPin4) == LOW)
        {
            door = 1;
            flag = 1;
            Serial.println("开门");
            // 等待按键释放（可选）
            while (digitalRead(buttonPin4) == LOW)
            {
                vTaskDelay(5 / portTICK_PERIOD_MS); // 10ms间隔
            }
        }
        else if (digitalRead(buttonPin25) == LOW)
        {
            door = 0;
            flag = 1;
            Serial.println("关门");
            // 等待按键释放（可选）
            while (digitalRead(buttonPin25) == LOW)
            {
                vTaskDelay(5 / portTICK_PERIOD_MS); // 10ms间隔
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
// 核心1任务：数据处理和打印
void Task_DataProcess(void *pvParameters)
{
    // #ifdef INITIATOR
    while (1)
    {
        responder();
    }
}
void setup()
{
    UART_init();
    spiBegin(UWB_IRQ, UWB_RST);
    spiSelect(UWB_SS);
    delay(2);
    start_uwb();
    pinMode(buttonPin4, INPUT_PULLUP);  // 启用内部上拉
    delay(2);
    pinMode(buttonPin25, INPUT_PULLUP); // 启用内部上拉
    delay(2);
    xTaskCreatePinnedToCore(
        Task_BluetoothReceive, // 任务函数
        "BluetoothReceive",    // 任务名称
        4096,                  // 堆栈大小
        NULL,                  // 参数
        3,                     // 优先级
        NULL,                  // 任务句柄
        0                      // 运行在核心0
    );
    xTaskCreatePinnedToCore(
        Task_DataProcess, // 任务函数
        "DataProcess",    // 任务名称
        16384,            // 堆栈大小
        NULL,             // 参数
        1,                // 优先级
        NULL,             // 任务句柄
        1                 // 运行在核心1
    );
}

void loop()
{
    vTaskDelete(NULL); // 删除主循环任务
}

// void loop()
// {
//     // 读取按键状态（按下时为LOW）
//     //   if(digitalRead(buttonPin4) == LOW && flag == 0) {
//     //     Serial.println("开门");
//     //     flag = 1;
//     //     // 防抖处理
//     //     delay(20);
//     //     // 等待按键释放（可选）
//     //     while(digitalRead(buttonPin4) == LOW) {
//     //         delay(5);
//     //     }
//     //   }else if(digitalRead(buttonPin25) == LOW && flag == 1){
//     //     Serial.println("关门");
//     //     flag = 0;
//     //     // 防抖处理
//     //     delay(20);
//     //     // 等待按键释放（可选）
//     //     while(digitalRead(buttonPin25) == LOW) {
//     //         delay(5);
//     //     }
//     //   }

// }
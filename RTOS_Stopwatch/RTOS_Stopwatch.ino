#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

const int OLED_SDA = 8;
const int OLED_SCL = 9;
const int OLED_ADDRESS = 0x3C;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

#if CONFIG_FREERTOS_UNICORE
const BaseType_t app_cpu = 0;
#else
const BaseType_t app_cpu = 1;
#endif

QueueHandle_t commandQueue;
SemaphoreHandle_t oledMutex;
bool running = false;
TickType_t startedAt = 0;
TickType_t savedTicks = 0;

// Elapsed time = previous runs + time since the last start.
TickType_t elapsedTicks(TickType_t now) {
  if (running) return savedTicks + (now - startedAt);
  return savedTicks;
}

// Write the status in the top row of the display buffer.
void showStatus(const char *text) {
  display.fillRect(0, 0, 128, 16, SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(0, 4);
  display.print(text);
}

void serialTask(void *parameter) {
  for (;;) {
    while (Serial.available() > 0) {
      char command = Serial.read();
      if (command == 's' || command == 'r') {
        xQueueSend(commandQueue, &command, portMAX_DELAY);
      } else if (command != '\n' && command != '\r') {
        Serial.println("Use s to start/pause or r to reset.");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void timeTask(void *parameter) {
  char command;
  for (;;) {
    if (xQueueReceive(commandQueue, &command, portMAX_DELAY) == pdPASS) {
      TickType_t now = xTaskGetTickCount();
      xSemaphoreTake(oledMutex, portMAX_DELAY);
      if (command == 'r') {
        running = false;
        savedTicks = 0;
        showStatus("READY");
      } else if (running) {
        savedTicks = elapsedTicks(now);
        running = false;
        showStatus("PAUSED");
      } else {
        startedAt = now;
        running = true;
        showStatus("RUNNING");
      }
      xSemaphoreGive(oledMutex);
    }
  }
}

void oledTask(void *parameter) {
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    xSemaphoreTake(oledMutex, portMAX_DELAY);
    unsigned long tenths = (uint64_t)elapsedTicks(xTaskGetTickCount()) * 10 / configTICK_RATE_HZ;
    display.fillRect(0, 20, 128, 24, SSD1306_BLACK);
    display.setTextSize(2);
    display.setCursor(0, 24);
    display.printf("%02lu:%02lu.%lu", tenths / 600, (tenths / 10) % 60, tenths % 10);
    display.display();
    xSemaphoreGive(oledMutex);
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  if (!Wire.begin(OLED_SDA, OLED_SCL)) {
    Serial.println("I2C setup failed. Check the OLED pins.");
    return;
  }
  Wire.beginTransmission(OLED_ADDRESS);
  if (Wire.endTransmission() != 0 ||
      !display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, false, false)) {
    Serial.println("OLED setup failed. Check its wiring and address.");
    return;
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  showStatus("READY");

  commandQueue = xQueueCreate(5, sizeof(char));
  oledMutex = xSemaphoreCreateMutex();
  if (commandQueue == NULL || oledMutex == NULL) {
    Serial.println("FreeRTOS setup failed.");
    return;
  }
  if (xTaskCreatePinnedToCore(oledTask, "OLED", 4096, NULL, 1, NULL, app_cpu) != pdPASS ||
      xTaskCreatePinnedToCore(timeTask, "Time", 3072, NULL, 2, NULL, app_cpu) != pdPASS ||
      xTaskCreatePinnedToCore(serialTask, "Serial", 3072, NULL, 2, NULL, app_cpu) != pdPASS) {
    Serial.println("Could not start all three tasks.");
    return;
  }
  Serial.println("Send s to start/pause/resume. Send r to reset.");
}

void loop() {
  vTaskDelete(NULL);
}

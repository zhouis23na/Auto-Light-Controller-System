#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= PIN DEFINITIONS =================
#define PIN_LDR_AO 12
#define PIN_LDR_DO 41
#define PIN_POT 2
#define PIN_EMG 3
#define PIN_MODE 11
#define PIN_LED_PWM 4
#define PIN_SDA 5
#define PIN_SCL 6

// RGB LED (Common Cathode)
#define PIN_R 7
#define PIN_G 8
#define PIN_B 9

#define PIN_BUZZER 10

// ================= OLED =================
Adafruit_SSD1306 display(128, 64, &Wire);

// =============== RTOS OBJECTS ===============
QueueHandle_t queueSensor;
QueueHandle_t queueInput;
SemaphoreHandle_t semOLED;
SemaphoreHandle_t mutexShared;

// =============== STRUCT ===============
typedef struct {
  int analogValue;
  int digitalValue;
} SensorData;

// =============== VARIABLES ===============
int brightnessShared = 0;
bool emergencyStop = false;
int systemMode = 0; // 0 = AUTO, 1 = MANUAL

int lastLDR = 0;  // nilai LDR terbaru untuk OLED

// MODE button debounce
bool lastModeBtn = HIGH;
unsigned long lastDebounce = 0;

// ================= INTERRUPT =================
void IRAM_ATTR emergencyISR() {
  emergencyStop = true;
}

// ================= TASK SENSOR =================
void TaskSensor(void *param) {
  SensorData data;

  while (1) {
    data.analogValue = analogRead(PIN_LDR_AO);   // FIXED HERE
    data.digitalValue = digitalRead(PIN_LDR_DO);

    xQueueSend(queueSensor, &data, 0);

    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ================= TASK POT =================
void TaskInput(void *param) {
  while (1) {
    int potValue = analogRead(PIN_POT);
    xQueueSend(queueInput, &potValue, 0);
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ================= TASK OLED =================
void TaskOLED(void *param) {
  while (1) {
    if (xSemaphoreTake(semOLED, portMAX_DELAY)) {

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      display.setCursor(0, 0);
      display.println("Auto Light System");

      display.setCursor(0, 15);
      display.printf("Mode: %s", systemMode == 0 ? "AUTO" : "MANUAL");

      display.setCursor(0, 30);
      display.printf("Brightness: %d", brightnessShared);

      display.setCursor(0, 50);
      display.printf("LDR (AO): %d", lastLDR);

      display.display();

      xSemaphoreGive(semOLED);
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ================= TASK MODE BUTTON =================
void TaskMode(void *param) {
  pinMode(PIN_MODE, INPUT_PULLUP);

  while (1) {
    bool reading = digitalRead(PIN_MODE);

    if (reading != lastModeBtn) {
      lastDebounce = millis();
    }

    if ((millis() - lastDebounce) > 50) {
      if (reading == LOW) {
        systemMode = !systemMode;
        Serial.print("MODE: ");
        Serial.println(systemMode == 0 ? "AUTO" : "MANUAL");
        vTaskDelay(300);
      }
    }

    lastModeBtn = reading;
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// ================= TASK CONTROL =================
void TaskControl(void *param) {
  SensorData sensor;
  int potValue;

  while (1) {
    if (emergencyStop) {
      digitalWrite(PIN_R, HIGH);
      digitalWrite(PIN_G, LOW);
      digitalWrite(PIN_B, LOW);
      digitalWrite(PIN_BUZZER, HIGH);
      continue;
    }

    xQueueReceive(queueSensor, &sensor, 10);
    xQueueReceive(queueInput, &potValue, 10);

    lastLDR = sensor.analogValue;  // update OLED

    xSemaphoreTake(mutexShared, portMAX_DELAY);

    if (systemMode == 0) {
      // AUTO MODE (Hybrid Analog + DO)
      if (sensor.digitalValue == 0) {
        brightnessShared = map(sensor.analogValue, 0, 4095, 200, 255);
      } else {
        brightnessShared = map(sensor.analogValue, 0, 4095, 255, 0);
      }
    } else {
      brightnessShared = map(potValue, 0, 4095, 0, 255);
    }

    xSemaphoreGive(mutexShared);

    vTaskDelay(40 / portTICK_PERIOD_MS);
  }
}

// ================= TASK SOFTWARE PWM =================
void TaskLightControl(void *param) {
  pinMode(PIN_LED_PWM, OUTPUT);

  while (1) {
    xSemaphoreTake(mutexShared, portMAX_DELAY);
    int br = brightnessShared;
    xSemaphoreGive(mutexShared);

    digitalWrite(PIN_LED_PWM, HIGH);
    delayMicroseconds(br * 4);
    digitalWrite(PIN_LED_PWM, LOW);
    delayMicroseconds((255 - br) * 4);
  }
}

// ================= TASK RGB STATUS =================
void TaskAlarm(void *param) {
  while (1) {
    if (systemMode == 0) {
      digitalWrite(PIN_R, LOW);
      digitalWrite(PIN_G, HIGH);
      digitalWrite(PIN_B, LOW);
    } else {
      digitalWrite(PIN_R, LOW);
      digitalWrite(PIN_G, LOW);
      digitalWrite(PIN_B, HIGH);
    }

    vTaskDelay(150 / portTICK_PERIOD_MS);
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_LDR_DO, INPUT);
  pinMode(PIN_EMG, INPUT_PULLUP);

  attachInterrupt(PIN_EMG, emergencyISR, FALLING);

  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  Wire.begin(PIN_SDA, PIN_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  queueSensor = xQueueCreate(10, sizeof(SensorData));
  queueInput = xQueueCreate(10, sizeof(int));
  mutexShared = xSemaphoreCreateMutex();
  semOLED = xSemaphoreCreateBinary();
  xSemaphoreGive(semOLED);

  xTaskCreatePinnedToCore(TaskSensor, "Sensor", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskInput, "Input", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskOLED, "OLED", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskMode, "Mode", 4096, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(TaskControl, "Control", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskLightControl, "LightPWM", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskAlarm, "Alarm", 4096, NULL, 1, NULL, 1);
}

void loop() {}

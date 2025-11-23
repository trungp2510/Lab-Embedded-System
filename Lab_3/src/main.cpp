#define LED_PIN 48
#define SDA_PIN GPIO_NUM_11
#define SCL_PIN GPIO_NUM_12
#define NEO_PIN 45
#define LED_COUNT 1 

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include "DHT20.h"
#include "Wire.h"
#include <ArduinoOTA.h>
#include <TensorFlowLite_ESP32.h>
#include <TinyML.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <TaskWebServer.h>



// constexpr char WIFI_SSID[] = "ACLAB-IOT";
// constexpr char WIFI_PASSWORD[] = "12345678";

constexpr char TOKEN[] = "aw3nAm0MhHxovsbMvyjF";

constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

constexpr char BLINKING_INTERVAL_ATTR[] = "blinkingInterval";
constexpr char LED_MODE_ATTR[] = "ledMode";
constexpr char LED_STATE_ATTR[] = "ledState";

volatile bool attributesChanged = false;
volatile int ledMode = 0;
volatile bool ledState = false;

constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;

uint32_t previousStateChange;

constexpr int16_t telemetrySendInterval = 10000U;
uint32_t previousDataSend;

constexpr std::array<const char *, 2U> SHARED_ATTRIBUTES_LIST = {
  LED_STATE_ATTR,
  BLINKING_INTERVAL_ATTR
};

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

DHT20 dht20;
namespace{
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
constexpr int kTensorArenaSize = 8 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
}

RPC_Response setLedSwitchState(const RPC_Data &data) {
    Serial.println("Received Switch state");
    bool newState = data;
    Serial.print("Switch state change: ");
    Serial.println(newState);
    digitalWrite(LED_PIN, newState);
    attributesChanged = true;
    return RPC_Response("setLedSwitchValue", newState);
}

const std::array<RPC_Callback, 1U> callbacks = {
  RPC_Callback{ "setLedSwitchValue", setLedSwitchState }
};

void processSharedAttributes(const Shared_Attribute_Data &data) {
  for (auto it = data.begin(); it != data.end(); ++it) {
    if (strcmp(it->key().c_str(), BLINKING_INTERVAL_ATTR) == 0) {
      const uint16_t new_interval = it->value().as<uint16_t>();
      if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX) {
        blinkingInterval = new_interval;
        Serial.print("Blinking interval is set to: ");
        Serial.println(new_interval);
      }
    } else if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0) {
      ledState = it->value().as<bool>();
      digitalWrite(LED_PIN, ledState);
      Serial.print("LED state is set to: ");
      Serial.println(ledState);
    }
  }
  attributesChanged = true;
}

const Shared_Attribute_Callback attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback attribute_shared_request_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());


void neo_blinky(void *pvParameters){

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();

    while(1) {                          
        strip.setPixelColor(0, strip.Color(255, 0, 0)); // Set pixel 0 to red
        strip.show(); // Update the strip

        // Wait for 500 milliseconds
        vTaskDelay(500);

        // Set the pixel to off
        strip.setPixelColor(0, strip.Color(0, 0, 0)); // Turn pixel 0 off
        strip.show(); // Update the strip

        // Wait for another 500 milliseconds
        vTaskDelay(500);
    }
}
void WiFiTask(void *pvParameters) {
  while(1) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi not connected, reconnecting...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);

      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("WiFi reconnected");
    }
    vTaskDelay(10000);
  }
}

void serverTask(void *pvParameters) {
  while(1) {
    if(WiFi.status()!= WL_CONNECTED){
      return;
    }
    if (!tb.connected()) {
      Serial.print("Connecting to: ");
      Serial.print(THINGSBOARD_SERVER);
      Serial.print(" with token ");
      Serial.println(TOKEN);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
        Serial.println("Failed to connect");
        return;
      }
  
      Serial.println("Subscribing for RPC...");
      if (!tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend())) {
        Serial.println("Failed to subscribe for RPC");
        return;
      }
  
      if (!tb.Shared_Attributes_Subscribe(attributes_callback)) {
        Serial.println("Failed to subscribe for shared attribute updates");
        return;
      }
  
      Serial.println("Subscribe done");
  
      if (!tb.Shared_Attributes_Request(attribute_shared_request_callback)) {
        Serial.println("Failed to request for shared attributes");
        return;
      }
    }else if(tb.connected()){
      tb.loop();
    }
    vTaskDelay(1000);
  }
}

void setupAI(){
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;
  
  model = tflite::GetModel(Tinymodel);
  if(model->version() != TFLITE_SCHEMA_VERSION){
    error_reporter->Report("Model provided is schema version %d, not equal to supported version %d.",model->version(),TFLITE_SCHEMA_VERSION);
    return;
  }
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter (model, resolver, tensor_arena,kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if(allocate_status != kTfLiteOk){
    error_reporter->Report ("AllocateTensors() failed");
    Serial.println(allocate_status);
    return;
  }
  Serial.println(interpreter->arena_used_bytes());
  input = interpreter->input(0);
  output = interpreter->output(0);
}

void AITask(void *pvParameters){
  setupAI();
  while(1){
    input->data.f[0] = global_temperature;
    input->data.f[1] = global_humidity;
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk){
      error_reporter->Report("Invoke failed");
      return;
    }
    float result = output->data.f[0];
    Serial.printf("Input: %f, %f, -> Predicted: %f\n",
                           global_temperature, global_humidity, result);
    vTaskDelay(5000);
  }
}
void LedTask(void *pvParameters){
  pinMode(GPIO_NUM_48,OUTPUT);
  int ledState = 0;
  while(1){
    if(ledState == 0){
      digitalWrite(GPIO_NUM_48,HIGH);
    } else {
      digitalWrite(GPIO_NUM_48,LOW);
    }
    ledState = 1 - ledState;
    vTaskDelay(1000);
  }
}
void LoopTask(void * pvParameters) {
  while(1) {
    tb.loop();
    vTaskDelay(1000);
  }
}
void sendTask(void * pvParameters) {
  while(1) {
    dht20.read();
    
    float temperature = dht20.getTemperature();
    float humidity = dht20.getHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT20 sensor!");
    } else {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" °C, Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");
      global_temperature = temperature;
      global_humidity = humidity;
      if(WiFi.status() == WL_CONNECTED){
      String jsonPayload = "{";
      jsonPayload += "\"temperature\":" + String(temperature, 2) + ",";
      jsonPayload += "\"humidity\":" + String(humidity, 2);
      jsonPayload += "}";
      tb.sendTelemetryJson(jsonPayload.c_str());
      }
    }
    vTaskDelay(1000);
  }
}
void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  pinMode(LED_PIN, OUTPUT);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);
  dht20.begin();
  

  
  
  xTaskCreate(sendTask, "sendTask", 4096, NULL, 2, NULL);
  xTaskCreate(LedTask,"ledTask", 2048, NULL, 2, NULL);
  xTaskCreate(neo_blinky,"neo_blinky", 2048, NULL, 2, NULL);
  xTaskCreate(AITask, "AITask", 8192, NULL, 3, NULL);
  xTaskCreate(main_server_task, "main_server_task", 8192, NULL, 2, NULL);
  xTaskCreate(serverTask, "serverTask", 4096, NULL, 3, NULL);
}

void loop() {
  
}
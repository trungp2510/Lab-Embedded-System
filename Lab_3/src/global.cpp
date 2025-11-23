#include "global.h"

float global_temperature = 0;
float global_humidity = 0;
String WIFI_SSID = "";
String WIFI_PASS = "";
String CORE_IOT_TOKEN;
String CORE_IOT_SERVER;
String CORE_IOT_PORT;

bool isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();
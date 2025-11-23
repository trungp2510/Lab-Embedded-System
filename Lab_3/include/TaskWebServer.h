#ifndef WIFI_WEBSERVER_H
#define WIFI_WEBSERVER_H

#include "global.h"
#include <Arduino.h>
/**
 * WiFi Manager - Quản lý kết nối WiFi với giao diện web
 * 
 * Sử dụng:
 *   #include "WiFiManager.h"
 *   
 *   void setup() {
 *     Serial.begin(115200);
 *     wifi_web();  // Tự động kết nối hoặc mở portal cấu hình
 *   }
 */

// Cấu hình mặc định
#define DEFAULT_AP_SSID "ESP32_Config"
#define DEFAULT_AP_PASS ""  // Để trống = open network
#define DEFAULT_WIFI_TIMEOUT 20000  // 20 giây

/**
 * Hàm chính - Khởi động WiFi Manager
 * 
 * @param ap_ssid SSID của Access Point (mặc định: "ESP32_Config")
 * @param ap_pass Mật khẩu AP (mặc định: "" = mạng mở, tối thiểu 8 ký tự nếu có)
 * @param timeout_ms Thời gian chờ kết nối WiFi (mặc định: 20000ms)
 * @return true nếu đã kết nối WiFi thành công, false nếu đang chạy portal
 * 
 * Hàm này sẽ:
 * 1. Kiểm tra xem đã có thông tin WiFi đã lưu chưa
 * 2. Nếu có: thử kết nối WiFi
 *    - Thành công: return true, app có thể chạy tiếp
 *    - Thất bại: mở portal cấu hình (blocking)
 * 3. Nếu không có: mở portal cấu hình (blocking)
 */
bool wifi_web(const char* ap_ssid = DEFAULT_AP_SSID, 
              const char* ap_pass = DEFAULT_AP_PASS,
              uint32_t timeout_ms = DEFAULT_WIFI_TIMEOUT);

/**
 * Xóa thông tin WiFi đã lưu
 * Hữu ích khi muốn reset cấu hình
 */
void wifi_clear_credentials();

/**
 * Kiểm tra trạng thái kết nối WiFi
 * @return true nếu đang kết nối
 */
bool wifi_is_connected();

/**
 * Lấy thông tin WiFi đã lưu
 * @param ssid Tham chiếu để nhận SSID
 * @param pass Tham chiếu để nhận password
 * @return true nếu có thông tin đã lưu
 */
bool wifi_get_credentials(String &ssid, String &pass);

void main_server_task(void *pvParameters);
#endif // WIFI_WEBSERVER_H


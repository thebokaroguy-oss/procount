#pragma once
// Copy to secrets.h and fill in your own values. Never commit secrets.h.
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define MQTT_HOST "192.168.1.100" // Computer running HiveMQ, as reachable from the ESP32 Wi-Fi network
#define MQTT_PORT 1883
#define MQTT_USE_TLS 0
#define MQTT_USERNAME "" // Empty for the supplied local dev broker unless you add authentication
#define MQTT_PASSWORD ""
#define MQTT_DEVICE_ID "procount-esp32-01"
#define RFID_UID_A "REPLACE_WITH_TAG_A_UID"
#define RFID_UID_B "REPLACE_WITH_TAG_B_UID"
// Set MQTT_USE_TLS to 1 and add a CA certificate only when using a TLS listener.

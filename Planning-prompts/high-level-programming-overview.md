# High-Level Programming Overview

## JKBMS2ESPMQTT Firmware Function Reference

This document provides a high-level overview of all functions in the ESP32 firmware, organized by functional area.

**Version 1.3.0 Enhancement**: This firmware now publishes comprehensive BMS data including all available protocol fields (30+ parameters) organized into structured sections for maximum data richness and usability.

---

## Core Application Functions

### `app_main(void)`
**Main entry point** - Initializes all systems, configures hardware, starts tasks, and enters the main BMS monitoring loop.

---

## UART Communication & BMS Interface

### `init_uart()`
**Initialize UART hardware** - Configures UART2 (pins 16/17) for RS485 communication with the BMS at 115200 baud.

### `send_bms_command(const uint8_t* cmd, size_t len)`
**Send command to BMS** - Transmits a raw command frame to the BMS via UART and logs the transmission.

### `parse_and_print_bms_data(const uint8_t *data, int len)`
**Parse BMS response frame** - Validates frame structure, checks CRC, extracts cell voltages and pack data, parses extra fields using ID code table.

---

## Data Unpacking Utilities

### `unpack_u16_be(const uint8_t *buffer, int offset)`
**Extract 16-bit big-endian value** - Utility function to read 2-byte values from BMS data packets.

### `unpack_u8(const uint8_t *buffer, int offset)`
**Extract 8-bit value** - Utility function to read single byte values from BMS data packets.

---

## MQTT Communication

### `publish_bms_data_mqtt(const bms_data_t *bms_data_ptr)`
**Publish comprehensive BMS data to MQTT** - Creates structured JSON with 30+ BMS parameters organized into logical sections (systemStatus, systemConfig, temperatureProtection, systemInfo) plus individual cell voltages and raw field data. Payload size: ~2,760-2,780 characters.

### `mqtt_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)`
**Handle MQTT events** - Processes connection, disconnection, publish confirmation events, triggers heartbeat LED and watchdog reset on successful publish.

---

## WiFi Management

### `wifi_init_sta(void)`
**Initialize WiFi station mode** - Configures ESP32 as WiFi client, loads credentials from NVS, connects to configured network.

### `wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)`
**Handle WiFi events** - Manages WiFi connection/disconnection events, implements retry logic with exponential backoff.

### `wifi_scan_json_get_handler(httpd_req_t *req)`
**WiFi network scanner** - Scans for available WiFi networks, returns JSON list with SSID, signal strength, and security type for captive portal.

---

## Configuration Management (NVS)

### `load_wifi_config_from_nvs()`
**Load WiFi credentials** - Retrieves stored WiFi SSID and password from non-volatile storage, uses defaults if not found.

### `load_mqtt_config_from_nvs()`
**Load MQTT broker URL** - Retrieves stored MQTT broker URL from NVS, uses default if not configured.

### `load_sample_interval_from_nvs()`
**Load BMS polling interval** - Retrieves configured BMS data sampling interval from NVS, uses 5-second default.

### `load_bms_topic_from_nvs()`
**Load MQTT topic name** - Retrieves configured MQTT topic prefix from NVS, uses "BMS/JKBMS" default.

### `save_bms_topic_to_nvs(const char *topic)`
**Save MQTT topic** - Stores user-configured MQTT topic name to non-volatile storage.

---

## Web Interface & Captive Portal

### `init_spiffs()`
**Initialize file system** - Mounts SPIFFS file system containing web interface files (HTML, CSS, JavaScript).

### `parameters_html_handler(httpd_req_t *req)`
**Serve configuration page** - Returns the main configuration web page (parameters.html) for captive portal.

### `params_json_get_handler(httpd_req_t *req)`
**Get current config as JSON** - Returns current WiFi, MQTT, and BMS settings as JSON for web interface.

### `params_update_post_handler(httpd_req_t *req)`
**Update configuration** - Processes form submissions from web interface, validates and saves new WiFi/MQTT/BMS settings to NVS.

### `captive_redirect_handler(httpd_req_t *req)`
**Captive portal redirect** - Redirects all HTTP requests to the configuration page when in AP mode.

---

## Access Point & Configuration Mode

### `ap_config_task(void *pvParameter)`
**Access Point configuration task** - Creates WiFi AP, starts web server, enables captive portal, handles DNS hijacking for device configuration.

---

## Network Utilities

### `dns_hijack_task(void *pvParameter)`
**DNS hijacking server** - Intercepts all DNS requests in AP mode and redirects them to the ESP32's IP address for captive portal functionality.

### `url_decode(char *dst, const char *src, size_t dstsize)`
**URL decoder** - Decodes percent-encoded URLs from HTTP form submissions (converts %20 to spaces, etc.).

### `hex2int(char c)`
**Hex character to integer** - Utility function to convert hexadecimal characters ('0'-'9', 'A'-'F') to integer values.

---

## System Health & Monitoring

### `blink_heartbeat()`
**Visual status indicator** - Blinks the onboard LED to indicate successful MQTT publish operations and system health.

---

## Program Flow Summary

1. **Initialization** (`app_main`)
   - Initialize NVS, UART, SPIFFS
   - Load configuration from storage
   - Check boot button for config mode

2. **Configuration Mode** (if boot button pressed)
   - Start AP mode (`ap_config_task`)
   - Serve web interface
   - Allow user to configure WiFi/MQTT settings

3. **Normal Operation**
   - Connect to WiFi (`wifi_init_sta`)
   - Connect to MQTT broker
   - Enter main loop:
     - Send BMS command
     - Parse BMS response
     - Publish to MQTT
     - Blink heartbeat LED
     - Wait for configured interval

4. **Error Recovery**
   - Software watchdog monitors MQTT publish success
   - Automatic reboot if communication fails
   - WiFi reconnection with retry logic

---

## Key Data Structures

- **`bms_data_t`** - Parsed BMS data (voltages, current, SOC, temperatures)
- **`bms_extra_field_t`** - Additional BMS parameters with ID codes
- **`bms_idcode_entry_t`** - Mapping table for BMS field ID codes to human names

---

## Configuration Constants

- **UART**: Port 2, Pins 16/17, 115200 baud
- **LED**: GPIO 2 (onboard LED)
- **Boot Button**: GPIO 0
- **Default Topic**: "BMS/JKBMS"
- **Default Sample Interval**: 5 seconds
- **AP SSID**: "ESP32-Config-{MAC}"

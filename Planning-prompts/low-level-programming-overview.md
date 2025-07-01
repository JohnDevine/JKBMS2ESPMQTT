# Low-Level Programming Overview

## JKBMS2ESPMQTT Firmware Detailed Function Analysis

This document provides in-depth analysis of all functions including internal logic, parameter details, data structures, and implementation specifics.

---

## Core Application Functions

### `app_main(void)`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **Logging Control Setup**
   - Calls `esp_log_level_set("*", ESP_LOG_WARN)` to reduce output verbosity
2. **NVS Initialization**
   - Calls `nvs_flash_init()` with error checking
   - Handles `ESP_ERR_NVS_NO_FREE_PAGES` and `ESP_ERR_NVS_NEW_VERSION_FOUND` by erasing and reinitializing
3. **Task Watchdog Configuration**
   - Creates `esp_task_wdt_config_t` structure with 30-second timeout
   - Monitors all cores with `idle_core_mask = (1 << portNUM_PROCESSORS) - 1`
   - Enables panic trigger on timeout
4. **Hardware Initialization**
   - Calls `init_uart()` for RS485 communication
   - Calls `init_spiffs()` for web file system
5. **Configuration Loading**
   - Sequentially loads: WiFi config, MQTT config, sample interval, BMS topic
6. **Boot Button Check**
   - Configures GPIO 0 as input with pull-up
   - 10-second polling loop checking `gpio_get_level(BOOT_BTN_GPIO)`
   - Creates AP config task if button pressed
7. **Main Operation Loop**
   - WiFi station initialization
   - MQTT client setup and connection
   - Infinite BMS polling loop with configurable interval
   - Software watchdog management based on MQTT publish success

---

## UART Communication & BMS Interface

### `init_uart()`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **UART Configuration Structure**
   ```c
   uart_config_t uart_config = {
       .baud_rate = 115200,               // JK BMS standard rate
       .data_bits = UART_DATA_8_BITS,     // 8N1 configuration
       .parity = UART_PARITY_DISABLE,
       .stop_bits = UART_STOP_BITS_1,
       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
       .source_clk = UART_SCLK_APB
   };
   ```
2. **Driver Installation**
   - `uart_driver_install(UART_NUM_2, UART_BUF_SIZE*2, 0, 0, NULL, 0)`
   - RX buffer: 2048 bytes (1024 * 2)
   - TX buffer: 0 (direct write mode)
   - No event queue or interrupts
3. **Pin Assignment**
   - TX: GPIO 17, RX: GPIO 16
   - No RTS/CTS flow control pins

### `send_bms_command(const uint8_t* cmd, size_t len)`
**Parameters:**
- `cmd`: Pointer to command byte array
- `len`: Command length in bytes
**Returns:** void  
**Internal Logic:**
1. **Direct UART Write**
   - `uart_write_bytes(UART_NUM_2, (const char*)cmd, len)`
   - Casts uint8_t* to char* for API compatibility
   - Returns actual bytes written for logging

### `parse_and_print_bms_data(const uint8_t *data, int len)`
**Parameters:**
- `data`: Raw BMS response buffer
- `len`: Response length in bytes
**Returns:** void  
**Internal Logic:**
1. **Frame Validation**
   - Minimum length check (>= 20 bytes)
   - Start frame validation: `data[0] == 0x4E && data[1] == 0x57`
   - Frame length field extraction: `(data[2] << 8) | data[3]`
   - Total frame length validation
2. **CRC Verification**
   - Calculates sum of all bytes from offset 2 to end-4
   - Extracts received CRC from last 4 bytes
   - Compares calculated vs received checksums
3. **Payload Extraction**
   - Payload starts at offset 11
   - Payload length = frame_len_field - 9
   - Hex dump logging for debugging
4. **Cell Voltage Parsing**
   - Expects first byte = 0x79 (Cell Voltages ID)
   - Second byte = number of cells
   - Each cell: 2 bytes big-endian, value/1000 = voltage
   - Updates `current_bms_data.cell_voltages[]` array
5. **Pack Data Extraction**
   - Total voltage: bytes 2-3 (big-endian) / 100
   - Current: bytes 4-5 (signed big-endian) / 100
   - SOC: byte 8
   - Temperatures: bytes 9-10, 11-12, 13-14 (big-endian, Celsius)
6. **Extra Fields Processing**
   - Scans remaining payload for additional data fields
   - Uses `bms_idcodes[]` lookup table for field identification
   - Supports ASCII string fields (Code, Column types)
   - Handles numeric fields with various byte lengths
   - Skips duplicate field IDs
   - Stores in `extra_fields[]` global array

---

## Data Unpacking Utilities

### `unpack_u16_be(const uint8_t *buffer, int offset)`
**Parameters:**
- `buffer`: Source data buffer
- `offset`: Byte position to read from
**Returns:** `uint16_t` big-endian value  
**Internal Logic:**
```c
return (buffer[offset] << 8) | buffer[offset + 1];
```

### `unpack_u8(const uint8_t *buffer, int offset)`
**Parameters:**
- `buffer`: Source data buffer  
- `offset`: Byte position to read from
**Returns:** `uint8_t` value  
**Internal Logic:**
```c
return buffer[offset];
```

---

## MQTT Communication

### `publish_bms_data_mqtt(const bms_data_t *bms_data_ptr)`
**Parameters:**
- `bms_data_ptr`: Pointer to parsed BMS data structure
**Returns:** void  
**Internal Logic:**
1. **Client Validation**
   - Checks `mqtt_client != NULL`
   - Returns early if client not initialized
2. **JSON Structure Creation**
   ```c
   {
     "pack": {
       "packV": float,
       "packA": float, 
       "packNumberOfCells": int,
       "packSOC": int,
       "tempSensorValues": {
         "NTC0": float,  // MOSFET temp
         "NTC1": float,  // Probe 1 temp
         "NTC2": float   // Probe 2 temp
       },
       // Extra fields with sanitized names
     },
     "cells": {
       "cell0mV": int, "cell0V": float,
       "cell1mV": int, "cell1V": float,
       // ... for each cell
     }
   }
   ```
3. **Extra Fields Processing**
   - Iterates through `extra_fields[]` array
   - Looks up human-readable names in `bms_idcodes[]` table
   - Sanitizes field names (alphanumeric + underscore only)
   - Special handling for Device ID Code (0xB4) - always string
   - Adds ASCII fields as strings, numeric fields as numbers
4. **Topic Construction**
   - Uses `bms_topic` directly (no prefix added)
   - Example: "BMS/JKBMS" or user-configured value
5. **Publishing**
   - `cJSON_PrintUnformatted()` for compact JSON
   - `esp_mqtt_client_publish()` with QoS 1, retain flag 0

### `mqtt_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)`
**Parameters:**
- `arg`: User data (unused)
- `event_base`: Event base (MQTT_EVENTS)
- `event_id`: Specific event type
- `event_data`: Event-specific data
**Returns:** void  
**Internal Logic:**
1. **Event Processing Switch**
   - `MQTT_EVENT_CONNECTED`: Sets global connection flag
   - `MQTT_EVENT_DISCONNECTED`: Clears connection flag
   - `MQTT_EVENT_PUBLISHED`: Triggers heartbeat LED and watchdog reset
   - `MQTT_EVENT_ERROR`: Logs error details
2. **Critical Actions on Publish Success**
   - `blink_heartbeat()` - Visual confirmation
   - `esp_task_wdt_reset()` - Prevent system reboot
   - `mqtt_publish_success = true` - Flag for main loop

---

## WiFi Management

### `wifi_init_sta(void)`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **Network Interface Creation**
   - `esp_netif_create_default_wifi_sta()`
   - Creates default station interface
2. **WiFi Initialization**
   - `esp_wifi_init()` with default configuration
   - Sets storage to RAM mode
3. **Event Handler Registration**
   - Registers `wifi_event_handler` for WiFi events
   - Registers `wifi_event_handler` for IP events
4. **Station Configuration**
   ```c
   wifi_config_t wifi_config = {
       .sta = {
           .ssid = wifi_ssid,        // From NVS
           .password = wifi_pass,    // From NVS
           .threshold.authmode = WIFI_AUTH_WPA2_PSK
       }
   };
   ```
5. **Connection Process**
   - Sets WiFi mode to station
   - Applies configuration
   - Starts WiFi and initiates connection

### `wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)`
**Parameters:**
- `arg`: User context (unused)
- `event_base`: Event source (WIFI_EVENT or IP_EVENT)
- `event_id`: Specific event ID
- `event_data`: Event payload data
**Returns:** void  
**Internal Logic:**
1. **WiFi Event Processing**
   - `WIFI_EVENT_STA_START`: Calls `esp_wifi_connect()`
   - `WIFI_EVENT_STA_DISCONNECTED`: Implements retry logic
     * Exponential backoff with jitter
     * Maximum 5 retry attempts
     * Delay calculation: `(1 << wifi_retry_num) * 1000 + (esp_random() % 1000)`
2. **IP Event Processing**
   - `IP_EVENT_STA_GOT_IP`: Logs IP address, resets retry counter

### `wifi_scan_json_get_handler(httpd_req_t *req)`
**Parameters:**
- `req`: HTTP request structure
**Returns:** `esp_err_t` (ESP_OK on success)  
**Internal Logic:**
1. **Mode Switching**
   - Checks current WiFi mode
   - Switches to APSTA mode if in AP-only mode for scanning
2. **Scanning Process**
   - Configures `wifi_scan_config_t` with defaults
   - Calls `esp_wifi_scan_start()` with blocking=true
   - Retrieves results with `esp_wifi_scan_get_ap_records()`
3. **JSON Response Generation**
   - Creates networks array with SSID, RSSI, auth type
   - SSID sanitization (escapes quotes and control characters)
   - Auth mode translation to readable strings
   - Limits to 20 networks maximum
4. **Fallback Handling**
   - Returns hardcoded networks if scan fails
   - Restores original WiFi mode on completion

---

## Configuration Management (NVS)

### `load_wifi_config_from_nvs()`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **NVS Handle Opening**
   - Opens "wifi_cfg" namespace in read-only mode
   - Graceful fallback if namespace doesn't exist
2. **SSID Loading**
   - `nvs_get_str()` with WIFI_NVS_KEY_SSID ("ssid")
   - Buffer size: 33 bytes (max SSID length + null)
   - Uses DEFAULT_WIFI_SSID if not found
3. **Password Loading**
   - `nvs_get_str()` with WIFI_NVS_KEY_PASS ("pass")
   - Buffer size: 65 bytes (max WPA password + null)
   - Uses DEFAULT_WIFI_PASS if not found
4. **Error Handling**
   - Logs specific NVS error codes
   - Always ensures null-terminated strings

### `load_mqtt_config_from_nvs()`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **Similar pattern to WiFi config**
2. **MQTT URL Loading**
   - Key: MQTT_NVS_KEY_URL ("broker_url")
   - Buffer: 128 bytes for full URL
   - Default: DEFAULT_MQTT_BROKER_URL ("mqtt://127.0.0.1")

### `load_sample_interval_from_nvs()`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **Interval Loading**
   - Key: NVS_KEY_SAMPLE_INTERVAL ("sample_interval")
   - Type: `size_t` (8 bytes)
   - Default: DEFAULT_SAMPLE_INTERVAL (5000ms)
2. **Validation**
   - Ensures positive, reasonable values
   - Used for BMS polling frequency

### `load_bms_topic_from_nvs()`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **Topic String Loading**
   - Key: NVS_KEY_BMS_TOPIC ("BMS_Topic")
   - Buffer: 41 bytes (topic name + null)
   - Default: "BMS/JKBMS"
2. **Auto-save Default**
   - Saves default to NVS if not found
   - Ensures persistence across reboots

### `save_bms_topic_to_nvs(const char *topic)`
**Parameters:**
- `topic`: New topic string to save
**Returns:** void  
**Internal Logic:**
1. **NVS Write Operation**
   - Opens namespace in read-write mode
   - `nvs_set_str()` with validation
   - `nvs_commit()` to ensure persistence
2. **Error Handling**
   - Logs save operation success/failure
   - Graceful handling of write errors

---

## Web Interface & Captive Portal

### `init_spiffs()`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **SPIFFS Configuration**
   ```c
   esp_vfs_spiffs_conf_t conf = {
       .base_path = "/spiffs",
       .partition_label = NULL,    // Use default partition
       .max_files = 5,             // Limit open files
       .format_if_mount_failed = false
   };
   ```
2. **Mount Process**
   - Attempts to mount existing SPIFFS partition
   - No auto-formatting to prevent data loss
   - Logs mount success/failure with size info

### `parameters_html_handler(httpd_req_t *req)`
**Parameters:**
- `req`: HTTP request context
**Returns:** `esp_err_t`  
**Internal Logic:**
1. **File Reading**
   - Opens "/spiffs/parameters.html"
   - Reads entire file into memory buffer
   - Maximum size: 8KB for web page content
2. **HTTP Response**
   - Sets "text/html" content type
   - Sends complete file content
   - Proper error handling for file operations

### `params_json_get_handler(httpd_req_t *req)`
**Parameters:**
- `req`: HTTP request context
**Returns:** `esp_err_t`  
**Internal Logic:**
1. **JSON Construction**
   ```json
   {
     "wifi_ssid": "current_ssid",
     "wifi_pass": "current_password", 
     "mqtt_broker_url": "mqtt://broker:1883",
     "sample_interval_ms": 5000,
     "bms_topic": "BMS/JKBMS"
   }
   ```
2. **Data Source**
   - Reads from global configuration variables
   - No NVS access (uses cached values)
3. **Response Formatting**
   - "application/json" content type
   - Compact JSON formatting

### `params_update_post_handler(httpd_req_t *req)`
**Parameters:**
- `req`: HTTP request with form data
**Returns:** `esp_err_t`  
**Internal Logic:**
1. **Content-Length Validation**
   - Limits POST data to 1024 bytes
   - Prevents buffer overflow attacks
2. **Form Data Reading**
   - `httpd_req_recv()` into buffer
   - Null-terminates received data
3. **URL Decoding Process**
   - Parses "key=value&key=value" format
   - `url_decode()` for percent-encoded values
   - Handles spaces, special characters
4. **Parameter Extraction**
   - WiFi SSID: "wifi_ssid=" parameter
   - WiFi Password: "wifi_pass=" parameter  
   - MQTT URL: "mqtt_broker_url=" parameter
   - Sample Interval: "sample_interval_ms=" parameter
   - BMS Topic: "bms_topic=" parameter
5. **Validation & Storage**
   - Length validation for each parameter
   - Updates global variables
   - Calls individual NVS save functions
   - Sets reboot flag for WiFi changes
6. **Response Generation**
   - JSON response with save status
   - Indicates if reboot required

### `captive_redirect_handler(httpd_req_t *req)`
**Parameters:**
- `req`: HTTP request for any URL
**Returns:** `esp_err_t`  
**Internal Logic:**
1. **HTTP Redirect Response**
   - Status: 302 Found
   - Location header: "http://192.168.4.1/parameters.html"
   - Forces all requests to configuration page
2. **Captive Portal Behavior**
   - Intercepts all HTTP traffic in AP mode
   - Essential for mobile device portal detection

---

## Access Point & Configuration Mode

### `ap_config_task(void *pvParameter)`
**Parameters:**
- `pvParameter`: Task parameter (unused)
**Returns:** void (task function)  
**Internal Logic:**
1. **Task Watchdog Management**
   - `esp_task_wdt_add(NULL)` - Add current task
   - Periodic `esp_task_wdt_reset()` calls
2. **Access Point Setup**
   ```c
   wifi_config_t wifi_config = {
       .ap = {
           .ssid = "ESP32-Config-XXXXXX",     // MAC-based SSID
           .ssid_len = strlen(ap_ssid),
           .channel = 1,
           .password = "",                     // Open network
           .max_connection = 4,                // Concurrent clients
           .authmode = WIFI_AUTH_OPEN,
           .ssid_hidden = 0,
           .beacon_interval = 100
       }
   };
   ```
3. **Network Interface Creation**
   - `esp_netif_create_default_wifi_ap()`
   - Static IP: 192.168.4.1
4. **HTTP Server Configuration**
   ```c
   httpd_config_t config = {
       .task_priority = tskIDLE_PRIORITY+5,
       .stack_size = 8192,
       .server_port = 80,
       .ctrl_port = 32768,
       .max_open_sockets = 7,
       .max_uri_handlers = 8,
       .max_resp_headers = 8,
       .backlog_conn = 5,
       .lru_purge_enable = false,
       .recv_wait_timeout = 5,
       .send_wait_timeout = 5
   };
   ```
5. **URI Handler Registration**
   - GET /parameters.html → Main config page
   - GET /params.json → Current settings
   - POST /params_update → Save new settings
   - GET /wifi_scan.json → Available networks
   - GET /* (wildcard) → Captive redirect
6. **DNS Hijacking Task**
   - Creates separate task for DNS interception
   - Handles all DNS queries in AP mode
7. **Configuration Loop**
   - Loads BMS topic from NVS
   - Infinite loop with watchdog feeding
   - Task cleanup on shutdown

---

## Network Utilities

### `dns_hijack_task(void *pvParameter)`
**Parameters:**
- `pvParameter`: Task parameter (unused)
**Returns:** void (task function)  
**Internal Logic:**
1. **UDP Socket Creation**
   - `socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)`
   - Binds to port 53 (DNS) on all interfaces
   - 1-second receive timeout for periodic checks
2. **DNS Request Processing**
   - Receives DNS queries via `recvfrom()`
   - Minimal DNS response construction:
     * Copies query ID and question section
     * Sets response flags: QR=1, AA=1, RCODE=0
     * Answer count = 1
3. **Response Generation**
   - Answer section format:
     * Name pointer: 0xC00C (points to query name)
     * Type: A (0x0001), Class: IN (0x0001)
     * TTL: 60 seconds
     * RDATA: 192.168.4.1 (AP IP address)
   - Sends response to original requester
4. **Watchdog Management**
   - `esp_task_wdt_reset()` after each request
   - Handles timeout periods gracefully
   - Checks `system_shutting_down` flag

### `url_decode(char *dst, const char *src, size_t dstsize)`
**Parameters:**
- `dst`: Destination buffer for decoded string
- `src`: Source URL-encoded string
- `dstsize`: Maximum destination buffer size
**Returns:** void (in-place decoding)  
**Internal Logic:**
1. **Character Processing Loop**
   - Percent encoding: %XX → single character
   - Plus encoding: + → space character
   - Regular characters: copied directly
2. **Hex Conversion**
   - `hex2int()` for %XX sequences
   - Validates hex digits before conversion
   - Handles invalid sequences gracefully
3. **Buffer Safety**
   - Respects destination buffer limits
   - Always null-terminates result

### `hex2int(char c)`
**Parameters:**
- `c`: Hexadecimal character ('0'-'9', 'A'-'F', 'a'-'f')
**Returns:** `int` (0-15 value)  
**Internal Logic:**
```c
if (c >= '0' && c <= '9') return c - '0';      // 0-9
if (c >= 'A' && c <= 'F') return c - 'A' + 10; // A-F
if (c >= 'a' && c <= 'f') return c - 'a' + 10; // a-f
return 0;  // Invalid character
```

---

## System Health & Monitoring

### `blink_heartbeat()`
**Parameters:** None  
**Returns:** void  
**Internal Logic:**
1. **GPIO Operations**
   - `gpio_set_level(GPIO_NUM_2, 1)` - LED on
   - `vTaskDelay(pdMS_TO_TICKS(100))` - 100ms delay
   - `gpio_set_level(GPIO_NUM_2, 0)` - LED off
2. **Timing**
   - Brief 100ms flash for visual confirmation
   - Non-blocking operation in task context
3. **Purpose**
   - Called only on successful MQTT publish
   - Visual indicator of system health

---

## Key Data Structures

### `bms_data_t`
```c
typedef struct {
    float pack_voltage;        // Total pack voltage (V)
    float pack_current;        // Pack current (A, + = charging)
    uint8_t soc_percent;       // State of charge (0-100%)
    float cell_voltages[24];   // Individual cell voltages (V)
    int num_cells;             // Actual number of cells
    float min_cell_voltage;    // Lowest cell voltage
    float max_cell_voltage;    // Highest cell voltage  
    float cell_voltage_delta;  // Max - min cell voltage
    float mosfet_temp;         // MOSFET temperature (°C)
    float probe1_temp;         // Temperature probe 1 (°C)
    float probe2_temp;         // Temperature probe 2 (°C)
} bms_data_t;
```

### `bms_extra_field_t`
```c
typedef struct {
    uint8_t id;                // BMS field ID code
    uint32_t value;            // Numeric value
    int is_ascii;              // 1 if string field, 0 if numeric
    char strval[48];           // String value for ASCII fields
} bms_extra_field_t;
```

### `bms_idcode_entry_t`
```c
typedef struct {
    uint8_t id;                // Field ID (0x79, 0xB4, etc.)
    const char *name;          // Human-readable name
    int byte_len;              // Field length in bytes
    const char *type;          // Data type ("HEX", "Code", "Column")
    const char *description;   // Field description
} bms_idcode_entry_t;
```

---

## Global Variables

```c
// Configuration Storage
static char wifi_ssid[33] = DEFAULT_WIFI_SSID;
static char wifi_pass[65] = DEFAULT_WIFI_PASS;  
static char mqtt_broker_url[128] = DEFAULT_MQTT_BROKER_URL;
char bms_topic[41] = "BMS/JKBMS";
static long sample_interval_ms = DEFAULT_SAMPLE_INTERVAL;

// Runtime State
static esp_mqtt_client_handle_t mqtt_client;
static bms_data_t current_bms_data;
static bms_extra_field_t extra_fields[MAX_EXTRA_FIELDS];
static int extra_fields_count = 0;
static bool mqtt_publish_success = false;
static bool system_shutting_down = false;
static int wifi_retry_num = 0;

// Hardware Constants
#define UART_NUM UART_NUM_2
#define UART_TX_PIN GPIO_NUM_17
#define UART_RX_PIN GPIO_NUM_16
#define BOOT_BTN_GPIO GPIO_NUM_0
#define UART_BUF_SIZE 1024
#define MAX_EXTRA_FIELDS 32
```

---

## BMS Protocol Details

### Command Structure
```
Offset | Length | Value    | Description
-------|--------|----------|---------------------------
0      | 1      | 0xAA     | Start byte 1
1      | 1      | 0x55     | Start byte 2  
2      | 1      | 0x90     | Command type
3      | 1      | 0xEB     | Data request
```

### Response Frame Format
```
Offset | Length | Description
-------|--------|-----------------------------------
0-1    | 2      | Start frame (0x4E 0x57)
2-3    | 2      | Frame length (big-endian)
4-7    | 4      | Terminal number (0x00000000)
8      | 1      | Command word (0x00)
9      | 1      | Frame source (0x00) 
10     | 1      | Transmission type (0x00)
11+    | N      | Data payload (variable length)
End-5  | 1      | End ID (0x68)
End-4  | 4      | Checksum (CRC)
```

### Data Payload Structure
```
Offset | Length | Description
-------|--------|-----------------------------------
0      | 1      | 0x79 (Cell voltages ID)
1      | 1      | Number of cells
2+     | 2×N    | Cell voltages (big-endian mV)
After  | 2      | Pack voltage (big-endian 0.01V)
       | 2      | Pack current (big-endian 0.01A)
       | 1      | Reserved
       | 1      | SOC percentage
       | 2      | MOSFET temperature (0.1°C)
       | 2      | Probe 1 temperature (0.1°C) 
       | 2      | Probe 2 temperature (0.1°C)
       | ...    | Additional fields with ID codes
```

This comprehensive analysis provides complete implementation details for understanding, debugging, and modifying the firmware behavior.

/*
 * CONDITIONAL LOGGING CONFIGURATION
 * 
 * To enable debug logging for this specific file, uncomment the line below.
 * This allows ESP_LOGD() calls to be compiled in and displayed when the 
 * global log level is set to DEBUG or VERBOSE.
 * 
 * Log Levels (from highest to lowest priority):
 * - ESP_LOG_ERROR   (E) - Error conditions
 * - ESP_LOG_WARN    (W) - Warning conditions  
 * - ESP_LOG_INFO    (I) - Informational messages
 * - ESP_LOG_DEBUG   (D) - Debug messages
 * - ESP_LOG_VERBOSE (V) - Verbose debug messages
 * 
 * To set global log level in menuconfig:
 * Component config > Log output > Default log verbosity
 * 
 * To set at runtime: esp_log_level_set("*", ESP_LOG_DEBUG);
 */
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

// Standard C input/output library
#include <stdio.h>
// FreeRTOS real-time operating system definitions
#include "freertos/FreeRTOS.h"
// FreeRTOS task management
#include "freertos/task.h"
// ESP32 UART driver
#include "driver/uart.h"
// ESP32 GPIO driver
#include "driver/gpio.h"
// ESP-IDF logging library
#include "esp_log.h"
// Standard C string manipulation library (for memcmp)
#include <string.h> 

// Added for Wi-Fi and MQTT
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "mqtt_client.h"
#include "cJSON.h" // For JSON formatting
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <errno.h>
#include "esp_chip_info.h"
#include "esp_ota_ops.h"
#include "esp_flash.h"
#include "spi_flash_mmap.h"
#include "esp_partition.h"
#include "esp_http_server.h"

// Watchdog timer includes
#include "esp_task_wdt.h"

// System monitoring includes
#include "esp_netif.h"

// Software watchdog variables
static uint32_t watchdog_timeout_ms = 0;  // Watchdog timeout (10× sample interval)
static TickType_t last_successful_publish = 0;  // Last successful MQTT publish time
static bool watchdog_enabled = false;  // Whether watchdog is enabled
static bool mqtt_publish_success = false;  // Flag for successful MQTT publish

// Debug logging flag - set to false to reduce log output (except watchdog logs)
static bool debug_logging = false;

#define WIFI_NVS_NAMESPACE "wifi_cfg"
#define WIFI_NVS_KEY_SSID "ssid"
#define WIFI_NVS_KEY_PASS "pass"
#define MQTT_NVS_KEY_URL "broker_url"
#define NVS_KEY_SAMPLE_INTERVAL "sample_interval"
#define NVS_KEY_BMS_TOPIC "BMS_Topic"
#define NVS_KEY_WATCHDOG_COUNTER "watchdog_cnt"
#define NVS_KEY_PACK_NAME "pack_name"
#define DEFAULT_WIFI_SSID "yourSSID"
#define DEFAULT_WIFI_PASS "yourpassword"
#define DEFAULT_MQTT_BROKER_URL "mqtt://192.168.1.100"
#define DEFAULT_SAMPLE_INTERVAL 5000L
#define DEFAULT_PACK_NAME "Set in Parameters"

static char wifi_ssid[33] = DEFAULT_WIFI_SSID;
static char wifi_pass[65] = DEFAULT_WIFI_PASS;
static char mqtt_broker_url[128] = DEFAULT_MQTT_BROKER_URL;
char bms_topic[41] = "BMS/JKBMS";  // Remove static to make it globally accessible
static long sample_interval_ms = DEFAULT_SAMPLE_INTERVAL;
static uint32_t watchdog_reset_counter = 0;  // Watchdog timer reset counter
static char pack_name[64] = DEFAULT_PACK_NAME;  // Pack name field

// Define the UART peripheral number to be used (UART2 in this case)
#define UART_NUM UART_NUM_2
// Define the GPIO pin for UART Transmit (TX)
#define UART_TX_PIN GPIO_NUM_17
// Define the GPIO pin for UART Receive (RX)
#define UART_RX_PIN GPIO_NUM_16
#define BOOT_BTN_GPIO GPIO_NUM_0
// Define the size of the UART buffer for receiving data
#define UART_BUF_SIZE 1024

// Tag used for logging messages from this module
static const char *TAG = "BMS_READER";

// Wi-Fi Configuration
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

// MQTT Configuration
#define MQTT_BROKER_URL "mqtt://192.168.1.5"
#define MQTT_TOPIC_PACK "NodeJKBMS2/pack"
#define MQTT_TOPIC_CELLS "NodeJKBMS2/cells"

// Global variable for MQTT client
static esp_mqtt_client_handle_t mqtt_client;
static int wifi_retry_num = 0;

// Placeholder structures for BMS data
// This will be populated with more fields later
typedef struct {
    float pack_voltage;
    float pack_current;
    uint8_t soc_percent;
    // Cell voltages will be handled separately or in an array here
    float cell_voltages[24]; // Assuming max 24 cells for now
    int num_cells;
    float min_cell_voltage;
    float max_cell_voltage;
    float cell_voltage_delta;
    float mosfet_temp;
    float probe1_temp;
    float probe2_temp;
    // Add common system status fields
    uint8_t charge_mosfet_status;
    uint8_t discharge_mosfet_status;
    uint8_t balancing_active;
    uint32_t battery_capacity_ah;
    uint32_t remaining_capacity_ah;
    uint8_t num_temp_sensors;
    uint8_t battery_type;
    uint8_t low_capacity_alarm;
    // Add other fields from your target JSON here as they are parsed
    // e.g., uint16_t pack_rate_cap;
    // ...
} bms_data_t;

// Global instance of BMS data
static bms_data_t current_bms_data;


// --- Data Identification Code Table (auto-generated from CSV) ---
typedef struct {
    uint8_t id;
    const char *name;
    int byte_len;
    const char *type;
    const char *info;
} bms_idcode_t;

static const bms_idcode_t bms_idcodes[] = {
    {0x99, "Equalizing opening differential", 2, "HEX", "10 - 1000 MV"},
    {0x9a, "Active equalization switch", 1, "HEX", "0 off or 1 on"},
    {0x9b, "Power tube temperature protection value", 2, "HEX", "0 - 100 °C"},
    {0x9f, "Temperature protection value in battery box", 2, "HEX", "0 - 100 °C"},
    {0xa0, "Recovery value 2 of battery in box", 2, "HEX", "40 - 100 °C"},
    {0xa1, "Battery temperature difference", 2, "HEX", "40 -- 100 °C"},
    {0xa2, "Battery charging 2 high temperature protection value", 2, "HEX", "5-20 °C"},
    {0xa3, "High Temperature Protection Value for Battery Charging", 2, "HEX", "0-100 °C Y"},
    {0xa4, "High Temperature Protection Value for Battery Discharge", 2, "HEX", "0 - 100 °C"},
    {0xa5, "Charging cryoprotection value", 2, "HEX", "- 45 °C /+ 25 °C(No datum - signed data)"},
    {0xa6, "Recovery value 2 of charge cryoprotection", 2, "HEX", "- 45 °C /+ 25 °C(No datum - signed data)"},
    {0xa7, "Discharge cryoprotection value", 2, "HEX", "- 45 °C /+ 25 °C(No datum - signed data)"},
    {0xa8, "Discharge Low Temperature Protection Recovery Value", 2, "HEX", "- 45 °C /+ 25 °C(No datum - signed data)"},
    {0xa9, "Number of battery strings settings", 1, "HEX", "13 - 32"},
    {0xaa, "Battery Capacity Settings", 4, "HEX", "AH"},
    {0xab, "Charging MOS switch", 1, "HEX", "OFF 1 ON"},
    {0xac, "Discharge MOS switch", 1, "HEX", "OFF 1 ON"},
    {0xad, "Current Calibration", 2, "HEX", "100 MA- 20000 MA"},
    {0xae, "Protective Board 1 Address", 1, "HEX", "This site is reserved and used in cascade"},
    {0xaf, "Battery type", 1, "HEX", "0: lithium iron phosphate, 1: ternary, 2: lithium titanate"},
    {0xb0, "Sleep Wait Time", 2, "HEX", "Second data, for reference"},
    {0xb1, "Low Capacity Alarm Value", 1, "HEX", "0 - 80%"},
    {0xb2, "Modify parameter password", 10, "HEX", "For temporary reference, fix a password"},
    {0xb3, "Special Charger 1 Switch", 1, "HEX", "0 OFF 1 ON"},
    {0xb4, "Device ID Code", 8, "Code", "Example 60300001 ..."},
    {0xb5, "Date of production", 4, "Code", "Example 2004 ..."},
    {0xb6, "System working time", 4, "HEX", "Reset when leaving the factory, unit: Min"},
    {0xb7, "Software Version Number", 15, "Code", "NW_1_0_0_200428"},
    {0xb8, "Start Current Calibration", 1, "HEX", "1: Start Calibration 0: Turn off calibration"},
    {0xb9, "Actual battery capacity", 4, "HEX", "Code AH"},
    {0xba, "Naming of factory ID", 24, "Column", "BT 3072020120000200521001 ..."},
};
#define BMS_IDCODES_COUNT (sizeof(bms_idcodes)/sizeof(bms_idcodes[0]))

// --- Store decoded extra fields for MQTT output ---
typedef struct {
    uint8_t id;
    uint32_t value;
    char strval[48]; // For ASCII fields
    int is_ascii;
} bms_extra_field_t;
#define MAX_EXTRA_FIELDS 32
static bms_extra_field_t extra_fields[MAX_EXTRA_FIELDS];
static int extra_fields_count = 0;

// Forward declarations for functions defined later in this file

// Forward declarations for NVS functions
static void load_watchdog_counter_from_nvs(void);
static void save_watchdog_counter_to_nvs(void);
static void load_pack_name_from_nvs(void);

// Forward declaration for DNS hijack task
void dns_hijack_task(void *pvParameter);

void url_decode(char *dst, const char *src, size_t dstsize);

// Forward declarations for HTTP handlers
static esp_err_t params_json_get_handler(httpd_req_t *req);
static esp_err_t params_update_post_handler(httpd_req_t *req);
static esp_err_t parameters_html_handler(httpd_req_t *req);
static esp_err_t captive_redirect_handler(httpd_req_t *req);
static esp_err_t sysinfo_json_get_handler(httpd_req_t *req);
static esp_err_t ota_firmware_handler(httpd_req_t *req);
static esp_err_t ota_filesystem_handler(httpd_req_t *req);

// Initializes the UART communication peripheral.
void init_uart();
// Sends a command to the BMS via UART.
void send_bms_command(const uint8_t* cmd, size_t len);
// Reads data from the BMS via UART. This function is static, meaning it's only visible within this file.
static void read_bms_data(); 
// Parses the raw data received from the BMS and prints it in a human-readable format.
void parse_and_print_bms_data(const uint8_t *data, int len);

// New forward declarations for Wi-Fi and MQTT
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
static void mqtt_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
void wifi_init_sta(void);
static void mqtt_app_start(void);
void publish_bms_data_mqtt(const bms_data_t *bms_data_ptr); // Combined publish function for now

// Forward declaration for DNS hijack task
void dns_hijack_task(void *pvParameter);

// Constant array holding the "Read All Data" command bytes to be sent to the JK BMS.
// This command requests a comprehensive status update from the BMS.
const uint8_t bms_read_all_cmd[] = {
    0x4E, 0x57, // Start Frame
    0x00, 0x13, // Length (2 bytes, value 19: from this byte to end of checksum)
    0x00, 0x00, 0x00, 0x00, // Terminal Number (4 bytes, typically 0)
    0x06,       // Command Word (0x06 for "Read All Data")
    0x03,       // Frame Source (e.g., 0x03 for host computer)
    0x00, 0x00, // Data: Cell number (start) - 0x0000 for all cells
    0x00, 0x00, // Data: Cell number (end) - 0x0000 for all cells
    0x00, 0x00, // Reserved
    0x68,       // End ID
    0x00, 0x00, // Checksum (placeholder, actual CRC is calculated over previous bytes)
    0x01, 0x29  // Checksum (CRC16 of bytes from Start Frame up to End ID)
                // 0x0129 = 297. Sum of (4E+57+00+13+00+00+00+00+06+03+00+00+00+00+00+00+68) = 297
};

// Helper function to extract a 16-bit unsigned integer from a buffer.
// Assumes big-endian byte order (most significant byte first).
// buffer: Pointer to the byte array.
// offset: Starting index in the buffer from where to read the 16-bit integer.
// Returns the extracted 16-bit unsigned integer.
uint16_t unpack_u16_be(const uint8_t *buffer, int offset) {
    // Combine two consecutive bytes into a 16-bit value.
    // buffer[offset] is the most significant byte, buffer[offset+1] is the least significant.
    return ((uint16_t)buffer[offset] << 8) | buffer[offset + 1];
}

// Helper function to extract an 8-bit unsigned integer (a single byte) from a buffer.
// buffer: Pointer to the byte array.
// offset: Index in the buffer from where to read the 8-bit integer.
// Returns the extracted 8-bit unsigned integer.
uint8_t unpack_u8(const uint8_t *buffer, int offset) {
    // Directly return the byte at the specified offset.
    return buffer[offset];
}

// Initializes the UART peripheral for communication with the BMS.
void init_uart() {
    // UART configuration structure
    uart_config_t uart_config = {
        .baud_rate = 115200,                // Baud rate for serial communication
        .data_bits = UART_DATA_8_BITS,      // 8 data bits per frame
        .parity    = UART_PARITY_DISABLE,   // No parity bit
        .stop_bits = UART_STOP_BITS_1,      // 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // No hardware flow control
        .source_clk = UART_SCLK_APB,        // Clock source for UART (APB clock)
    };
    // Install UART driver, and get the queue.
    // UART_NUM: UART port number
    // UART_BUF_SIZE * 2: RX buffer size (doubled for safety)
    // 0: TX buffer size (0 means driver will not allocate TX buffer, direct write)
    // 0, NULL: No event queue
    // 0: Interrupt allocation flags
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    // Set UART pins for TX, RX, RTS, CTS
    // UART_TX_PIN: TX pin number
    // UART_RX_PIN: RX pin number
    // UART_PIN_NO_CHANGE: RTS pin (not used)
    // UART_PIN_NO_CHANGE: CTS pin (not used)
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "UART initialized");
}

// Sends a command (array of bytes) to the BMS via UART.
// cmd: Pointer to the byte array containing the command.
// len: Length of the command in bytes.
void send_bms_command(const uint8_t* cmd, size_t len) {
    // Write data to UART.
    // (const char*)cmd: Cast command buffer to char pointer as expected by the function.
    int txBytes = uart_write_bytes(UART_NUM, (const char*)cmd, len);
    ESP_LOGI(TAG, "Wrote %d bytes", txBytes); // Log the number of bytes written.
}

// Parses the raw data received from the BMS and prints it.
// data: Pointer to the buffer containing the raw data from BMS.
// len: Length of the data in bytes.
void parse_and_print_bms_data(const uint8_t *data, int len) {
    // Detailed frame format based on JK BMS RS485 Protocol documentation and Python examples:
    // Offset | Length | Description
    // -------|--------|----------------------------------------------------
    // 0      | 2      | Start Frame (Fixed: 0x4E, 0x57)
    // 2      | 2      | Length (Big-endian, from this field to the end of the checksum)
    // 4      | 4      | Terminal Number (Usually 0x00000000)
    // 8      | 1      | Command Word (e.g., 0x00 for response to read all)
    // 9      | 1      | Frame Source (e.g., 0x00 for BMS)
    // 10     | 1      | Transmission Type (e.g., 0x00 for normal data)
    // 11     | N      | Frame Info / Data Payload (Actual BMS data, variable length)
    // 11+N   | 4      | Record Number (Usually 0x00000000, can be part of some frames)
    // 11+N+4 | 1      | End ID (Fixed: 0x68)
    // 11+N+5 | 4      | Checksum (CRC, last 2 bytes are the sum, first 2 are 0x0000)
    // Total length = 2 (Start) + Length_Field_Value

    // Minimum length check for a valid frame.
    // Smallest possible payload could be just a few bytes.
    // Header (2) + Length (2) + Terminal (4) + Cmd (1) + Src (1) + Type (1) = 11 bytes
    // Trailer: RecordNum (4, optional but often present) + EndID (1) + Checksum (4) = 9 bytes
    // So, 11 (header) + min_payload (e.g. 1 byte for simple ack) + 9 (trailer) approx.
    // The python script implies payload starts at 11 and ends at process_len - 9 (excluding RecNum, EndID, CRC).
    // Minimum frame with 0 payload data: 2(start)+2(len_val)+4(term)+1(cmd)+1(src)+1(type) + 4(rec_num)+1(end_id)+4(crc) = 19 bytes.
    // The length field itself is part of this.
    // If frame_len_field is minimal (e.g. for header + trailer only, no payload), it would be 4+1+1+1+4+1+4 = 15.
    if (len < 15) { 
        ESP_LOGW(TAG, "Frame too short for basic structure: %d bytes", len);
        return;
    }

    // 1. Check Start Frame (must be 0x4E, 0x57)
    if (data[0] != 0x4E || data[1] != 0x57) {
        ESP_LOGW(TAG, "Invalid start frame: %02X %02X", data[0], data[1]);
        return;
    }
    ESP_LOGI(TAG, "Start frame OK");

    // 2. Get Length field from the frame.
    // This length is from the length field itself to the end of the checksum.
    uint16_t frame_len_field = unpack_u16_be(data, 2); 
    ESP_LOGI(TAG, "Frame length field value: %d", frame_len_field);

    // Validate if the received data length (`len`) is sufficient for the declared frame length.
    // Total expected length = 2 (Start Frame bytes) + frame_len_field.
    if (len < (frame_len_field + 2)) { 
        ESP_LOGW(TAG, "Incomplete frame. Expected header(2) + frame_len_field(%d) = %d bytes, but got %d bytes in total.", 
                 frame_len_field, frame_len_field + 2, len);
        return;
    }
    // Determine the length of the frame to process.
    // This is either the declared total length or the received length, whichever is smaller (to prevent overruns).
    // However, the previous check should ensure `len` is at least `declared_total_len`.
    int declared_total_len = 2 + frame_len_field; 
    int process_len = (declared_total_len < len) ? declared_total_len : len;


    // 3. Calculate and Verify CRC (Checksum)
    // The JK BMS uses a simple sum CRC.
    // The CRC is calculated over the bytes from the Start Frame (0x4E) up to, but not including,
    // the 4-byte checksum field itself.
    // The actual 16-bit CRC sum is stored in the last 2 bytes of this 4-byte field (i.e., at offset process_len - 2).
    // (The first 2 bytes of the checksum field are often 0x0000).

    // Minimum length for CRC calculation: Header (11 bytes) + Trailer before CRC (RecNum(4) + EndID(1) = 5 bytes) + CRC (4 bytes) = 20 bytes.
    // So, process_len must be at least 20 for a valid payload structure and CRC.
    // If process_len is less than 4, we can't even read the CRC.
    if (process_len < 4) {
        ESP_LOGW(TAG, "Frame too short to contain CRC: %d bytes", process_len);
        return;
    }
    // The python script sums `data[0:-4]`, meaning all bytes except the last four.
    // So, we sum `process_len - 4` bytes.
    uint32_t crc_calc = 0; // Use uint32_t for sum to avoid overflow before casting to uint16_t.
    for (int i = 0; i < process_len - 4; i++) {
        crc_calc += data[i];
    }

    // Extract the 16-bit CRC value received in the frame.
    // It's located in the last 2 bytes of the 4-byte checksum field (i.e., at offset process_len - 2).
    uint16_t crc_received = unpack_u16_be(data, process_len - 2);

    ESP_LOGI(TAG, "Calculated CRC sum (16-bit): %u (0x%04X)", (uint16_t)crc_calc, (uint16_t)crc_calc);
    ESP_LOGI(TAG, "Received CRC from frame: %u (0x%04X)", crc_received, crc_received);

    // Compare calculated CRC with received CRC.
    if ((uint16_t)crc_calc != crc_received) {
        ESP_LOGE(TAG, "CRC mismatch! Calculated: 0x%04X, Received: 0x%04X. Frame might be corrupt.", 
                 (uint16_t)crc_calc, crc_received);
        // return; // Optionally, uncomment to discard frames with bad CRC. For debugging, we might proceed.
    } else {
        ESP_LOGI(TAG, "CRC OK");
    }

    // 4. Extract Data Payload (Frame Info section)
    // The payload starts after the initial header fields:
    // Start Frame (2) + Length (2) + Terminal Number (4) + Command Word (1) + Frame Source (1) + Transmission Type (1) = 11 bytes.
    // The payload ends before the trailer fields:
    // Record Number (4) + End ID (1) + Checksum (4) = 9 bytes.
    // So, payload_len = process_len - (offset_of_payload) - (length_of_trailer)
    // payload_len = process_len - 11 - 9 = process_len - 20.

    // Check if frame is long enough to contain any payload.
    if (process_len < 20) { // 11 (header) + 9 (trailer) = 20. If less, no payload.
        ESP_LOGW(TAG, "Frame too short for data payload (less than 20 bytes): %d", process_len);
        return;
    }
    // Pointer to the start of the payload.
    const uint8_t *payload = &data[11];
    // Calculate the length of the payload.
    int payload_len = process_len - 20; 
    
    ESP_LOGI(TAG, "Payload length: %d bytes", payload_len);
    if (payload_len <= 0) {
        ESP_LOGW(TAG, "No actual payload data (payload_len <= 0).");
        // If CRC was OK, this might be an ack or a frame with no data content.
        // Depending on the command sent, this might be expected.
        return; 
    }
    // Log the hexadecimal representation of the payload for debugging.
    ESP_LOGI(TAG, "Payload HEX:");
    ESP_LOG_BUFFER_HEX(TAG, payload, payload_len);

    // Initialize current_bms_data fields to default/invalid values
    current_bms_data.num_cells = 0;
    current_bms_data.min_cell_voltage = 5.0f;
    current_bms_data.max_cell_voltage = 0.0f;


    // 5. Parse specific data fields from the payload.
    // This parsing is based on the "Read All Data" (0x06) command response structure.
    // The first byte of the payload for this response is typically 0x79 (Cell Voltages ID).

    // Guard against empty payload before accessing payload[0].
    if (payload_len == 0) { 
        ESP_LOGW(TAG, "Payload is empty, cannot parse specific fields.");
        return;
    }
    // Check if payload starts with the expected Cell Voltages ID (0x79).
    if (payload[0] != 0x79) {
        ESP_LOGW(TAG, "Payload does not start with 0x79 (Cell Voltages ID), instead: 0x%02X. Parsing may be incorrect for this response.", payload[0]);
        // Depending on the actual command/response, this might not be an error.
        // For "Read All Data", 0x79 is expected.
        // return; // Optionally stop if the structure is not as expected.
    }

    // 5.1 Parse Cell Voltages (ID 0x79)
    // Structure: 0x79 (ID) | ByteCount (1 byte) | Cell1_Hi | Cell1_Lo | Cell1_Info | Cell2_Hi | ...
    // Python: bytecount = data[1] (relative to payload start)
    // Python: cellcount = int(bytecount/3)
    // Python: Voltages start at payload[2], each voltage uses 3 bytes.
    //         The actual mV value is in the last 2 bytes of these 3 (skip 1, read 2 as u16_be).

    if (payload_len < 2) { // Need at least ID (already checked) and ByteCount.
        ESP_LOGW(TAG, "Payload too short for cell voltage byte count (needs at least 2 bytes).");
        return;
    }
    uint8_t cell_voltage_bytecount = payload[1]; // Number of bytes used for all cell voltage data.
    int num_cells = cell_voltage_bytecount / 3;  // Each cell's data is 3 bytes long.
    ESP_LOGI(TAG, "Cell voltage data byte count from payload: %d, Calculated number of cells: %d", cell_voltage_bytecount, num_cells);

    // Check if payload is long enough for all declared cell voltages.
    // Expected length = 2 (ID + ByteCount) + cell_voltage_bytecount.
    if (payload_len < (2 + cell_voltage_bytecount)) {
        ESP_LOGW(TAG, "Payload too short for all cell voltages. Expected %d bytes for cell data part, got %d bytes in payload.", 
                 (2 + cell_voltage_bytecount), payload_len);
        // We might still try to parse what's available if num_cells > 0 and data seems partially there.
        // For now, return if data is clearly insufficient.
        return;
    }

    if (debug_logging) printf("Cell Voltages (V):\n");
    float min_cell_voltage = 5.0f; // Initialize with a high value to find the minimum.
    float max_cell_voltage = 0.0f; // Initialize with a low value to find the maximum.

    for (int i = 0; i < num_cells; i++) {
        // Offset for current cell's data within payload: 2 (ID+ByteCount) + i * 3 (3 bytes per cell).
        // Voltage is in bytes 2 and 3 of these 3 bytes (i.e., skip the first byte of the 3-byte group).
        // So, offset for mV value is payload_start + 2 + i*3 + 1.
        int cell_data_start_offset = 2 + i * 3;
        if ( (cell_data_start_offset + 2) < payload_len) { // Check bounds: need 3 bytes for cell i, last is at offset+2
             uint16_t voltage_mv = unpack_u16_be(payload, cell_data_start_offset + 1); 
             float voltage_v = voltage_mv / 1000.0f; // Convert mV to V.
             if (debug_logging) printf("  Cell %2d: %.3f V\n", i + 1, voltage_v);
             // Store individual cell voltage
             if (i < 24) { // Ensure we don't write out of bounds of our array
                 current_bms_data.cell_voltages[i] = voltage_v;
             }
             // Update min/max cell voltages, ensuring voltage_mv > 0 to avoid uninitialized cells if BMS reports them as 0.
             if (voltage_mv > 0 && voltage_v < min_cell_voltage) min_cell_voltage = voltage_v;
             if (voltage_v > max_cell_voltage) max_cell_voltage = voltage_v;
        } else {
            ESP_LOGW(TAG, "Not enough data in payload for cell %d voltage. Stopping cell voltage parsing.", i + 1);
            break; // Stop if data runs out.
        }
    }
    // Print Min, Max, and Delta cell voltages if cells were processed.
    if (num_cells > 0) {
        if (min_cell_voltage > 4.9f) min_cell_voltage = 0.0f; // If min_cell_voltage wasn't updated, set to 0.
        if (debug_logging) {
            printf("Min Cell Voltage: %.3f V\n", min_cell_voltage);
            printf("Max Cell Voltage: %.3f V\n", max_cell_voltage);
            printf("Cell Voltage Delta: %.3f V\n", max_cell_voltage - min_cell_voltage);
        }
        // Store in global struct
        current_bms_data.num_cells = num_cells;
        current_bms_data.min_cell_voltage = min_cell_voltage;
        current_bms_data.max_cell_voltage = max_cell_voltage;
        current_bms_data.cell_voltage_delta = max_cell_voltage - min_cell_voltage;
    }
    
    // current_offset tracks the position in the payload after the last parsed field.
    // Initialized to after the cell voltage block: ID (1) + ByteCount (1) + cell_voltage_bytecount.
    int current_offset = 2 + cell_voltage_bytecount;

    // 5.2 Parse Temperatures (IDs 0x80, 0x81, 0x82)
    // Each temperature field: ID (1 byte) + Temperature Data (2 bytes, u16_be).
    // Temperature conversion: If raw > 100, it's negative: -(raw - 100). Otherwise, it's raw. (As per Python example)
    // Protocol doc might say value/10 for degrees C, with 1000 offset for negative. (e.g. 900 = -10C, 1100 = 10C)
    // Sticking to Python example's simpler (raw > 100 ? -(raw-100) : raw) logic for now.

    // MOSFET Temperature (ID 0x80)
    if (payload_len > current_offset + 2 && payload[current_offset] == 0x80) { // Check ID and enough data (ID + 2 bytes)
        uint16_t temp_fet_raw = unpack_u16_be(payload, current_offset + 1);
        float temp_fet = (temp_fet_raw > 100) ? -(float)(temp_fet_raw - 100) : (float)temp_fet_raw;
        if (debug_logging) printf("MOSFET Temperature: %.1f C (raw: %u)\n", temp_fet, temp_fet_raw);
        current_bms_data.mosfet_temp = temp_fet; // Store data
        current_offset += 3; // Advance offset by ID (1) + Data (2).
    } else {
        ESP_LOGW(TAG, "MOSFET Temp (ID 0x80) data missing or wrong ID at payload offset %d. Found ID: 0x%02X", 
                 current_offset, payload_len > current_offset ? payload[current_offset] : 0xFF);
    }

    // Probe 1 Temperature (ID 0x81)
    if (payload_len > current_offset + 2 && payload[current_offset] == 0x81) {
        uint16_t temp_1_raw = unpack_u16_be(payload, current_offset + 1);
        float temp_1 = (temp_1_raw > 100) ? -(float)(temp_1_raw - 100) : (float)temp_1_raw;
        if (debug_logging) printf("Probe 1 Temperature: %.1f C (raw: %u)\n", temp_1, temp_1_raw);
        current_bms_data.probe1_temp = temp_1; // Store data
        current_offset += 3;
    } else {
        ESP_LOGW(TAG, "Probe 1 Temp (ID 0x81) data missing or wrong ID at payload offset %d. Found ID: 0x%02X", 
                 current_offset, payload_len > current_offset ? payload[current_offset] : 0xFF);
    }

    // Probe 2 Temperature (ID 0x82)
    if (payload_len > current_offset + 2 && payload[current_offset] == 0x82) {
        uint16_t temp_2_raw = unpack_u16_be(payload, current_offset + 1);
        float temp_2 = (temp_2_raw > 100) ? -(float)(temp_2_raw - 100) : (float)temp_2_raw;
        if (debug_logging) printf("Probe 2 Temperature: %.1f C (raw: %u)\n", temp_2, temp_2_raw);
        current_bms_data.probe2_temp = temp_2; // Store data
        current_offset += 3;
    } else {
        ESP_LOGW(TAG, "Probe 2 Temp (ID 0x82) data missing or wrong ID at payload offset %d. Found ID: 0x%02X", 
                 current_offset, payload_len > current_offset ? payload[current_offset] : 0xFF);
    }
    
    // 5.3 Parse Total Battery Voltage (ID 0x83)
    // Structure: ID (1 byte) + Total Voltage Data (2 bytes, u16_be, value in 0.01V).
    if (payload_len > current_offset + 2 && payload[current_offset] == 0x83) {
        uint16_t total_voltage_raw = unpack_u16_be(payload, current_offset + 1); 
        float total_v = total_voltage_raw / 100.0f;
        if (debug_logging) printf("Total Battery Voltage: %.2f V (raw: %u)\n", total_v, total_voltage_raw);
        current_bms_data.pack_voltage = total_v; // Store data
        current_offset += 3;
    } else {
        ESP_LOGW(TAG, "Total Voltage (ID 0x83) data missing or wrong ID at payload offset %d. Found ID: 0x%02X", 
                 current_offset, payload_len > current_offset ? payload[current_offset] : 0xFF);
    }

    // 5.4 Parse Current (ID 0x84)
    // Structure: ID (1 byte) + Current Data (2 bytes, u16_be).
    // Current interpretation varies based on `frame_len_field` as per `data_bms_full.py`.
    // Value is in 0.01A.
    if (payload_len > current_offset + 2 && payload[current_offset] == 0x84) {
        uint16_t current_raw = unpack_u16_be(payload, current_offset + 1);
        float current_a;

        ESP_LOGI(TAG, "Current parsing: frame_len_field=%u, current_raw=0x%04X (%u)", 
                 frame_len_field, current_raw, current_raw);

        // Logic from data_bms_full.py, swapped for alternative interpretation:
        // `frame_len_field` is the length from the BMS packet (from length field itself to end of CRC).
        if (frame_len_field < 260) {
            // Method 2 (Previously for frame_len_field >= 260): MSB indicates sign.
            // Python: if (value & 0x8000) == 0x8000 : current = (value & 0x7FFF)/100
            //         else : current = ((value & 0x7FFF)/100) * -1
            // This means if MSB is set, it's positive current (value & 0x7FFF).
            // If MSB is clear, it's negative current -(value & 0x7FFF).
            // Note: This is different from standard two\'s complement.
            if ((current_raw & 0x8000) == 0x8000) {
                current_a = (float)(current_raw & 0x7FFF) / 100.0f;
            } else {
                current_a = -((float)(current_raw & 0x7FFF) / 100.0f);
            }
            ESP_LOGI(TAG, "Current (method 2, frame_len_field < 260): %.2f A", current_a);
        } else {
            // Method 1 (Previously for frame_len_field < 260): (10000 - raw_value) * 0.01. This implies 10000 is zero current.
            // e.g. raw=10000 -> 0A. raw=9900 -> 1A. raw=10100 -> -1A.
            current_a = (float)(10000 - (int32_t)current_raw) * 0.01f;
            ESP_LOGI(TAG, "Current (method 1, frame_len_field >= 260): %.2f A", current_a);
        }

        if (debug_logging) printf("Current: %.2f A (raw: %u)\n", current_a, current_raw);
        current_bms_data.pack_current = current_a; // Store data
        current_offset += 3;
    } else {
        ESP_LOGW(TAG, "Current (ID 0x84) data missing or wrong ID at payload offset %d. Found ID: 0x%02X", 
                 current_offset, payload_len > current_offset ? payload[current_offset] : 0xFF);
    }

    // 5.5 Parse Remaining Capacity (SOC) (ID 0x85)
    // Structure: ID (1 byte) + SOC Data (1 byte, percentage).
    if (payload_len > current_offset + 1 && payload[current_offset] == 0x85) { // Need ID + 1 byte data.
        uint8_t soc_percent = unpack_u8(payload, current_offset + 1);
        if (debug_logging) printf("Remaining Capacity (SOC): %u%%\n", soc_percent);
        current_bms_data.soc_percent = soc_percent; // Store data
        current_offset += 2; // Advance by ID(1) + Data(1).
    } else {
        ESP_LOGW(TAG, "SOC (ID 0x85) data missing or wrong ID at payload offset %d. Found ID: 0x%02X", 
                 current_offset, payload_len > current_offset ? payload[current_offset] : 0xFF);
    }
    
    // --- Parse additional fields from Data Identification Codes table ---
    ESP_LOGI(TAG, "=== PARSING EXTRA FIELDS ===");
    ESP_LOGI(TAG, "Starting extra field parsing at offset %d, payload length: %d", current_offset, payload_len);
    
    // Debug: Show the raw data we're about to parse
    if (debug_logging && payload_len > current_offset) {
        ESP_LOGI(TAG, "Raw data to parse (next 50 bytes):");
        for (int debug_i = current_offset; debug_i < payload_len && debug_i < current_offset + 50; debug_i++) {
            printf("0x%02X ", payload[debug_i]);
            if ((debug_i - current_offset + 1) % 16 == 0) printf("\n");
        }
        printf("\n");
    }
    
    extra_fields_count = 0;
    int extra_offset = current_offset;
    while (extra_offset < payload_len && extra_fields_count < MAX_EXTRA_FIELDS) {
        uint8_t id = payload[extra_offset];
        if (debug_logging) ESP_LOGI(TAG, "Found field ID 0x%02X at offset %d", id, extra_offset);
        
        // Check if this field ID has already been processed
        bool already_exists = false;
        for (int j = 0; j < extra_fields_count; j++) {
            if (extra_fields[j].id == id) {
                already_exists = true;
                if (debug_logging) ESP_LOGW(TAG, "DUPLICATE DETECTED: Field ID 0x%02X already exists at index %d", id, j);
                break;
            }
        }
        
        if (!already_exists) {
            int found = 0;
            for (size_t i = 0; i < BMS_IDCODES_COUNT; ++i) {
                if (bms_idcodes[i].id == id) {
                    // Special debug for 0xB4
                    if (id == 0xB4) {
                        ESP_LOGI(TAG, "=== PROCESSING 0xB4 SPECIFICALLY ===");
                        ESP_LOGI(TAG, "Field bytes: %d, Type: %s", bms_idcodes[i].byte_len, bms_idcodes[i].type);
                        ESP_LOGI(TAG, "Available data: offset %d, payload_len %d", extra_offset, payload_len);
                    }
                    
                    uint32_t value = 0;
                    int bytes = bms_idcodes[i].byte_len;
                    const char *type = bms_idcodes[i].type;
                    int is_ascii = (type && (strcmp(type, "Code") == 0 || strcmp(type, "Column") == 0));
                    char strval[48] = {0};
                    if (bytes > 0 && (extra_offset + bytes) < payload_len) {
                        // Always calculate numeric value (needed for fallback hex string)
                        for (int b = 0; b < bytes; ++b) {
                            value = (value << 8) | payload[extra_offset + 1 + b];
                        }
                        
                        if (is_ascii) {
                            int copylen = (bytes < 47) ? bytes : 47;
                            memcpy(strval, &payload[extra_offset + 1], copylen);
                            strval[copylen] = '\0';
                            // Remove non-printable chars
                            for (int s = 0; s < copylen; ++s) {
                                if (strval[s] < 32 || strval[s] > 126) strval[s] = '\0';
                            }
                            
                            // Special debug for 0xB4
                            if (id == 0xB4) {
                                ESP_LOGI(TAG, "0xB4 raw bytes: %02X %02X %02X %02X %02X %02X %02X %02X", 
                                         payload[extra_offset + 1], payload[extra_offset + 2], payload[extra_offset + 3], payload[extra_offset + 4],
                                         payload[extra_offset + 5], payload[extra_offset + 6], payload[extra_offset + 7], payload[extra_offset + 8]);
                                ESP_LOGI(TAG, "0xB4 parsed string: '%s'", strval);
                            }
                            
                            // Force Device ID Code to be string even if parsing results in empty string
                            if (id == 0xB4 && strval[0] == '\0') {
                                snprintf(strval, sizeof(strval), "%08X", (unsigned int)value);
                                ESP_LOGI(TAG, "0xB4 forced to hex string: '%s'", strval);
                            }
                        }
                    } else {
                        if (id == 0xB4) {
                            ESP_LOGW(TAG, "0xB4 SKIPPED: insufficient data. Need %d bytes, have %d", bytes, payload_len - extra_offset - 1);
                        }
                    }
                    extra_fields[extra_fields_count].id = id;
                    extra_fields[extra_fields_count].value = value;
                    extra_fields[extra_fields_count].is_ascii = is_ascii;
                    if (is_ascii) {
                        strncpy(extra_fields[extra_fields_count].strval, strval, sizeof(extra_fields[extra_fields_count].strval)-1);
                    } else {
                        extra_fields[extra_fields_count].strval[0] = '\0';
                    }
                    extra_fields_count++;
                    ESP_LOGI(TAG, "PARSED: %s (0x%02X): %s%lu (offset %d -> %d)", 
                             bms_idcodes[i].name, id, is_ascii ? strval : "", 
                             is_ascii ? 0 : (unsigned long)value, extra_offset, extra_offset + 1 + bytes);
                    extra_offset += 1 + bytes;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                if (debug_logging) ESP_LOGW(TAG, "UNKNOWN field ID 0x%02X at offset %d, skipping", id, extra_offset);
                extra_offset++;
            }
        } else {
            // Skip this duplicate field
            if (debug_logging) ESP_LOGW(TAG, "SKIPPING duplicate field ID 0x%02X", id);
            int found = 0;
            for (size_t i = 0; i < BMS_IDCODES_COUNT; ++i) {
                if (bms_idcodes[i].id == id) {
                    if (debug_logging) ESP_LOGI(TAG, "Skipping %d bytes for duplicate 0x%02X", bms_idcodes[i].byte_len, id);
                    extra_offset += 1 + bms_idcodes[i].byte_len;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                ESP_LOGW(TAG, "Unknown duplicate field 0x%02X, advancing by 1 byte", id);
                extra_offset++;
            }
        }
    }
    
    ESP_LOGI(TAG, "=== EXTRA FIELDS PARSING COMPLETE ===");
    ESP_LOGI(TAG, "Parsed %d unique extra fields", extra_fields_count);

    if (debug_logging) printf("----------------------------------------\n"); // Separator for console output.

    // After parsing all data and populating current_bms_data
    // Call function to publish data via MQTT
    // This check ensures we only try to publish if we have some valid cell data
    if (debug_logging) printf("[DEBUG] Checking publish condition: num_cells=%d\n", current_bms_data.num_cells);
    if (current_bms_data.num_cells > 0) {
        if (debug_logging) printf("[DEBUG] Calling publish_bms_data_mqtt...\n");
        publish_bms_data_mqtt(&current_bms_data);
        if (debug_logging) printf("[DEBUG] publish_bms_data_mqtt returned\n");
    } else {
        if (debug_logging) printf("[DEBUG] Not publishing - no valid cell data\n");
    }
}


// Reads data from the BMS. This function handles potential UART echo and then parses the response.
static void read_bms_data() {
    uint8_t data_buf[UART_BUF_SIZE]; // Buffer to store raw data from UART.
    int length = 0; // Variable to store the length of data available in UART RX buffer.

    ESP_LOGI(TAG, "Attempting to read BMS data...");
    // Wait for a period to allow the BMS to respond.
    // JK BMS response time is typically <100ms for "Read All Data".
    // A 300ms delay provides a safety margin.
    vTaskDelay(pdMS_TO_TICKS(300)); 
    
    // Feed TWDT after the delay to prevent timeout during BMS communication
    esp_task_wdt_reset(); 

    // Get the number of bytes available in the UART RX buffer.
    ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_NUM, (size_t*)&length));
    
    if (length > 0) { // If data is available
        ESP_LOGI(TAG, "%d bytes available in UART buffer.", length);
        if (length > UART_BUF_SIZE) {
            ESP_LOGW(TAG, "Incoming data (%d bytes) larger than local buffer (%d bytes). Truncating.", length, UART_BUF_SIZE);
            length = UART_BUF_SIZE; // Limit read to buffer size to prevent overflow.
        }
        
        // Read the data from UART RX buffer into data_buf.
        // pdMS_TO_TICKS(200): Timeout for the read operation.
        int read_len = uart_read_bytes(UART_NUM, data_buf, length, pdMS_TO_TICKS(200)); 
        
        // Feed TWDT after UART read operation
        esp_task_wdt_reset(); 
        
        if (read_len > 0) {
            ESP_LOGI(TAG, "Successfully read %d bytes from UART:", read_len);
            ESP_LOG_BUFFER_HEX(TAG, data_buf, read_len); // Log the raw received data in hex.

            uint8_t* data_to_parse = data_buf; // Pointer to the start of data to be parsed.
            int len_to_parse = read_len;       // Length of data to be parsed.

            // Check if the received data starts with the command that was sent (UART echo).
            // This can happen if TX and RX lines are connected in a way that allows echo.
            if (len_to_parse >= sizeof(bms_read_all_cmd) &&
                memcmp(data_to_parse, bms_read_all_cmd, sizeof(bms_read_all_cmd)) == 0) {
                ESP_LOGI(TAG, "Command echo detected at the beginning of the read buffer. Skipping %d echo bytes.", (int)sizeof(bms_read_all_cmd));
                // Advance the pointer and reduce the length to skip the echo.
                data_to_parse += sizeof(bms_read_all_cmd);
                len_to_parse -= sizeof(bms_read_all_cmd);

                if (len_to_parse <= 0) {
                    ESP_LOGW(TAG, "Buffer contained only echo, no further data for BMS response.");
                    // uart_flush_input(UART_NUM); // Flush, though it will be flushed later anyway.
                    return; // No actual BMS response data left.
                }
                ESP_LOGI(TAG, "Processing remaining %d bytes for BMS response:", len_to_parse);
                ESP_LOG_BUFFER_HEX(TAG, data_to_parse, len_to_parse); // Log the data after skipping echo.
            }

            // If there's still data left after potential echo removal, parse it.
            if (len_to_parse > 0) { 
                parse_and_print_bms_data(data_to_parse, len_to_parse);
            } else {
                ESP_LOGW(TAG, "No data left to parse after potential echo removal.");
            }

        } else { // uart_read_bytes returned 0 or error.
            ESP_LOGW(TAG, "Failed to read data from UART buffer after length check (uart_read_bytes returned %d).", read_len);
        }
        // Flush the UART RX buffer to remove any remaining or old data.
        // This is important to prevent interference with the next read cycle.
        uart_flush_input(UART_NUM);
        ESP_LOGI(TAG, "UART RX buffer flushed.");
    } else { // No data available in UART buffer after the initial wait.
        ESP_LOGW(TAG, "No data available from BMS after %dms wait.", 300);
    }
}

// Place these functions before app_main so they are visible to it
void load_wifi_config_from_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        size_t ssid_len = sizeof(wifi_ssid);
        size_t pass_len = sizeof(wifi_pass);
        if (nvs_get_str(nvs_handle, WIFI_NVS_KEY_SSID, wifi_ssid, &ssid_len) != ESP_OK) {
            strncpy(wifi_ssid, DEFAULT_WIFI_SSID, sizeof(wifi_ssid)-1);
            nvs_set_str(nvs_handle, WIFI_NVS_KEY_SSID, wifi_ssid);
        }
        if (nvs_get_str(nvs_handle, WIFI_NVS_KEY_PASS, wifi_pass, &pass_len) != ESP_OK) {
            strncpy(wifi_pass, DEFAULT_WIFI_PASS, sizeof(wifi_pass)-1);
            nvs_set_str(nvs_handle, WIFI_NVS_KEY_PASS, wifi_pass);
        }
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    } else {
        strncpy(wifi_ssid, DEFAULT_WIFI_SSID, sizeof(wifi_ssid)-1);
        strncpy(wifi_pass, DEFAULT_WIFI_PASS, sizeof(wifi_pass)-1);
    }
}

void load_mqtt_config_from_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        size_t url_len = sizeof(mqtt_broker_url);
        if (nvs_get_str(nvs_handle, MQTT_NVS_KEY_URL, mqtt_broker_url, &url_len) != ESP_OK) {
            strncpy(mqtt_broker_url, DEFAULT_MQTT_BROKER_URL, sizeof(mqtt_broker_url)-1);
            nvs_set_str(nvs_handle, MQTT_NVS_KEY_URL, mqtt_broker_url);
        }
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    } else {
        strncpy(mqtt_broker_url, DEFAULT_MQTT_BROKER_URL, sizeof(mqtt_broker_url)-1);
    }
}

void load_sample_interval_from_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        int64_t val = 0;
        if (nvs_get_i64(nvs_handle, NVS_KEY_SAMPLE_INTERVAL, &val) == ESP_OK && val > 0) {
            sample_interval_ms = (long)val;
        } else {
            sample_interval_ms = DEFAULT_SAMPLE_INTERVAL;
            nvs_set_i64(nvs_handle, NVS_KEY_SAMPLE_INTERVAL, (int64_t)sample_interval_ms);
        }
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    } else {
        sample_interval_ms = DEFAULT_SAMPLE_INTERVAL;
    }
}

void load_bms_topic_from_nvs() {
    ESP_LOGI(TAG, "=== LOADING BMS TOPIC FROM NVS ===");
    ESP_LOGI(TAG, "Initial bms_topic value: '%s'", bms_topic);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_LOGI(TAG, "NVS open result: %s", esp_err_to_name(err));
    
    if (err == ESP_OK) {
        size_t topic_len = sizeof(bms_topic);
        ESP_LOGI(TAG, "Attempting to read NVS key '%s', buffer size: %d", NVS_KEY_BMS_TOPIC, topic_len);
        
        esp_err_t get_err = nvs_get_str(nvs_handle, NVS_KEY_BMS_TOPIC, bms_topic, &topic_len);
        ESP_LOGI(TAG, "NVS get_str result: %s", esp_err_to_name(get_err));
        
        if (get_err != ESP_OK) {
            ESP_LOGW(TAG, "BMS topic not found in NVS (error: %s), using default: BMS/JKBMS", esp_err_to_name(get_err));
            strncpy(bms_topic, "BMS/JKBMS", sizeof(bms_topic)-1);
            bms_topic[sizeof(bms_topic)-1] = '\0';
            esp_err_t set_err = nvs_set_str(nvs_handle, NVS_KEY_BMS_TOPIC, bms_topic);
            ESP_LOGI(TAG, "Default BMS topic save result: %s", esp_err_to_name(set_err));
            nvs_commit(nvs_handle);
        } else {
            ESP_LOGI(TAG, "Successfully loaded BMS topic from NVS: '%s' (length: %d)", bms_topic, topic_len);
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for BMS topic: %s", esp_err_to_name(err));
        strncpy(bms_topic, "BMS/JKBMS", sizeof(bms_topic)-1);
        bms_topic[sizeof(bms_topic)-1] = '\0';
    }
    ESP_LOGI(TAG, "=== FINAL BMS TOPIC: '%s' ===", bms_topic);
}

void save_bms_topic_to_nvs(const char *topic) {
    nvs_handle_t nvs_handle;
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_str(nvs_handle, NVS_KEY_BMS_TOPIC, topic);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
}

void load_watchdog_counter_from_nvs() {
    ESP_LOGI(TAG, "=== LOADING WATCHDOG COUNTER FROM NVS ===");
    ESP_LOGI(TAG, "Initial watchdog_reset_counter value: %lu", watchdog_reset_counter);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_LOGI(TAG, "NVS open result: %s", esp_err_to_name(err));
    
    if (err == ESP_OK) {
        uint32_t val = 0;
        esp_err_t get_err = nvs_get_u32(nvs_handle, NVS_KEY_WATCHDOG_COUNTER, &val);
        ESP_LOGI(TAG, "NVS get_u32 result: %s", esp_err_to_name(get_err));
        
        if (get_err == ESP_OK) {
            watchdog_reset_counter = val;
            ESP_LOGI(TAG, "Successfully loaded watchdog counter from NVS: %lu", watchdog_reset_counter);
        } else {
            ESP_LOGW(TAG, "Watchdog counter not found in NVS (error: %s), using default: 0", esp_err_to_name(get_err));
            watchdog_reset_counter = 0;
            esp_err_t set_err = nvs_set_u32(nvs_handle, NVS_KEY_WATCHDOG_COUNTER, watchdog_reset_counter);
            ESP_LOGI(TAG, "Default watchdog counter save result: %s", esp_err_to_name(set_err));
            nvs_commit(nvs_handle);
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for watchdog counter: %s", esp_err_to_name(err));
        watchdog_reset_counter = 0;
    }
    ESP_LOGI(TAG, "=== FINAL WATCHDOG COUNTER: %lu ===", watchdog_reset_counter);
}

void save_watchdog_counter_to_nvs() {
    ESP_LOGI(TAG, "=== SAVING WATCHDOG COUNTER TO NVS ===");
    ESP_LOGI(TAG, "Saving watchdog_reset_counter: %lu", watchdog_reset_counter);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        esp_err_t set_err = nvs_set_u32(nvs_handle, NVS_KEY_WATCHDOG_COUNTER, watchdog_reset_counter);
        ESP_LOGI(TAG, "NVS set_u32 result: %s", esp_err_to_name(set_err));
        if (set_err == ESP_OK) {
            nvs_commit(nvs_handle);
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for saving watchdog counter: %s", esp_err_to_name(err));
    }
}

void load_pack_name_from_nvs() {
    ESP_LOGI(TAG, "=== LOADING PACK NAME FROM NVS ===");
    ESP_LOGI(TAG, "Initial pack_name value: '%s'", pack_name);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    ESP_LOGI(TAG, "NVS open result: %s", esp_err_to_name(err));
    
    if (err == ESP_OK) {
        size_t name_len = sizeof(pack_name);
        ESP_LOGI(TAG, "Attempting to read NVS key '%s', buffer size: %d", NVS_KEY_PACK_NAME, name_len);
        
        esp_err_t get_err = nvs_get_str(nvs_handle, NVS_KEY_PACK_NAME, pack_name, &name_len);
        ESP_LOGI(TAG, "NVS get_str result: %s", esp_err_to_name(get_err));
        
        if (get_err != ESP_OK) {
            ESP_LOGW(TAG, "Pack name not found in NVS (error: %s), using default: %s", esp_err_to_name(get_err), DEFAULT_PACK_NAME);
            strncpy(pack_name, DEFAULT_PACK_NAME, sizeof(pack_name)-1);
            pack_name[sizeof(pack_name)-1] = '\0';
            esp_err_t set_err = nvs_set_str(nvs_handle, NVS_KEY_PACK_NAME, pack_name);
            ESP_LOGI(TAG, "Default pack name save result: %s", esp_err_to_name(set_err));
            nvs_commit(nvs_handle);
        } else {
            ESP_LOGI(TAG, "Successfully loaded pack name from NVS: '%s' (length: %d)", pack_name, name_len);
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for pack name: %s", esp_err_to_name(err));
        strncpy(pack_name, DEFAULT_PACK_NAME, sizeof(pack_name)-1);
        pack_name[sizeof(pack_name)-1] = '\0';
    }
    ESP_LOGI(TAG, "=== FINAL PACK NAME: '%s' ===", pack_name);
}

void save_pack_name_to_nvs(const char *name) {
    ESP_LOGI(TAG, "=== SAVING PACK NAME TO NVS ===");
    ESP_LOGI(TAG, "Saving pack_name: '%s'", name);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        esp_err_t set_err = nvs_set_str(nvs_handle, NVS_KEY_PACK_NAME, name);
        ESP_LOGI(TAG, "NVS set_str result: %s", esp_err_to_name(set_err));
        if (set_err == ESP_OK) {
            nvs_commit(nvs_handle);
        }
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for saving pack name: %s", esp_err_to_name(err));
    }
}

// SPIFFS init
void init_spiffs() {
    ESP_LOGI(TAG, "Initializing SPIFFS...");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS: %d kB total, %d kB used", total / 1024, used / 1024);
    }
}

// HTTP handler for /params.json
esp_err_t params_json_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "=== PARAMS.JSON REQUEST ===");
    
    // Debug: Show counter before reloading from NVS
    ESP_LOGI(TAG, "*** DEBUG: watchdog_reset_counter before reload: %lu ***", watchdog_reset_counter);
    
    // Reload watchdog counter from NVS to ensure latest value is displayed
    load_watchdog_counter_from_nvs();
    
    // Debug: Show counter after reloading from NVS
    ESP_LOGI(TAG, "*** DEBUG: watchdog_reset_counter after reload: %lu ***", watchdog_reset_counter);
    
    ESP_LOGI(TAG, "Current bms_topic variable: '%s'", bms_topic);
    ESP_LOGI(TAG, "Current wifi_ssid: '%s'", wifi_ssid);
    ESP_LOGI(TAG, "Current mqtt_broker_url: '%s'", mqtt_broker_url);
    ESP_LOGI(TAG, "Current sample_interval_ms: %ld", sample_interval_ms);
    ESP_LOGI(TAG, "Current watchdog_reset_counter: %lu", watchdog_reset_counter);
    ESP_LOGI(TAG, "Current pack_name: '%s'", pack_name);
    
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"password\":\"%s\",\"mqtt_url\":\"%s\",\"sample_interval\":%ld,\"bms_topic\":\"%s\",\"watchdog_reset_counter\":%lu,\"pack_name\":\"%s\"}", 
             wifi_ssid, wifi_pass, mqtt_broker_url, sample_interval_ms, bms_topic, watchdog_reset_counter, pack_name);
    ESP_LOGI(TAG, "Sending params.json response: %s", buf);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "=== PARAMS.JSON RESPONSE SENT ===");
    return ESP_OK;
}

// Global flag to signal shutdown
static bool system_shutting_down = false;

// Reboot task function
static void reboot_task(void *param) {
    ESP_LOGI(TAG, "Reboot task started, shutting down gracefully...");
    
    // Set shutdown flag to signal other tasks to stop
    system_shutting_down = true;
    
    // Give time for HTTP response to complete
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "Disabling task watchdog before restart...");
    esp_task_wdt_deinit();
    
    // Give a bit more time for tasks to see the shutdown flag
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    ESP_LOGI(TAG, "Performing system restart NOW...");
    fflush(stdout);  // Ensure log message is sent
    
    esp_restart();
    
    // This line should never be reached
    vTaskDelete(NULL);
}

// HTTP handler for /update (POST)
esp_err_t params_update_post_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "=== UPDATE REQUEST RECEIVED ===");
    
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to receive update data");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    ESP_LOGI(TAG, "Received update data: %s", buf);
    
    char ssid[33] = "", pass[65] = "", mqtt_url[128] = "";
    long interval = 5000;
    sscanf(strstr(buf, "ssid=")+5, "%32[^&]", ssid);
    sscanf(strstr(buf, "password=")+9, "%64[^&]", pass);
    sscanf(strstr(buf, "mqtt_url=")+9, "%127[^&]", mqtt_url);
    sscanf(strstr(buf, "sample_interval=")+16, "%ld", &interval);
    // Decode URL-encoded values
    char ssid_dec[33], pass_dec[65], url_dec[128];
    url_decode(ssid_dec, ssid, sizeof(ssid_dec));
    url_decode(pass_dec, pass, sizeof(pass_dec));
    url_decode(url_dec, mqtt_url, sizeof(url_dec));
    strncpy(wifi_ssid, ssid_dec, sizeof(wifi_ssid)-1);
    strncpy(wifi_pass, pass_dec, sizeof(wifi_pass)-1);
    strncpy(mqtt_broker_url, url_dec, sizeof(mqtt_broker_url)-1);
    sample_interval_ms = interval;
    // Save to NVS
    nvs_handle_t nvs_handle;
    nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    nvs_set_str(nvs_handle, WIFI_NVS_KEY_SSID, wifi_ssid);
    nvs_set_str(nvs_handle, WIFI_NVS_KEY_PASS, wifi_pass);
    nvs_set_str(nvs_handle, MQTT_NVS_KEY_URL, mqtt_broker_url);
    nvs_set_i64(nvs_handle, NVS_KEY_SAMPLE_INTERVAL, (int64_t)sample_interval_ms);
    // Handle BMS topic
    ESP_LOGI(TAG, "=== PROCESSING BMS TOPIC ===");
    ESP_LOGI(TAG, "Current bms_topic before update: '%s'", bms_topic);
    
    char bms_topic_in[41] = "";
    char bms_topic_dec[41];
    char *bms_topic_ptr = strstr(buf, "bms_topic=");
    if (bms_topic_ptr) {
        sscanf(bms_topic_ptr + 10, "%40[^&]", bms_topic_in);
        ESP_LOGI(TAG, "Raw BMS topic from form: '%s'", bms_topic_in);
        
        url_decode(bms_topic_dec, bms_topic_in, sizeof(bms_topic_dec));
        ESP_LOGI(TAG, "URL decoded BMS topic: '%s'", bms_topic_dec);
        
        strncpy(bms_topic, bms_topic_dec, sizeof(bms_topic)-1);
        bms_topic[sizeof(bms_topic)-1] = '\0';
        ESP_LOGI(TAG, "BMS topic updated to: '%s'", bms_topic);
    } else {
        ESP_LOGW(TAG, "No bms_topic found in request data: %s", buf);
    }
    
    esp_err_t nvs_err = nvs_set_str(nvs_handle, NVS_KEY_BMS_TOPIC, bms_topic);
    ESP_LOGI(TAG, "Saved BMS topic '%s' to NVS with key '%s': %s", bms_topic, NVS_KEY_BMS_TOPIC, esp_err_to_name(nvs_err));
    ESP_LOGI(TAG, "=== BMS TOPIC PROCESSING COMPLETE ===");
    
    // Handle Pack Name
    ESP_LOGI(TAG, "=== PROCESSING PACK NAME ===");
    ESP_LOGI(TAG, "Current pack_name before update: '%s'", pack_name);
    
    char pack_name_in[64] = "";
    char pack_name_dec[64];
    char *pack_name_ptr = strstr(buf, "pack_name=");
    if (pack_name_ptr) {
        sscanf(pack_name_ptr + 10, "%63[^&]", pack_name_in);
        ESP_LOGI(TAG, "Raw pack name from form: '%s'", pack_name_in);
        
        url_decode(pack_name_dec, pack_name_in, sizeof(pack_name_dec));
        ESP_LOGI(TAG, "URL decoded pack name: '%s'", pack_name_dec);
        
        strncpy(pack_name, pack_name_dec, sizeof(pack_name)-1);
        pack_name[sizeof(pack_name)-1] = '\0';
        ESP_LOGI(TAG, "Pack name updated to: '%s'", pack_name);
    } else {
        ESP_LOGW(TAG, "No pack_name found in request data: %s", buf);
    }
    
    esp_err_t pack_name_nvs_err = nvs_set_str(nvs_handle, NVS_KEY_PACK_NAME, pack_name);
    ESP_LOGI(TAG, "Saved pack name '%s' to NVS with key '%s': %s", pack_name, NVS_KEY_PACK_NAME, esp_err_to_name(pack_name_nvs_err));
    ESP_LOGI(TAG, "=== PACK NAME PROCESSING COMPLETE ===");
    
    // Handle Watchdog Reset Counter
    ESP_LOGI(TAG, "=== PROCESSING WATCHDOG RESET COUNTER ===");
    ESP_LOGI(TAG, "Current watchdog_reset_counter before update: %lu", watchdog_reset_counter);
    
    char *watchdog_counter_ptr = strstr(buf, "watchdog_reset_counter=");
    if (watchdog_counter_ptr) {
        uint32_t counter_value = 0;
        sscanf(watchdog_counter_ptr + 23, "%lu", &counter_value);
        ESP_LOGI(TAG, "Watchdog counter from form: %lu", counter_value);
        
        watchdog_reset_counter = counter_value;
        ESP_LOGI(TAG, "Watchdog reset counter updated to: %lu", watchdog_reset_counter);
    } else {
        ESP_LOGW(TAG, "No watchdog_reset_counter found in request data: %s", buf);
    }
    
    esp_err_t watchdog_nvs_err = nvs_set_u32(nvs_handle, NVS_KEY_WATCHDOG_COUNTER, watchdog_reset_counter);
    ESP_LOGI(TAG, "Saved watchdog counter %lu to NVS with key '%s': %s", watchdog_reset_counter, NVS_KEY_WATCHDOG_COUNTER, esp_err_to_name(watchdog_nvs_err));
    ESP_LOGI(TAG, "=== WATCHDOG RESET COUNTER PROCESSING COMPLETE ===");
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Configuration saved successfully. Sending response...");
    httpd_resp_sendstr(req, "Saved. Rebooting...");
    
    ESP_LOGI(TAG, "Response sent. Starting reboot sequence...");
    // Give the HTTP response time to be sent
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // Schedule reboot in a separate task to avoid blocking the HTTP response
    ESP_LOGI(TAG, "Creating reboot task...");
    BaseType_t task_created = xTaskCreate(reboot_task, "reboot_task", 2048, NULL, 1, NULL);
    if (task_created == pdPASS) {
        ESP_LOGI(TAG, "Reboot task created successfully");
    } else {
        ESP_LOGE(TAG, "Failed to create reboot task");
    }
    
    return ESP_OK;
}

// HTTP handler for /parameters.html
esp_err_t parameters_html_handler(httpd_req_t *req) {
    FILE *f = fopen("/spiffs/parameters.html", "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    char buf[512];
    httpd_resp_set_type(req, "text/html");
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, n);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Captive portal redirect handler
esp_err_t captive_redirect_handler(httpd_req_t *req) {
    ESP_LOGD(TAG, "Captive portal request: %s", req->uri);
    
    // If the request is already for /parameters.html, serve it directly
    if (strcmp(req->uri, "/parameters.html") == 0) {
        return parameters_html_handler(req);
    }
    
    // Check for other valid endpoints and serve them directly
    if (strcmp(req->uri, "/params.json") == 0) {
        return params_json_get_handler(req);
    }
    if (strcmp(req->uri, "/sysinfo.json") == 0) {
        return sysinfo_json_get_handler(req);
    }
    if (strcmp(req->uri, "/update") == 0) {
        return params_update_post_handler(req);
    }
    
    // For any other request, redirect to the configuration page
    ESP_LOGD(TAG, "Redirecting %s to /parameters.html", req->uri);
    
    const char* redirect_html = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<title>ESP32 Configuration</title>\n"
        "<meta http-equiv=\"refresh\" content=\"0; url=/parameters.html\">\n"
        "</head>\n"
        "<body>\n"
        "<p>Redirecting to configuration page...</p>\n"
        "<p>If you are not redirected automatically, <a href=\"/parameters.html\">click here</a>.</p>\n"
        "</body>\n"
        "</html>";
    
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_send(req, redirect_html, strlen(redirect_html));
    
    return ESP_OK;
}

// Handler for /sysinfo.json
static esp_err_t sysinfo_json_get_handler(httpd_req_t *req) {
    char resp[512];
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    // Use esp_app_get_description if available, else fallback
    #if ESP_IDF_VERSION_MAJOR >= 5
    const esp_app_desc_t *app_desc = esp_app_get_description();
    #else
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    #endif
    const char *idf_ver = esp_get_idf_version();
    
    // Read version from version.txt file in SPIFFS
    // Note: This file (data/version.txt) must be kept in sync with the master 
    // version file (version.txt in project root). The master file serves as 
    // the source of truth, while this file is deployed to the ESP32 for 
    // runtime display in the web interface.
    char version_str[16] = "1.3.0"; // Default fallback - update when master version changes
    FILE *version_file = fopen("/spiffs/version.txt", "r");
    if (version_file) {
        if (fgets(version_str, sizeof(version_str), version_file)) {
            // Remove trailing newline if present
            char *newline = strchr(version_str, '\n');
            if (newline) *newline = '\0';
            char *carriage_return = strchr(version_str, '\r');
            if (carriage_return) *carriage_return = '\0';
        }
        fclose(version_file);
    }
    
    esp_reset_reason_t reason = esp_reset_reason();
    const char *reason_str = "Unknown";
    switch (reason) {
        case ESP_RST_POWERON: reason_str = "Power-on"; break;
        case ESP_RST_EXT: reason_str = "External"; break;
        case ESP_RST_SW: reason_str = "Software"; break;
        case ESP_RST_PANIC: reason_str = "Panic"; break;
        case ESP_RST_INT_WDT: reason_str = "Interrupt WDT"; break;
        case ESP_RST_TASK_WDT: reason_str = "Task WDT"; break;
        case ESP_RST_WDT: reason_str = "Other WDT"; break;
        case ESP_RST_DEEPSLEEP: reason_str = "Deep Sleep"; break;
        case ESP_RST_BROWNOUT: reason_str = "Brownout"; break;
        case ESP_RST_SDIO: reason_str = "SDIO"; break;
        default: break;
    }
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char chip_id[18];
    snprintf(chip_id, sizeof(chip_id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // Get flash chip information
    uint32_t flash_id = 0x000000; // Placeholder
    uint32_t flash_size = 0;
    
    // Get actual flash size - ESP32 DevKit V1 typically has 4MB
    // Use the more reliable method of checking all partitions to estimate total flash
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    uint32_t max_offset = 0;
    while (it != NULL) {
        const esp_partition_t *partition = esp_partition_get(it);
        uint32_t partition_end = partition->address + partition->size;
        if (partition_end > max_offset) {
            max_offset = partition_end;
        }
        it = esp_partition_next(it);
    }
    if (it) esp_partition_iterator_release(it);
    
    // Round up to nearest MB and ensure it's at least 4MB for ESP32 DevKit V1
    if (max_offset > 0) {
        flash_size = ((max_offset + 1024*1024 - 1) / (1024*1024)) * 1024*1024; // Round up to nearest MB
        if (flash_size < 4*1024*1024) flash_size = 4*1024*1024; // Minimum 4MB for ESP32 DevKit V1
    } else {
        flash_size = 4 * 1024 * 1024; // Default 4MB for ESP32 DevKit V1
    }
    snprintf(resp, sizeof(resp),
        "{"
        "\"version\":\"%s\","  // Program Version
        "\"build\":\"%s %s\"," // Build Date & Time
        "\"sdk\":\"%s\","      // Core/SDK Version
        "\"restart_reason\":\"%s\"," // Restart Reason
        "\"chip_id\":\"%s\","  // ESP Chip Id
        "\"flash_id\":\"%06lX\"," // Flash Chip Id
        "\"flash_size\":\"%lu KB\""
        "}",
        version_str,  // Use version from version.txt file
        app_desc->date, app_desc->time,
        idf_ver,
        reason_str,
        chip_id,
        (unsigned long)(flash_id & 0xFFFFFF),
        (unsigned long)(flash_size / 1024)
    );
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// OTA Firmware Update Handler
static esp_err_t ota_firmware_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "OTA firmware handler called - Content-Length: %d", req->content_len);
    ESP_LOGI(TAG, "Request method: %d, URI: %s", req->method, req->uri);
    
    // Add CORS headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    
    // Handle preflight requests
    if (req->method == HTTP_OPTIONS) {
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    esp_ota_handle_t ota_handle = 0;
    const esp_partition_t *ota_partition = NULL;
    esp_err_t err = ESP_OK;
    char *buf = NULL;
    const size_t buffer_size = 2048;  // Reduced buffer size for better memory management
    int received = 0;
    int remaining = req->content_len;

    ESP_LOGI(TAG, "Starting OTA firmware update, size: %d bytes", remaining);
    
    // Allocate buffer on heap to avoid stack overflow
    buf = malloc(buffer_size);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for upload buffer", buffer_size);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    // Get next OTA partition
    ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx", 
             ota_partition->subtype, ota_partition->address);

    // Begin OTA
    err = esp_ota_begin(ota_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    // Receive and write firmware data
    int timeout_count = 0;
    int zero_recv_count = 0;
    int last_fw_progress = -1;  // Move static variable to local scope
    
    while (remaining > 0) {
        int chunk_size = (remaining < buffer_size) ? remaining : buffer_size;
        int recv_len = httpd_req_recv(req, buf, chunk_size);
        
        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                timeout_count++;
                ESP_LOGW(TAG, "Socket timeout #%d, continuing... (%d bytes remaining)", timeout_count, remaining);
                if (timeout_count > 15) {  // Increased tolerance for large files
                    ESP_LOGE(TAG, "Too many timeouts, aborting");
                    esp_ota_abort(ota_handle);
                    free(buf);
                    httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Upload timeout - too many socket timeouts");
                    return ESP_FAIL;
                }
                vTaskDelay(pdMS_TO_TICKS(200));  // Longer delay for large file processing
                continue;
            }
            ESP_LOGE(TAG, "File reception failed with error: %d", recv_len);
            esp_ota_abort(ota_handle);
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File reception failed - connection error");
            return ESP_FAIL;
        }
        
        if (recv_len == 0) {
            zero_recv_count++;
            ESP_LOGW(TAG, "Received 0 bytes #%d, continuing... (%d bytes remaining)", zero_recv_count, remaining);
            if (zero_recv_count > 8) {  // Increased tolerance for large files
                ESP_LOGE(TAG, "Too many zero-byte receives, connection likely closed");
                esp_ota_abort(ota_handle);
                free(buf);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Connection closed unexpectedly");
                return ESP_FAIL;
            }
            vTaskDelay(pdMS_TO_TICKS(100));  // Longer delay before retry
            continue;
        }
        
        // Reset counters on successful receive
        timeout_count = 0;
        zero_recv_count = 0;

        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed - firmware corrupted?");
            return ESP_FAIL;
        }

        received += recv_len;
        remaining -= recv_len;
        
        // Log progress every 5% for large files
        int progress = (received * 100) / req->content_len;
        if (progress != last_fw_progress && progress % 5 == 0) {
            ESP_LOGI(TAG, "Firmware Progress: %d%% (%d/%d bytes, free heap: %lu)", 
                     progress, received, req->content_len, (unsigned long)esp_get_free_heap_size());
            last_fw_progress = progress;
        }
        
        // Yield to other tasks periodically for large uploads
        if ((received % (buffer_size * 4)) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));  // Brief yield every ~8KB
        }
    }

    free(buf);  // Clean up allocated buffer

    // End OTA
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware validation failed - invalid binary");
        } else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        }
        return ESP_FAIL;
    }

    // Set boot partition
    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA firmware update successful, rebooting...");
    httpd_resp_sendstr(req, "Firmware Update Successful! Rebooting...");
    
    // Reboot after a short delay
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

// OTA Filesystem Update Handler
static esp_err_t ota_filesystem_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "OTA filesystem handler called - Content-Length: %d", req->content_len);
    ESP_LOGI(TAG, "Request method: %d, URI: %s", req->method, req->uri);
    
    // Add CORS headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    
    // Handle preflight requests
    if (req->method == HTTP_OPTIONS) {
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    const esp_partition_t *spiffs_partition = NULL;
    esp_err_t err = ESP_OK;
    char *buf = NULL;
    const size_t buffer_size = 2048;  // Reduced buffer size for better memory management
    int received = 0;
    int remaining = req->content_len;

    ESP_LOGI(TAG, "Starting OTA filesystem update, size: %d bytes", remaining);
    
    // Allocate buffer on heap to avoid stack overflow
    buf = malloc(buffer_size);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for upload buffer", buffer_size);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    // Find SPIFFS partition
    spiffs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if (!spiffs_partition) {
        ESP_LOGE(TAG, "No SPIFFS partition found");
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No SPIFFS partition found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Erasing SPIFFS partition at offset 0x%lx, size: %lu bytes", 
             spiffs_partition->address, spiffs_partition->size);

    // Erase the partition first
    err = esp_partition_erase_range(spiffs_partition, 0, spiffs_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase SPIFFS partition: %s", esp_err_to_name(err));
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to erase SPIFFS partition");
        return ESP_FAIL;
    }

    // Write filesystem data
    size_t offset = 0;
    int timeout_count = 0;
    int zero_recv_count = 0;
    int last_fs_progress = -1;  // Move static variable to local scope
    
    while (remaining > 0) {
        int chunk_size = (remaining < buffer_size) ? remaining : buffer_size;
        int recv_len = httpd_req_recv(req, buf, chunk_size);
        
        if (recv_len < 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                timeout_count++;
                ESP_LOGW(TAG, "Socket timeout #%d, continuing... (%d bytes remaining)", timeout_count, remaining);
                if (timeout_count > 15) {  // Increased tolerance for large files
                    ESP_LOGE(TAG, "Too many timeouts, aborting");
                    free(buf);
                    httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Upload timeout - too many socket timeouts");
                    return ESP_FAIL;
                }
                vTaskDelay(pdMS_TO_TICKS(200));  // Longer delay for large file processing
                continue;
            }
            ESP_LOGE(TAG, "File reception failed with error: %d", recv_len);
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File reception failed - connection error");
            return ESP_FAIL;
        }
        
        if (recv_len == 0) {
            zero_recv_count++;
            ESP_LOGW(TAG, "Received 0 bytes #%d, continuing... (%d bytes remaining)", zero_recv_count, remaining);
            if (zero_recv_count > 8) {  // Increased tolerance for large files
                ESP_LOGE(TAG, "Too many zero-byte receives, connection likely closed");
                free(buf);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Connection closed unexpectedly");
                return ESP_FAIL;
            }
            vTaskDelay(pdMS_TO_TICKS(100));  // Longer delay before retry
            continue;
        }
        
        // Reset counters on successful receive
        timeout_count = 0;
        zero_recv_count = 0;

        err = esp_partition_write(spiffs_partition, offset, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_partition_write failed: %s", esp_err_to_name(err));
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Partition write failed - flash error");
            return ESP_FAIL;
        }

        received += recv_len;
        remaining -= recv_len;
        offset += recv_len;
        
        // Log progress every 5% for large files
        int progress = (received * 100) / req->content_len;
        if (progress != last_fs_progress && progress % 5 == 0) {
            ESP_LOGI(TAG, "SPIFFS Progress: %d%% (%d/%d bytes, free heap: %lu)", 
                     progress, received, req->content_len, (unsigned long)esp_get_free_heap_size());
            last_fs_progress = progress;
        }
        
        // Yield to other tasks periodically for large uploads
        if ((received % (buffer_size * 4)) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));  // Brief yield every ~8KB
        }
    }

    free(buf);  // Clean up allocated buffer
    ESP_LOGI(TAG, "OTA filesystem update successful, rebooting...");
    httpd_resp_sendstr(req, "Filesystem Update Successful! Rebooting...");
    
    // Reboot after a short delay
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}



static void start_ap_and_captive_portal() {
    ESP_LOGI(TAG, "Starting AP and captive portal...");
    
    // Start AP mode only for configuration (no STA mode to avoid connection attempts)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Set to AP mode only - do not enable STA to avoid connection attempts
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_LOGI(TAG, "WiFi set to AP mode only for configuration");
    
    // Configure AP with a unique name based on MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "ESP32-CONFIG-%02X%02X", mac[4], mac[5]);
    
    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = 0,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .beacon_interval = 100
        }
    };
    strncpy((char*)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // Set AP IP and DHCP
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    
    // Stop DHCP server before configuring IP
    esp_netif_dhcps_stop(ap_netif);
    
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton("192.168.4.1");
    ip_info.netmask.addr = esp_ip4addr_aton("255.255.255.0");
    ip_info.gw.addr = esp_ip4addr_aton("192.168.4.1");
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    
    // Configure DHCP server options
    esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &ip_info.ip, sizeof(ip_info.ip));
    
    esp_netif_dhcps_start(ap_netif);

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi AP started: %s (IP: 192.168.4.1)", ap_ssid);
    
    // Wait a moment for WiFi to stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Start SPIFFS
    ESP_LOGI(TAG, "About to call init_spiffs()...");
    init_spiffs();
    ESP_LOGI(TAG, "init_spiffs() completed successfully");
    
    // Start HTTP server
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;  // Increase limit
    config.stack_size = 8192;      // Increase stack size
    
    // Configure for large file uploads (OTA)
    config.recv_wait_timeout = 120;     // 2 minutes timeout for receiving data
    config.send_wait_timeout = 120;     // 2 minutes timeout for sending data
    config.lru_purge_enable = true;     // Enable LRU purging of connections
    config.max_open_sockets = 4;        // Limit concurrent connections
    config.backlog_conn = 2;            // Reduce backlog
    
    ESP_LOGI(TAG, "HTTP server config: recv_timeout=%d, send_timeout=%d, max_sockets=%d", 
             config.recv_wait_timeout, config.send_wait_timeout, config.max_open_sockets);
    
    esp_err_t server_err = httpd_start(&server, &config);
    if (server_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(server_err));
        return;
    }
    
    ESP_LOGI(TAG, "HTTP server started successfully");
    
    // Register handlers in order of specificity (most specific first)
    httpd_uri_t params_json = { .uri = "/params.json", .method = HTTP_GET, .handler = params_json_get_handler, .user_ctx = NULL };
    httpd_uri_t params_update = { .uri = "/update", .method = HTTP_POST, .handler = params_update_post_handler, .user_ctx = NULL };
    httpd_uri_t parameters_html = { .uri = "/parameters.html", .method = HTTP_GET, .handler = parameters_html_handler, .user_ctx = NULL };
    httpd_uri_t sysinfo_json = { .uri = "/sysinfo.json", .method = HTTP_GET, .handler = sysinfo_json_get_handler, .user_ctx = NULL };
    httpd_uri_t ota_firmware = { .uri = "/ota/firmware", .method = HTTP_POST, .handler = ota_firmware_handler, .user_ctx = NULL };
    httpd_uri_t ota_filesystem = { .uri = "/ota/filesystem", .method = HTTP_POST, .handler = ota_filesystem_handler, .user_ctx = NULL };
    httpd_uri_t ota_firmware_options = { .uri = "/ota/firmware", .method = HTTP_OPTIONS, .handler = ota_firmware_handler, .user_ctx = NULL };
    httpd_uri_t ota_filesystem_options = { .uri = "/ota/filesystem", .method = HTTP_OPTIONS, .handler = ota_filesystem_handler, .user_ctx = NULL };
    
    // Register specific handlers first
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &params_json));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &params_update));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &parameters_html));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &sysinfo_json));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ota_firmware));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ota_filesystem));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ota_firmware_options));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ota_filesystem_options));
    
    // Register wildcard handler last to catch all other requests
    httpd_uri_t captive = { .uri = "/*", .method = HTTP_GET, .handler = captive_redirect_handler, .user_ctx = NULL };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &captive));
    
    ESP_LOGI(TAG, "All HTTP handlers registered successfully");
    ESP_LOGI(TAG, "Captive portal ready - connect to '%s' and visit any website", ap_ssid);
}

void ap_config_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting AP configuration mode...");
    
    // Debug: Show watchdog counter before loading from NVS
    ESP_LOGI(TAG, "*** DEBUG: watchdog_reset_counter before NVS load (AP mode): %lu ***", watchdog_reset_counter);
    
    // Load NVS values before starting AP and HTTP server
    load_wifi_config_from_nvs();
    load_mqtt_config_from_nvs();
    load_sample_interval_from_nvs();
    load_bms_topic_from_nvs();
    load_watchdog_counter_from_nvs();
    load_pack_name_from_nvs();
    
    // Debug: Show watchdog counter after loading from NVS
    ESP_LOGI(TAG, "*** DEBUG: watchdog_reset_counter after NVS load (AP mode): %lu ***", watchdog_reset_counter);
    
    ESP_LOGI(TAG, "=== CONFIG LOADED FROM NVS (AP MODE) ===");
    ESP_LOGI(TAG, "WiFi SSID: %s", wifi_ssid);
    ESP_LOGI(TAG, "WiFi PASS: %s", wifi_pass);
    ESP_LOGI(TAG, "MQTT Broker URL: %s", mqtt_broker_url);
    ESP_LOGI(TAG, "BMS Topic: %s", bms_topic);
    ESP_LOGI(TAG, "Sample interval (ms): %ld", sample_interval_ms);
    ESP_LOGI(TAG, "Watchdog reset counter: %lu", watchdog_reset_counter);
    ESP_LOGI(TAG, "Pack name: '%s'", pack_name);
    ESP_LOGI(TAG, "=== STARTING AP CONFIG MODE ===");
    
    // Note: esp_netif_init() and esp_event_loop_create_default() 
    // are already called in main app initialization
    
    // Start the captive portal
    start_ap_and_captive_portal();
    
    // Wait a moment for the HTTP server to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Start DNS hijack server for captive portal
    TaskHandle_t dns_task_handle = NULL;
    BaseType_t dns_task_result = xTaskCreate(dns_hijack_task, "dns_hijack_task", 4096, NULL, 4, &dns_task_handle);
    if (dns_task_result == pdPASS) {
        ESP_LOGI(TAG, "DNS hijack task started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to create DNS hijack task");
    }
    
    ESP_LOGI(TAG, "AP configuration mode fully initialized");
    ESP_LOGI(TAG, "Connect to ESP32-CONFIG-XXXX network and visit any website to configure");
    
    vTaskDelete(NULL);
}

// Blink task for onboard LED
#define ONBOARD_LED_GPIO GPIO_NUM_2

// Blink the onboard LED for a short time (heartbeat)
void blink_heartbeat() {
    gpio_set_direction(ONBOARD_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(ONBOARD_LED_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(ONBOARD_LED_GPIO, 0);
}

// Main application entry point.
void app_main(void) {
    // ============================================================
    // REDUCE LOGGING OUTPUT - Uncomment one of these lines:
    // ============================================================
    esp_log_level_set("*", ESP_LOG_WARN);   // Show only warnings and errors
    // esp_log_level_set("*", ESP_LOG_INFO);   // Show info, warnings and errors (for debugging)
    // esp_log_level_set("*", ESP_LOG_ERROR);  // Show only errors
    // esp_log_level_set("*", ESP_LOG_NONE);   // Turn off all logging
    
    // Initialize NVS first for all code paths
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Debug: Show initial watchdog counter value at app start
    ESP_LOGI(TAG, "*** DEBUG: Initial watchdog_reset_counter at app start: %lu ***", watchdog_reset_counter);

    // Configure Task Watchdog Timer
    ESP_LOGI(TAG, "Configuring Task Watchdog Timer...");
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 30000, // 30 second timeout
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // Monitor all cores
        .trigger_panic = true // Trigger panic on timeout
    };
    esp_err_t twdt_err = esp_task_wdt_init(&twdt_config);
    ESP_LOGI(TAG, "esp_task_wdt_init() returned: %d", twdt_err);
    // No esp_task_wdt_get_timeout() in ESP-IDF; cannot log actual timeout at runtime
    if (twdt_err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "Task Watchdog Timer already initialized");
    } else {
        ESP_ERROR_CHECK(twdt_err);
        ESP_LOGI(TAG, "Task Watchdog Timer initialized successfully");
    }
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); // Add current task to watchdog
    ESP_LOGI(TAG, "Task Watchdog Timer configured successfully");

    // Initialize network stack early for both AP and STA modes
    ESP_LOGI(TAG, "Initializing network stack...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "Network stack initialized successfully");

    ESP_LOGI(TAG, "Waiting 10 seconds for boot button press...");
    gpio_set_direction(BOOT_BTN_GPIO, GPIO_MODE_INPUT);
    bool boot_btn_pressed = false;
    for (int i = 0; i < 1000; ++i) { // 10 seconds, check every 10ms
        if (gpio_get_level(BOOT_BTN_GPIO) == 0) {
            boot_btn_pressed = true;
            ESP_LOGI(TAG, "Boot button press detected during wait!");
            break;
        }
        // Feed watchdog every 100 iterations (1 second)
        if (i % 100 == 0) {
            esp_task_wdt_reset();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "Boot button wait finished, continuing startup...");
    if (boot_btn_pressed) {
        ESP_LOGI(TAG, "Entering AP config mode due to boot button press.");
        xTaskCreate(ap_config_task, "ap_config_task", 8192, NULL, 5, NULL);
        while (1) {
            esp_task_wdt_reset(); // Feed watchdog in config mode
            vTaskDelay(1000/portTICK_PERIOD_MS); // Block forever
        }
    }

    // Initialize UART communication.
    init_uart();

    // Debug: Show watchdog counter before loading from NVS
    ESP_LOGI(TAG, "*** DEBUG: watchdog_reset_counter before NVS load (normal mode): %lu ***", watchdog_reset_counter);

    load_wifi_config_from_nvs();
    load_mqtt_config_from_nvs();
    load_sample_interval_from_nvs();
    load_bms_topic_from_nvs();
    load_watchdog_counter_from_nvs();
    load_pack_name_from_nvs();
    
    // Debug: Show watchdog counter after loading from NVS
    ESP_LOGI(TAG, "*** DEBUG: watchdog_reset_counter after NVS load (normal mode): %lu ***", watchdog_reset_counter);
    
    ESP_LOGI(TAG, "=== CONFIG LOADED FROM NVS ===");
    ESP_LOGI(TAG, "WiFi SSID: %s", wifi_ssid);
    ESP_LOGI(TAG, "WiFi PASS: %s", wifi_pass);
    ESP_LOGI(TAG, "MQTT Broker URL: %s", mqtt_broker_url);
    ESP_LOGI(TAG, "BMS Topic: %s", bms_topic);
    ESP_LOGI(TAG, "Sample interval (ms): %ld", sample_interval_ms);
    ESP_LOGI(TAG, "Watchdog reset counter: %lu", watchdog_reset_counter);
    ESP_LOGI(TAG, "Pack name: %s", pack_name);
    
    // Initialize SPIFFS for normal operation mode
    ESP_LOGI(TAG, "About to call init_spiffs() in normal mode...");
    init_spiffs();
    ESP_LOGI(TAG, "init_spiffs() completed successfully in normal mode");
    
    ESP_LOGI(TAG, "=== STARTING WIFI INIT ===");
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta(); // Initialize Wi-Fi
    
    // Initialize software watchdog
    // Watchdog timeout is 10× sample interval (in ms), inactive if timeout ≤10,000 ms
    watchdog_timeout_ms = sample_interval_ms * 10;
    if (watchdog_timeout_ms > 10000) {
        watchdog_enabled = true;
        last_successful_publish = xTaskGetTickCount();
        ESP_LOGI(TAG, "Software watchdog enabled with timeout: %lu ms (10× sample interval)", watchdog_timeout_ms);
    } else {
        watchdog_enabled = false;
        ESP_LOGI(TAG, "Software watchdog inactive (timeout ≤10,000 ms, sample interval: %lu ms)", sample_interval_ms);
    }

    // Buffer to attempt to read and discard UART echo. Size of the command sent.
    uint8_t echo_buf[sizeof(bms_read_all_cmd)]; 

    // Main loop to periodically send command and read response from BMS.
    while (1) {
        ESP_LOGD(TAG, "[MAIN LOOP] Start iteration");
        esp_task_wdt_reset();
        
        // Check software watchdog timeout
        ESP_LOGD(TAG, "[MAIN LOOP] Check software watchdog");
        if (watchdog_enabled) {
            TickType_t current_time = xTaskGetTickCount();
            TickType_t elapsed_ms = pdTICKS_TO_MS(current_time - last_successful_publish);
            if (elapsed_ms >= watchdog_timeout_ms) {
                ESP_LOGE(TAG, "Software watchdog timeout! No successful MQTT publish in %lu ms (timeout: %lu ms)", elapsed_ms, watchdog_timeout_ms);
                
                // Debug: Show counter before increment
                ESP_LOGE(TAG, "*** DEBUG: watchdog_reset_counter BEFORE increment: %lu ***", watchdog_reset_counter);
                
                // Increment watchdog reset counter and save to NVS
                watchdog_reset_counter++;
                ESP_LOGE(TAG, "Incrementing watchdog reset counter to: %lu", watchdog_reset_counter);
                
                // Debug: Show counter after increment
                ESP_LOGE(TAG, "*** DEBUG: watchdog_reset_counter AFTER increment: %lu ***", watchdog_reset_counter);
                
                save_watchdog_counter_to_nvs();
                
                // Debug: Verify save by reloading from NVS
                uint32_t verify_counter = 0;
                nvs_handle_t verify_handle;
                esp_err_t verify_err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &verify_handle);
                if (verify_err == ESP_OK) {
                    esp_err_t get_err = nvs_get_u32(verify_handle, NVS_KEY_WATCHDOG_COUNTER, &verify_counter);
                    ESP_LOGE(TAG, "*** DEBUG: Verification read from NVS result: %s, value: %lu ***", 
                             esp_err_to_name(get_err), verify_counter);
                    nvs_close(verify_handle);
                } else {
                    ESP_LOGE(TAG, "*** DEBUG: Failed to open NVS for verification: %s ***", esp_err_to_name(verify_err));
                }
                
                ESP_LOGE(TAG, "Forcing system restart...");
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_restart();
            }
        }
        ESP_LOGD(TAG, "[MAIN LOOP] After software watchdog check");

        mqtt_publish_success = false;
        ESP_LOGD(TAG, "[MAIN LOOP] After reset mqtt_publish_success");

        if (debug_logging) {
            ESP_LOGI(TAG, "Sending '%s' command to BMS...", "Read All Data");
        }
        send_bms_command(bms_read_all_cmd, sizeof(bms_read_all_cmd));
        ESP_LOGD(TAG, "[MAIN LOOP] After send_bms_command");

        int echo_read_len = uart_read_bytes(UART_NUM, echo_buf, sizeof(bms_read_all_cmd), pdMS_TO_TICKS(20));
        ESP_LOGD(TAG, "[MAIN LOOP] After uart_read_bytes for echo");

        if (echo_read_len == sizeof(bms_read_all_cmd)) {
            if (debug_logging) {
                ESP_LOGI(TAG, "Successfully read and discarded %d echo bytes.", echo_read_len);
            }
        } else if (echo_read_len > 0) {
            if (debug_logging) {
                ESP_LOGW(TAG, "Partially read %d echo bytes (expected %d). The rest might be in the main read or BMS response is very fast.", echo_read_len, (int)sizeof(bms_read_all_cmd));
            }
        } else {
            if (debug_logging) {
                ESP_LOGI(TAG, "No echo read or timeout/error during dedicated echo read attempt (read %d bytes). Main read will handle if echo is present.", echo_read_len);
            }
        }
        ESP_LOGD(TAG, "[MAIN LOOP] After echo read handling");

        read_bms_data();
        ESP_LOGD(TAG, "[MAIN LOOP] After read_bms_data");

        if (debug_logging) {
            ESP_LOGI(TAG, "Waiting %ld ms before next BMS read cycle...", sample_interval_ms);
        }
        if (sample_interval_ms > 5000) {
            uint32_t remaining_ms = sample_interval_ms;
            while (remaining_ms > 0) {
                uint32_t chunk_ms = (remaining_ms > 5000) ? 5000 : remaining_ms;
                vTaskDelay(pdMS_TO_TICKS(chunk_ms));
                esp_task_wdt_reset();
                remaining_ms -= chunk_ms;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(sample_interval_ms));
        }
        ESP_LOGD(TAG, "[MAIN LOOP] End of iteration, feeding TWDT");
        esp_task_wdt_reset();
    }
}

// Wi-Fi Event Handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    printf("[WIFI] Event handler called: base=%s, event_id=%ld\n", event_base, event_id);
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        printf("[WIFI] WIFI_EVENT_STA_START - calling esp_wifi_connect()\n");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("[WIFI] WIFI_EVENT_STA_DISCONNECTED - retry_num=%d\n", wifi_retry_num);
        if (wifi_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            wifi_retry_num++;
            printf("[WIFI] retry to connect to the AP\n");
        } else {
            printf("[WIFI] connect to the AP fail\n");
            printf("[WIFI] WiFi SSID used: %s\n", wifi_ssid);
            printf("[WIFI] WiFi Password used: %s\n", wifi_pass);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        printf("[WIFI] IP_EVENT_STA_GOT_IP received!\n");
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        printf("[WIFI] got ip:" IPSTR "\n", IP2STR(&event->ip_info.ip));
        printf("Obtained IP address: " IPSTR "\n", IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        // Start MQTT client once IP is obtained
        printf("[WIFI] About to start MQTT client...\n");
        mqtt_app_start();
        printf("[WIFI] MQTT client start function completed\n");
    } else {
        printf("[WIFI] Unhandled event: base=%s, event_id=%ld\n", event_base, event_id);
    }
    printf("[WIFI] Event handler completed\n");
}

// Wi-Fi Initialization
void wifi_init_sta(void)
{
    // Note: esp_netif_init() and esp_event_loop_create_default() 
    // are already called in main app initialization
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Or other appropriate authmode
        },
    };
    strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid)-1);
    strncpy((char*)wifi_config.sta.password, wifi_pass, sizeof(wifi_config.sta.password)-1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

// MQTT Event Handler
static void mqtt_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%ld", event_base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    printf("[MQTT] Event received: %ld\n", event_id);
    // esp_mqtt_client_handle_t client = event->client; // Not used in this basic handler
    // int msg_id; // Not used here
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        printf("[MQTT] Connected to broker successfully!\n");
        // Example: Subscribe to a topic (optional)
        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        printf("[MQTT] Disconnected from broker\n");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        printf("[MQTT] Message published successfully!\n");
        
        // Flash the LED to indicate successful MQTT publish (heartbeat)
        blink_heartbeat();
        
        // Reset the software watchdog timer (if enabled)
        if (watchdog_enabled) {
            last_successful_publish = xTaskGetTickCount(); // Update last publish time
        }
        break;
    case MQTT_EVENT_DATA: // Incoming data event (if subscribed)
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        printf("[MQTT] Connection error occurred!\n");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            printf("[MQTT] TCP Transport error - check network connectivity\n");
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            printf("[MQTT] Broker refused connection\n");
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            printf("[MQTT] Unknown error type: 0x%x\n", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

// MQTT Application Start
static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_broker_url,
    };
    // The event_handle field was removed from esp_mqtt_client_config_t in ESP-IDF v5.0.
    // Events are now registered globally for the client instance.
    // For older IDF: .event_handle = mqtt_event_handler,

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "MQTT client started.");
}

// Helper functions for processor metrics
static int32_t get_wifi_rssi_percent(void) {
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        int32_t rssi_dbm = ap_info.rssi;
        // Convert RSSI from dBm to percentage
        // Typical range: -100 dBm (0%) to -50 dBm (100%)
        int32_t rssi_percent;
        if (rssi_dbm >= -50) {
            rssi_percent = 100;
        } else if (rssi_dbm <= -100) {
            rssi_percent = 0;
        } else {
            // Linear mapping: -100 dBm = 0%, -50 dBm = 100%
            rssi_percent = 2 * (rssi_dbm + 100);
        }
        return rssi_percent;
    }
    return 0; // Error value - 0% signal
}

static char* get_ip_address(void) {
    static char ip_str[16];
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        esp_err_t ret = esp_netif_get_ip_info(netif, &ip_info);
        if (ret == ESP_OK) {
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            return ip_str;
        }
    }
    return "0.0.0.0";
}

static float get_cpu_temperature(void) {
    // ESP32 temperature reading is complex and requires additional configuration
    // For now, return a placeholder that could be enhanced later
    // Alternative: Use ADC reading from internal temperature sensor or external sensor
    return 25.0 + (esp_random() % 20); // Simulated temperature 25-45°C for demonstration
}

static char* get_software_version(void) {
    static char version_str[16] = "0.0.0"; // Default fallback - indicates version file not read
    static bool version_loaded = false;
    
    if (!version_loaded) {
        ESP_LOGI(TAG, "Attempting to read version from /spiffs/version.txt");
        FILE *version_file = fopen("/spiffs/version.txt", "r");
        if (version_file) {
            ESP_LOGI(TAG, "Version file opened successfully");
            if (fgets(version_str, sizeof(version_str), version_file)) {
                ESP_LOGI(TAG, "Read version string: '%s'", version_str);
                // Remove trailing newline if present
                char *newline = strchr(version_str, '\n');
                if (newline) *newline = '\0';
                char *carriage_return = strchr(version_str, '\r');
                if (carriage_return) *carriage_return = '\0';
                ESP_LOGI(TAG, "Cleaned version string: '%s'", version_str);
            } else {
                ESP_LOGE(TAG, "Failed to read from version file");
            }
            fclose(version_file);
        } else {
            ESP_LOGE(TAG, "Failed to open /spiffs/version.txt - file may not exist or SPIFFS not mounted");
        }
        version_loaded = true;
    }
    return version_str;
}

// Placeholder for publishing BMS data
// This function will be expanded to create and send the two JSON messages
void publish_bms_data_mqtt(const bms_data_t *bms_data_ptr) {
    if (debug_logging) printf("[DEBUG] publish_bms_data_mqtt() called with num_cells=%d\n", bms_data_ptr->num_cells);
    
    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized!");
        if (debug_logging) printf("[DEBUG] MQTT client not initialized - returning\n");
        return;
    }

    ESP_LOGI(TAG, "Preparing to publish BMS data via MQTT...");

    // --- Create JSON with temperature sensors added ---
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create cJSON root object.");
        return;
    }

    // Add processor metrics data first
    cJSON *processor_root = cJSON_CreateObject();
    if (processor_root) {
        // WiFi RSSI as percentage
        int32_t wifi_rssi_percent = get_wifi_rssi_percent();
        cJSON_AddNumberToObject(processor_root, "WiFiRSSI", wifi_rssi_percent);
        
        // IP Address
        char *ip_address = get_ip_address();
        cJSON_AddStringToObject(processor_root, "IPAddress", ip_address);
        
        // CPU Temperature
        float cpu_temp = get_cpu_temperature();
        cJSON_AddNumberToObject(processor_root, "CPUTemperature", cpu_temp);
        
        // Software Version
        char *software_version = get_software_version();
        cJSON_AddStringToObject(processor_root, "SoftwareVersion", software_version);
        
        // Watchdog Restart Count
        cJSON_AddNumberToObject(processor_root, "WDTRestartCount", watchdog_reset_counter);
        
        cJSON_AddItemToObject(root, "processor", processor_root);
    }

    // Add pack data with temperature sensors and enhanced system info
    cJSON *pack_root = cJSON_CreateObject();
    if (pack_root) {
        // Add pack name first in the pack object
        cJSON_AddStringToObject(pack_root, "packName", pack_name);
        
        // Core pack data
        cJSON_AddNumberToObject(pack_root, "packV", bms_data_ptr->pack_voltage);
        cJSON_AddNumberToObject(pack_root, "packA", bms_data_ptr->pack_current);
        cJSON_AddNumberToObject(pack_root, "packNumberOfCells", bms_data_ptr->num_cells);
        cJSON_AddNumberToObject(pack_root, "packSOC", bms_data_ptr->soc_percent);
        
        // Add cell voltage statistics
        cJSON_AddNumberToObject(pack_root, "packMinCellV", bms_data_ptr->min_cell_voltage);
        cJSON_AddNumberToObject(pack_root, "packMaxCellV", bms_data_ptr->max_cell_voltage);
        cJSON_AddNumberToObject(pack_root, "packCellVDelta", bms_data_ptr->cell_voltage_delta);
        
        // Add temperature sensors
        cJSON *temp_sensors = cJSON_CreateObject();
        if (temp_sensors) {
            cJSON_AddNumberToObject(temp_sensors, "NTC0", bms_data_ptr->mosfet_temp);
            cJSON_AddNumberToObject(temp_sensors, "NTC1", bms_data_ptr->probe1_temp);
            cJSON_AddNumberToObject(temp_sensors, "NTC2", bms_data_ptr->probe2_temp);
            cJSON_AddItemToObject(pack_root, "tempSensorValues", temp_sensors);
        }
        
        // Add system status information
        cJSON *system_status = cJSON_CreateObject();
        if (system_status) {
            // Look for MOSFET status fields
            for (int i = 0; i < extra_fields_count; ++i) {
                if (extra_fields[i].id == 0xAB) { // Charging MOS switch
                    cJSON_AddNumberToObject(system_status, "chargeMosfetStatus", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xAC) { // Discharge MOS switch
                    cJSON_AddNumberToObject(system_status, "dischargeMosfetStatus", extra_fields[i].value);
                } else if (extra_fields[i].id == 0x9A) { // Active equalization switch
                    cJSON_AddNumberToObject(system_status, "balancingActive", extra_fields[i].value);
                }
            }
            cJSON_AddItemToObject(pack_root, "systemStatus", system_status);
        }
        
        // Add system configuration
        cJSON *system_config = cJSON_CreateObject();
        if (system_config) {
            for (int i = 0; i < extra_fields_count; ++i) {
                if (extra_fields[i].id == 0xAA) { // Battery Capacity Settings
                    cJSON_AddNumberToObject(system_config, "batteryCapacityAh", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA9) { // Number of battery strings
                    cJSON_AddNumberToObject(system_config, "numberOfStrings", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xAF) { // Battery type
                    cJSON_AddNumberToObject(system_config, "batteryType", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xB1) { // Low Capacity Alarm Value
                    cJSON_AddNumberToObject(system_config, "lowCapacityAlarm", extra_fields[i].value);
                } else if (extra_fields[i].id == 0x99) { // Equalizing opening differential
                    cJSON_AddNumberToObject(system_config, "equalizingDifferentialMv", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xAD) { // Current Calibration
                    cJSON_AddNumberToObject(system_config, "currentCalibrationMa", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xB3) { // Special Charger Switch
                    cJSON_AddNumberToObject(system_config, "specialChargerSwitch", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xB0) { // Sleep Wait Time
                    cJSON_AddNumberToObject(system_config, "sleepWaitTimeSeconds", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xB8) { // Start Current Calibration
                    cJSON_AddNumberToObject(system_config, "startCurrentCalibration", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xAE) { // Protective Board 1 Address
                    cJSON_AddNumberToObject(system_config, "protectiveBoardAddress", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xB2 && extra_fields[i].is_ascii) { // Modify parameter password
                    cJSON_AddStringToObject(system_config, "modifyParameterPassword", extra_fields[i].strval);
                }
            }
            cJSON_AddItemToObject(pack_root, "systemConfig", system_config);
        }
        
        // Add temperature protection parameters
        cJSON *temp_protection = cJSON_CreateObject();
        if (temp_protection) {
            for (int i = 0; i < extra_fields_count; ++i) {
                if (extra_fields[i].id == 0x9B) { // Power tube temperature protection
                    cJSON_AddNumberToObject(temp_protection, "powerTubeTempProtectionC", extra_fields[i].value);
                } else if (extra_fields[i].id == 0x9F) { // Temperature protection in battery box
                    cJSON_AddNumberToObject(temp_protection, "batteryBoxTempProtectionC", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA0) { // Recovery value 2 of battery in box
                    cJSON_AddNumberToObject(temp_protection, "batteryBoxTempRecovery2C", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA1) { // Battery temperature difference
                    cJSON_AddNumberToObject(temp_protection, "batteryTempDifferenceC", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA2) { // Battery charging 2 high temp protection
                    cJSON_AddNumberToObject(temp_protection, "chargingHighTempProtection2C", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA3) { // High temp protection for charging
                    cJSON_AddNumberToObject(temp_protection, "chargingHighTempProtectionC", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA4) { // High temp protection for discharge
                    cJSON_AddNumberToObject(temp_protection, "dischargeHighTempProtectionC", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA5) { // Charging cryoprotection
                    cJSON_AddNumberToObject(temp_protection, "chargingLowTempProtectionC", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA6) { // Recovery value 2 of charge cryoprotection
                    cJSON_AddNumberToObject(temp_protection, "chargingLowTempRecovery2C", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA7) { // Discharge cryoprotection
                    cJSON_AddNumberToObject(temp_protection, "dischargeLowTempProtectionC", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xA8) { // Discharge low temp protection recovery
                    cJSON_AddNumberToObject(temp_protection, "dischargeLowTempRecoveryC", extra_fields[i].value);
                }
            }
            cJSON_AddItemToObject(pack_root, "temperatureProtection", temp_protection);
        }
        
        // Add system information
        cJSON *system_info = cJSON_CreateObject();
        if (system_info) {
            for (int i = 0; i < extra_fields_count; ++i) {
                if (extra_fields[i].id == 0xB4 && extra_fields[i].is_ascii) { // Device ID Code
                    cJSON_AddStringToObject(system_info, "deviceId", extra_fields[i].strval);
                } else if (extra_fields[i].id == 0xB5 && extra_fields[i].is_ascii) { // Date of production
                    cJSON_AddStringToObject(system_info, "productionDate", extra_fields[i].strval);
                } else if (extra_fields[i].id == 0xB7 && extra_fields[i].is_ascii) { // Software Version
                    cJSON_AddStringToObject(system_info, "softwareVersion", extra_fields[i].strval);
                } else if (extra_fields[i].id == 0xB6) { // System working time
                    cJSON_AddNumberToObject(system_info, "workingTimeMinutes", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xB9) { // Actual battery capacity
                    cJSON_AddNumberToObject(system_info, "actualCapacityAh", extra_fields[i].value);
                } else if (extra_fields[i].id == 0xBA && extra_fields[i].is_ascii) { // Factory ID
                    cJSON_AddStringToObject(system_info, "factoryId", extra_fields[i].strval);
                }
            }
            cJSON_AddItemToObject(pack_root, "systemInfo", system_info);
        }
        
        // Add expanded extra fields (comprehensive BMS system data)
        // Include ALL available extra fields to maximize data coverage
        for (int i = 0; i < extra_fields_count && i < MAX_EXTRA_FIELDS; ++i) { // Include all available fields
            const char *field_name = NULL;
            for (size_t j = 0; j < BMS_IDCODES_COUNT; ++j) {
                if (bms_idcodes[j].id == extra_fields[i].id) {
                    field_name = bms_idcodes[j].name;
                    break;
                }
            }
            if (field_name) {
                // Include ALL BMS fields for maximum data coverage
                uint8_t field_id = extra_fields[i].id;
                // Include all known BMS field IDs from the protocol definition
                // Core pack data: 0x82, 0x83, 0x84, 0x85 (already handled above in structured sections)
                // Balance & protection: 0x8F, 0x90, 0x91, 0x93, 0x94, 0x95, 0x96, 0x97
                // Configuration: 0x99, 0x9A, 0x9B, 0x9F, 0xA0-0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
                // System info: 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA
                // Extended fields: Any other fields that may be present
                if (field_id == 0x82 || field_id == 0x83 || field_id == 0x84 || field_id == 0x85 || 
                    field_id == 0x8F || field_id == 0x90 || field_id == 0x91 || field_id == 0x93 || 
                    field_id == 0x94 || field_id == 0x95 || field_id == 0x96 || field_id == 0x97 ||
                    field_id == 0x99 || field_id == 0x9A || field_id == 0x9B || field_id == 0x9F ||
                    (field_id >= 0xA0 && field_id <= 0xA8) || field_id == 0xA9 || field_id == 0xAA || 
                    field_id == 0xAB || field_id == 0xAC || field_id == 0xAD || field_id == 0xAE || field_id == 0xAF ||
                    field_id == 0xB0 || field_id == 0xB1 || field_id == 0xB2 || field_id == 0xB3 ||
                    field_id == 0xB4 || field_id == 0xB5 || field_id == 0xB6 || field_id == 0xB7 ||
                    field_id == 0xB8 || field_id == 0xB9 || field_id == 0xBA ||
                    // Include any other fields that might be present (for maximum coverage)
                    field_id >= 0x80) {
                    
                    char sanitized[64];
                    int si = 0;
                    for (int k = 0; field_name[k] && si < 63; ++k) {
                        char c = field_name[k];
                        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                            sanitized[si++] = c;
                        } else if (c == ' ' || c == '-' || c == '/' || c == '%') {
                            sanitized[si++] = '_';
                        }
                    }
                    sanitized[si] = '\0';
                    
                    if (extra_fields[i].is_ascii && extra_fields[i].strval[0]) {
                        cJSON_AddStringToObject(pack_root, sanitized, extra_fields[i].strval);
                    } else {
                        cJSON_AddNumberToObject(pack_root, sanitized, extra_fields[i].value);
                    }
                }
            }
        }
        
        // Add raw extra fields section for comprehensive debugging and analysis
        cJSON *raw_extra_fields = cJSON_CreateObject();
        if (raw_extra_fields) {
            char field_key[16];
            for (int i = 0; i < extra_fields_count && i < MAX_EXTRA_FIELDS; ++i) {
                snprintf(field_key, sizeof(field_key), "field_0x%02X", extra_fields[i].id);
                if (extra_fields[i].is_ascii && extra_fields[i].strval[0]) {
                    cJSON_AddStringToObject(raw_extra_fields, field_key, extra_fields[i].strval);
                } else {
                    cJSON_AddNumberToObject(raw_extra_fields, field_key, extra_fields[i].value);
                }
            }
            cJSON_AddItemToObject(pack_root, "rawExtraFields", raw_extra_fields);
        }
        cJSON_AddItemToObject(root, "pack", pack_root);
    }

    // Add cells data (volts only)
    cJSON *cells_root = cJSON_CreateObject();
    if (cells_root) {
        char cell_v_key[16];
        for (int i = 0; i < bms_data_ptr->num_cells; i++) {
            float cell_v = bms_data_ptr->cell_voltages[i];
            snprintf(cell_v_key, sizeof(cell_v_key), "cell%dV", i);
            cJSON_AddNumberToObject(cells_root, cell_v_key, cell_v);
        }
        cJSON_AddItemToObject(root, "cells", cells_root);
    }

    // Publish as a single topic
    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print combined cJSON to string.");
    } else {
        char topic[64];
        // Use bms_topic directly without any BMS prefix
        snprintf(topic, sizeof(topic), "%s", bms_topic);
        if (debug_logging) printf("[DEBUG] About to publish to topic: %s\n", topic);
        if (debug_logging) printf("[DEBUG] JSON payload length: %d\n", strlen(json_string));
        esp_mqtt_client_publish(mqtt_client, topic, json_string, 0, 1, 0);
        ESP_LOGI(TAG, "Published to %s (length: %d)", topic, strlen(json_string));
        free(json_string);
    }
    cJSON_Delete(root);
}

// Helper to convert hex char to int
int hex2int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

// URL decode utility (in-place)
void url_decode(char *dst, const char *src, size_t dstsize) {
    char a, b;
    size_t i = 0, j = 0;
    while (src[i] && j + 1 < dstsize) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            if ((a = src[i+1]) && (b = src[i+2])) {
                dst[j++] = hex2int(a) * 16 + hex2int(b);
                i += 3;
            } else {
                dst[j++] = src[i++];
            }
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

// DNS hijack task for captive portal
void dns_hijack_task(void *pvParameter) {
    // Remove from watchdog to prevent conflicts during configuration mode
    // esp_task_wdt_add(NULL);
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }
    
    // Set socket options for better reliability
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ESP_LOGW(TAG, "Failed to set SO_REUSEADDR: errno=%d", errno);
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket: errno=%d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "DNS hijack server started on UDP port 53");
    
    uint8_t buf[512];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    uint32_t dns_requests_handled = 0;
    
    // Set a more reasonable timeout
    struct timeval timeout;
    timeout.tv_sec = 5;  // Increased to 5 seconds
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        ESP_LOGW(TAG, "Failed to set socket timeout: errno=%d", errno);
    }
    
    while (!system_shutting_down) {  // Check shutdown flag
        int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &addr_len);
        
        if (len > 0 && len >= 12) {  // Valid DNS query minimum size
            dns_requests_handled++;
            ESP_LOGD(TAG, "DNS request #%lu received (%d bytes)", dns_requests_handled, len);
            
            // Minimal DNS response: copy ID, set response flags, answer count = 1
            uint8_t response[512];
            memcpy(response, buf, len);
            response[2] = 0x81; // QR=1, Opcode=0, AA=1, TC=0, RD=0
            response[3] = 0x80; // RA=1, Z=0, RCODE=0
            response[7] = 1;    // ANCOUNT = 1
            
            // Copy question section after header (12 bytes)
            int qlen = len - 12;
            if (qlen > 0 && qlen < 400) {  // Sanity check
                memcpy(response + 12, buf + 12, qlen);
                int pos = 12 + qlen;
                
                // Answer: pointer to name (0xC00C), type A, class IN, TTL, RDLENGTH, RDATA
                response[pos++] = 0xC0; response[pos++] = 0x0C;
                response[pos++] = 0x00; response[pos++] = 0x01; // Type A
                response[pos++] = 0x00; response[pos++] = 0x01; // Class IN
                response[pos++] = 0x00; response[pos++] = 0x00; response[pos++] = 0x00; response[pos++] = 0x3C; // TTL 60s
                response[pos++] = 0x00; response[pos++] = 0x04; // RDLENGTH 4
                response[pos++] = 192;  response[pos++] = 168;  response[pos++] = 4; response[pos++] = 1; // 192.168.4.1
                
                int sent = sendto(sock, response, pos, 0, (struct sockaddr *)&client_addr, addr_len);
                if (sent < 0) {
                    ESP_LOGW(TAG, "Failed to send DNS response: errno=%d", errno);
                } else {
                    ESP_LOGD(TAG, "DNS response sent (%d bytes)", sent);
                }
            } else {
                ESP_LOGW(TAG, "Invalid DNS query length: %d", qlen);
            }
        } else if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout - this is expected, continue
                ESP_LOGV(TAG, "DNS socket timeout (expected)");
            } else {
                ESP_LOGE(TAG, "DNS socket error: errno=%d", errno);
                break;
            }
        }
        
        // Yield to other tasks periodically
        if (dns_requests_handled % 10 == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    ESP_LOGI(TAG, "DNS hijack task shutting down gracefully (%lu requests handled)", dns_requests_handled);
    close(sock);
    vTaskDelete(NULL);
}

// Test function to manually trigger watchdog reset (for testing only)
void test_watchdog_reset() {
    ESP_LOGI(TAG, "=== TESTING WATCHDOG RESET ===");
    ESP_LOGI(TAG, "Current watchdog_reset_counter: %lu", watchdog_reset_counter);
    
    // Increment watchdog reset counter and save to NVS
    watchdog_reset_counter++;
    ESP_LOGI(TAG, "Incrementing watchdog reset counter to: %lu", watchdog_reset_counter);
    save_watchdog_counter_to_nvs();
    
    ESP_LOGI(TAG, "Test watchdog reset complete. Counter saved to NVS.");
}
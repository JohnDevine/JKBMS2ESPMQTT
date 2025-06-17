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

#define WIFI_NVS_NAMESPACE "wifi_cfg"
#define WIFI_NVS_KEY_SSID "ssid"
#define WIFI_NVS_KEY_PASS "pass"
#define MQTT_NVS_KEY_URL "broker_url"
#define DEFAULT_WIFI_SSID "BAANFARANG_O"
#define DEFAULT_WIFI_PASS "tAssy@#28"
#define DEFAULT_MQTT_BROKER_URL "mqtt://192.168.1.5"

static char wifi_ssid[33] = DEFAULT_WIFI_SSID;
static char wifi_pass[65] = DEFAULT_WIFI_PASS;
static char mqtt_broker_url[128] = DEFAULT_MQTT_BROKER_URL;

// Define the UART peripheral number to be used (UART2 in this case)
#define UART_NUM UART_NUM_2
// Define the GPIO pin for UART Transmit (TX)
#define UART_TX_PIN GPIO_NUM_17
// Define the GPIO pin for UART Receive (RX)
#define UART_RX_PIN GPIO_NUM_16
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
    // The actual 16-bit CRC sum is stored in the last 2 bytes of this 4-byte field.
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

    printf("Cell Voltages (V):\n");
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
             printf("  Cell %2d: %.3f V\n", i + 1, voltage_v);
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
        printf("Min Cell Voltage: %.3f V\n", min_cell_voltage);
        printf("Max Cell Voltage: %.3f V\n", max_cell_voltage);
        printf("Cell Voltage Delta: %.3f V\n", max_cell_voltage - min_cell_voltage);
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
        printf("MOSFET Temperature: %.1f C (raw: %u)\n", temp_fet, temp_fet_raw);
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
        printf("Probe 1 Temperature: %.1f C (raw: %u)\n", temp_1, temp_1_raw);
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
        printf("Probe 2 Temperature: %.1f C (raw: %u)\n", temp_2, temp_2_raw);
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
        printf("Total Battery Voltage: %.2f V (raw: %u)\n", total_v, total_voltage_raw);
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

        printf("Current: %.2f A (raw: %u)\n", current_a, current_raw);
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
        printf("Remaining Capacity (SOC): %u%%\n", soc_percent);
        current_bms_data.soc_percent = soc_percent; // Store data
        current_offset += 2; // Advance by ID(1) + Data(1).
    } else {
        ESP_LOGW(TAG, "SOC (ID 0x85) data missing or wrong ID at payload offset %d. Found ID: 0x%02X", 
                 current_offset, payload_len > current_offset ? payload[current_offset] : 0xFF);
    }
    
    // --- Parse additional fields from Data Identification Codes table ---
    extra_fields_count = 0;
    int extra_offset = current_offset;
    while (extra_offset < payload_len && extra_fields_count < MAX_EXTRA_FIELDS) {
        uint8_t id = payload[extra_offset];
        int found = 0;
        for (size_t i = 0; i < BMS_IDCODES_COUNT; ++i) {
            if (bms_idcodes[i].id == id) {
                uint32_t value = 0;
                int bytes = bms_idcodes[i].byte_len;
                const char *type = bms_idcodes[i].type;
                int is_ascii = (type && (strcmp(type, "Code") == 0 || strcmp(type, "Column") == 0));
                char strval[48] = {0};
                if (bytes > 0 && (extra_offset + bytes) < payload_len) {
                    if (is_ascii) {
                        int copylen = (bytes < 47) ? bytes : 47;
                        memcpy(strval, &payload[extra_offset + 1], copylen);
                        strval[copylen] = '\0';
                        // Remove non-printable chars
                        for (int s = 0; s < copylen; ++s) {
                            if (strval[s] < 32 || strval[s] > 126) strval[s] = '\0';
                        }
                    } else {
                        for (int b = 0; b < bytes; ++b) {
                            value = (value << 8) | payload[extra_offset + 1 + b];
                        }
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
                ESP_LOGI(TAG, "Decoded %s (0x%02X): %s%lu", bms_idcodes[i].name, id, is_ascii ? strval : "", is_ascii ? 0 : (unsigned long)value);
                extra_offset += 1 + bytes;
                found = 1;
                break;
            }
        }
        if (!found) extra_offset++;
    }

    printf("----------------------------------------\n"); // Separator for console output.

    // After parsing all data and populating current_bms_data
    // Call function to publish data via MQTT
    // This check ensures we only try to publish if we have some valid cell data
    if (current_bms_data.num_cells > 0) {
        publish_bms_data_mqtt(&current_bms_data);
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

// Main application entry point.
void app_main(void) {
    // Initialize UART communication.
    init_uart();

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    load_wifi_config_from_nvs();
    load_mqtt_config_from_nvs();
    ESP_LOGI(TAG, "WiFi SSID: %s", wifi_ssid);
    ESP_LOGI(TAG, "WiFi PASS: %s", wifi_pass);
    ESP_LOGI(TAG, "MQTT Broker URL: %s", mqtt_broker_url);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta(); // Initialize Wi-Fi
    
    // Buffer to attempt to read and discard UART echo. Size of the command sent.
    uint8_t echo_buf[sizeof(bms_read_all_cmd)]; 

    // Main loop to periodically send command and read response from BMS.
    while (1) {
        ESP_LOGI(TAG, "Sending '%s' command to BMS...", "Read All Data");
        // Send the pre-defined "Read All Data" command to the BMS.
        send_bms_command(bms_read_all_cmd, sizeof(bms_read_all_cmd));
        
        // Attempt to read and discard the UART echo immediately after sending.
        // The command is 21 bytes. At 115200 baud, transmission time is ~1.82ms.
        // Echo should appear very shortly after transmission completes.
        // A short timeout (e.g., 20ms) for uart_read_bytes should be sufficient for echo.
        ESP_LOGI(TAG, "Attempting to read and discard echo (%d bytes) with 20ms timeout...", (int)sizeof(bms_read_all_cmd));
        // Try to read exactly the number of bytes sent (the echo).
        int echo_read_len = uart_read_bytes(UART_NUM, echo_buf, sizeof(bms_read_all_cmd), pdMS_TO_TICKS(20));

        if (echo_read_len == sizeof(bms_read_all_cmd)) {
            ESP_LOGI(TAG, "Successfully read and discarded %d echo bytes.", echo_read_len);
            // ESP_LOG_BUFFER_HEX(TAG, echo_buf, echo_read_len); // Optional: log the echo if needed for debugging.
        } else if (echo_read_len > 0) {
            ESP_LOGW(TAG, "Partially read %d echo bytes (expected %d). The rest might be in the main read or BMS response is very fast.", 
                     echo_read_len, (int)sizeof(bms_read_all_cmd));
        } else { // echo_read_len <= 0 (0 means timeout, -1 means error from uart_read_bytes)
            ESP_LOGI(TAG, "No echo read or timeout/error during dedicated echo read attempt (read %d bytes). Main read will handle if echo is present.", echo_read_len);
        }
        // Even if this dedicated echo read fails or is partial, the `read_bms_data` function
        // has its own logic to detect and skip leading echo bytes in the main data buffer.

        // Call the function to read and process the actual BMS response.
        read_bms_data(); 
        
        // Wait for 5 seconds before sending the command again.
        ESP_LOGI(TAG, "Waiting 5 seconds before next BMS read cycle...");
        vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
}

// Wi-Fi Event Handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            ESP_LOGI(TAG, "connect to the AP fail");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        // Start MQTT client once IP is obtained
        mqtt_app_start();
    }
}

// Wi-Fi Initialization
void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
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
    // esp_mqtt_client_handle_t client = event->client; // Not used in this basic handler
    // int msg_id; // Not used here
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // Example: Subscribe to a topic (optional)
        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA: // Incoming data event (if subscribed)
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
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

// Placeholder for publishing BMS data
// This function will be expanded to create and send the two JSON messages
void publish_bms_data_mqtt(const bms_data_t *bms_data_ptr) {
    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized!");
        return;
    }

    ESP_LOGI(TAG, "Preparing to publish BMS data via MQTT...");

    // --- Determine topic prefix from Naming of factory ID ---
    char topic_prefix[128] = "BMS/";
    for (int i = 0; i < extra_fields_count; ++i) {
        if (extra_fields[i].is_ascii && extra_fields[i].strval[0]) {
            // Find the field name for this id
            for (size_t j = 0; j < BMS_IDCODES_COUNT; ++j) {
                if (bms_idcodes[j].id == extra_fields[i].id && strcmp(bms_idcodes[j].name, "Naming of factory ID") == 0) {
                    strncat(topic_prefix, extra_fields[i].strval, sizeof(topic_prefix) - strlen(topic_prefix) - 2); // leave space for '/\0'
                    size_t len = strlen(topic_prefix);
                    if (len < sizeof(topic_prefix) - 1 && topic_prefix[len-1] != '/') {
                        topic_prefix[len] = '/';
                        topic_prefix[len+1] = '\0';
                    }
                    break;
                }
            }
        }
    }
    char mqtt_topic_pack[160];
    char mqtt_topic_cells[160];
    snprintf(mqtt_topic_pack, sizeof(mqtt_topic_pack), "%spack", topic_prefix);
    snprintf(mqtt_topic_cells, sizeof(mqtt_topic_cells), "%scells", topic_prefix);


    // --- Create JSON for NodeJKBMS2/pack ---
    cJSON *pack_root = cJSON_CreateObject();
    if (pack_root == NULL) {
        ESP_LOGE(TAG, "Failed to create cJSON object for pack data.");
        return;
    }

    cJSON_AddNumberToObject(pack_root, "packV", bms_data_ptr->pack_voltage);
    cJSON_AddNumberToObject(pack_root, "packA", bms_data_ptr->pack_current);
    // Add other pack-specific fields here as they become available in bms_data_ptr
    // For example:
    // cJSON_AddNumberToObject(pack_root, "packRateCap", bms_data_ptr->pack_rate_cap);
    cJSON_AddNumberToObject(pack_root, "packNumberOfCells", bms_data_ptr->num_cells);
    // ... protectionStatus, packNumberCycles, etc. will be added later
    cJSON_AddNumberToObject(pack_root, "packSOC", bms_data_ptr->soc_percent);
    
    // Example for tempSensorValues (assuming NTC0=mosfet, NTC1=probe1, NTC2=probe2 for now)
    cJSON *temp_sensors = cJSON_CreateObject();
    if (temp_sensors) {
        cJSON_AddNumberToObject(temp_sensors, "NTC0", bms_data_ptr->mosfet_temp);
        cJSON_AddNumberToObject(temp_sensors, "NTC1", bms_data_ptr->probe1_temp);
        cJSON_AddNumberToObject(temp_sensors, "NTC2", bms_data_ptr->probe2_temp); // If probe2 exists
        cJSON_AddItemToObject(pack_root, "tempSensorValues", temp_sensors);
    }


    // --- Add all extra decoded fields to MQTT JSON output (handle ASCII fields as strings) ---
    for (int i = 0; i < extra_fields_count; ++i) {
        const char *field_name = NULL;
        for (size_t j = 0; j < BMS_IDCODES_COUNT; ++j) {
            if (bms_idcodes[j].id == extra_fields[i].id) {
                field_name = bms_idcodes[j].name;
                break;
            }
        }
        if (field_name) {
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
    // --- Publish using dynamic topic prefix ---
    char *pack_json_string = cJSON_PrintUnformatted(pack_root);
    if (pack_json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print pack cJSON to string.");
    } else {
        esp_mqtt_client_publish(mqtt_client, mqtt_topic_pack, pack_json_string, 0, 1, 0);
        ESP_LOGI(TAG, "Published to %s: %s", mqtt_topic_pack, pack_json_string);
        free(pack_json_string);
    }
    cJSON_Delete(pack_root);

    // --- Create JSON for cells and publish ---
    cJSON *cells_root = cJSON_CreateObject();
    if (cells_root == NULL) {
        ESP_LOGE(TAG, "Failed to create cJSON object for cells data.");
        return;
    }
    char cell_mv_key[16];
    char cell_v_key[16];
    for (int i = 0; i < bms_data_ptr->num_cells; i++) {
        float cell_v = bms_data_ptr->cell_voltages[i]; 
        int cell_mv = (int)(cell_v * 1000);
        snprintf(cell_mv_key, sizeof(cell_mv_key), "cell%dmV", i);
        snprintf(cell_v_key, sizeof(cell_v_key), "cell%dV", i);
        cJSON_AddNumberToObject(cells_root, cell_mv_key, cell_mv);
        cJSON_AddNumberToObject(cells_root, cell_v_key, cell_v);
    }
    char *cells_json_string = cJSON_PrintUnformatted(cells_root);
    if (cells_json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print cells cJSON to string.");
    } else {
        esp_mqtt_client_publish(mqtt_client, mqtt_topic_cells, cells_json_string, 0, 1, 0);
        ESP_LOGI(TAG, "Published to %s: %s", mqtt_topic_cells, cells_json_string);
        free(cells_json_string);
    }
    cJSON_Delete(cells_root);
}
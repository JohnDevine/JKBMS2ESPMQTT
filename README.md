# JKBMS2ESPMQTT Firmware

## Overview
This firmware enables an ESP32 (esp32doit-devkit-v1) to interface with a JK BMS (Jikong) over RS485, collect battery data, and publish it to an MQTT broker. The system is designed for robust, hands-off operation, with a heartbeat LED that flashes only on successful MQTT publishes and an automatic software watchdog for system recovery.

## Features
- Reads comprehensive data from JK BMS via RS485 (using MAX485 module)
- Publishes **complete BMS data** including all operational, safety, and configuration parameters to MQTT topics
- Heartbeat: onboard LED flashes only on successful MQTT publish (visual confirmation of communication)
- Captive portal for Wi-Fi and MQTT configuration
- MQTT topic is configurable via the web interface (see below)
- **Software watchdog timer** for automatic system recovery from communication failures
- **Comprehensive BMS data coverage** including temperature protection, calibration, and system information

## Heartbeat LED
The onboard LED (GPIO2) provides visual confirmation of system health:
- **LED Flash:** Occurs **only** when an MQTT message is successfully published and confirmed by the broker
- **No Flash:** Indicates MQTT communication issues (network problems, broker unavailable, etc.)
- **Reliability:** Combined with the software watchdog, this provides both visual monitoring and automatic recovery capabilities

## Pin Usage
| Signal         | ESP32 Pin      | Description                       |
|---------------|---------------|-----------------------------------|
| Onboard LED   | GPIO2         | Heartbeat indication              |
| UART TX       | GPIO17        | RS485 TX to MAX485 DI             |
| UART RX       | GPIO16        | RS485 RX from MAX485 RO           |
| Boot Button   | GPIO0         | Enter AP config mode on boot      |

**Note:** Connect MAX485 DE/RE to 3.3V (always transmit/receive enabled), or control via GPIO for advanced use.

## Hardware List
1. **ESP32 esp32doit-devkit-v1**
2. **MAX485 RS485 to TTL converter**
3. **JKBMS JIKONG RS485 CAN module and LCD display Adapter**
4. **JK BMS**

## Hardware Setup Example

![Project Hardware Running](Documents/Images/ProjectHardwareRunning.jpg)

*ESP32 development setup showing the actual hardware configuration with JK BMS, RS485 interface, and monitoring system in operation.*

## Completed Hardware Build

![Completed Hardware Build 1](Hardware/Images/Completed01.jpg)

![Completed Hardware Build 2](Hardware/Images/Completed02.jpg)

*Completed hardware installation showing the ESP32 BMS-to-MQTT converter integrated with JK BMS system for production monitoring.*

## Where to Buy

I just found this on AliExpress: THB229.89 | 1-10pcs ESP32 WROOM-32 ESP32-S Development Board WiFi+Bluetooth-compatible TYPE-C ESP32 30Pin ESP32 Nodemcu Development Module  
https://a.aliexpress.com/_oDbdH0o

I just found this on AliExpress: THB48.64 | 5PCS/LOT MAX485 module, RS485 module, TTL turn RS - 485 module, MCU development accessories  
https://a.aliexpress.com/_oFmcUde

I just found this on AliExpress: THB113.50 | JKBMS JIKONG RS485 CAN module  and LCD display Adapter Battery Examine and Repair Accessory  
https://a.aliexpress.com/_oF9ias4

I just found this on AliExpress: THB1,736.96 | Jikong JK BMS 4s 5s 6s 7s 8s 100a 200a Smart Lifepo4 Li-ion Active Balance Lithium 24v With Bt Heat Function Equalizer On Sale  
https://a.aliexpress.com/_oClgBbe

## Configuration & Usage

### WiFi Configuration Improvements
**Version 1.3.1** includes significant improvements to the WiFi configuration system:

- **Simplified SSID Entry**: Manual text input replaces unreliable WiFi network scanning
- **Better Reliability**: Eliminated intermittent issues with WiFi dropdown functionality  
- **Faster Setup**: No waiting for network scans - just type your network name
- **More Stable**: Reduced complexity leads to more consistent configuration experience

### 1. Set MQTT Topic (BMS Topic)
- The MQTT topic used for publishing is now fully configurable via the web interface (captive portal).
- On the configuration page, set the **BMS Topic** field. The firmware will publish to the topic `BMS/<YourTopic>` (e.g., `BMS/MyBattery`).
- The topic is stored in NVS and persists across reboots.

### 2. Wi-Fi & MQTT Setup (Captive Portal)
- On boot, if the boot button (GPIO0) is pressed within 10 seconds, the ESP32 will enter Wi-Fi Access Point mode.
- Connect to the `ESP32-CONFIG` Wi-Fi network from your phone or tablet.
- A captive portal page will appear, allowing you to set:
  - **Wi-Fi SSID** (manual text entry - simply type your network name)
  - **Wi-Fi Password**
  - **MQTT Broker URL** (e.g., `mqtt://192.168.1.23`)
  - **Sample interval** (milliseconds between each BMS read)
  - **BMS Topic** (see above)
  - **Pack Name** (identifier for your battery pack, appears in MQTT data)
- Save and reboot to apply settings.

### 3. Operation
- After booting, the ESP32 will connect to your Wi-Fi and MQTT broker.
- The onboard LED (GPIO2) will flash briefly **only when a message is successfully published** to the MQTT server—this is your heartbeat indicator.
- Data is published to the topic you set in the configuration page (e.g., `BMS/MyBattery`).
- The software watchdog automatically monitors system health and will reboot if MQTT communication fails for too long.

### 4. Additional Notes
- Ensure all hardware is wired as described above.
- The system is designed for continuous, unattended operation.
- For troubleshooting, monitor the serial log output via USB.

## MQTT Data Structure

The ESP32 publishes comprehensive BMS data in a structured JSON format to the configured MQTT topic. The payload includes over 30 different BMS parameters plus ESP32 system metrics organized into logical sections.

### Main Data Structure
```json
{
  // ESP32 processor metrics
  "processor": {
    "WiFiRSSI": 75,                    // WiFi signal strength (% - 0-100%)
    "IPAddress": "192.168.1.150",      // Current IP address
    "CPUTemperature": 32.0,            // ESP32 CPU temperature (°C - placeholder*)
    "SoftwareVersion": "1.3.0",        // Firmware version
    "WDTRestartCount": 2               // Watchdog restart counter
  },
  
  "pack": {
    "packName": "MyBatteryPack",         // Pack identifier (configurable)
    
    // Core pack data
    "packV": 13.28,                    // Total pack voltage (V)
    "packA": 0.0,                      // Pack current (A)
    "packNumberOfCells": 4,            // Number of cells in series
    "packSOC": 71,                     // State of charge (%)
    "packMinCellV": 3.320,             // Minimum cell voltage (V)
    "packMaxCellV": 3.324,             // Maximum cell voltage (V)
    "packCellVDelta": 0.004,           // Cell voltage difference (V)
    
    // Temperature sensors
    "tempSensorValues": {
      "NTC0": 28,                      // MOSFET temperature (°C)
      "NTC1": 25,                      // Probe 1 temperature (°C)
      "NTC2": 23                       // Probe 2 temperature (°C)
    },
    
    // System operational status
    "systemStatus": {
      "chargeMosfetStatus": 1,         // Charge MOSFET (0=OFF, 1=ON)
      "dischargeMosfetStatus": 1,      // Discharge MOSFET (0=OFF, 1=ON)
      "balancingActive": 0             // Cell balancing (0=OFF, 1=ON)
    },
    
    // Battery and system configuration
    "systemConfig": {
      "batteryCapacityAh": 320,        // Battery capacity (Ah)
      "numberOfStrings": 4,            // Number of battery strings
      "batteryType": 0,                // 0=LiFePO4, 1=Ternary, 2=LiTiO
      "currentCalibrationMa": 967,     // Current calibration (mA)
      "protectiveBoardAddress": 1,     // Board address for cascade
      "sleepWaitTimeSeconds": 10,      // Sleep timeout (seconds)
      "lowCapacityAlarm": 20,          // Low capacity alarm (%)
      "specialChargerSwitch": 0,       // Special charger (0=OFF, 1=ON)
      "startCurrentCalibration": 0,    // Start calibration (0=OFF, 1=ON)
      "modifyParameterPassword": 0     // Parameter password (if set)
    },
    
    // Temperature protection parameters
    "temperatureProtection": {
      "powerTubeTempProtectionC": 2700,           // Power tube limit (°C*100)
      "batteryBoxTempProtectionC": 65,            // Battery box limit (°C)
      "batteryBoxTempRecovery2C": 65,             // Battery box recovery (°C)
      "batteryTempDifferenceC": 60,               // Temperature difference (°C)
      "chargingHighTempProtection2C": 20,         // Charge high temp 2 (°C)
      "chargingHighTempProtectionC": 65,          // Charge high temp (°C)
      "dischargeHighTempProtectionC": 65,         // Discharge high temp (°C)
      "chargingLowTempProtectionC": 3,            // Charge low temp (°C)
      "chargingLowTempRecovery2C": 10,            // Charge low temp recovery (°C)
      "dischargeLowTempProtectionC": -20,         // Discharge low temp (°C)
      "dischargeLowTempRecoveryC": -10            // Discharge low temp recovery (°C)
    },
    
    // System information
    "systemInfo": {
      "deviceId": "0E42910D",                     // Device ID code
      "productionDate": "2407",                   // Production date (YYMM)
      "workingTimeMinutes": 280900,               // Total working time (minutes)
      "softwareVersion": "11.XA_S11.45___",      // Software version
      "actualCapacityAh": 320,                    // Actual capacity (Ah)
      "factoryId": "Test"                         // Factory ID
    },
    
    // Individual named fields (for compatibility)
    "Device_ID_Code": "0E42910D",
    "Battery_Capacity_Settings": 320,
    "Number_of_battery_strings_settings": 4,
    // ... (additional named fields)
    
    // Raw field data (for debugging and analysis)
    "rawExtraFields": {
      "field_0xB4": "0E42910D",        // Device ID (hex field 0xB4)
      "field_0xAA": 320,               // Battery capacity (hex field 0xAA)
      "field_0xA9": 4,                 // Number of strings (hex field 0xA9)
      // ... (all available BMS fields with hex IDs)
    }
  },
  
  // Individual cell voltages
  "cells": {
    "cell0V": 3.320,                   // Cell 1 voltage (V)
    "cell1V": 3.322,                   // Cell 2 voltage (V)
    "cell2V": 3.320,                   // Cell 3 voltage (V)
    "cell3V": 3.320                    // Cell 4 voltage (V)
  }
}
```

### Data Categories

#### 1. **Processor Metrics** (ESP32 System Data)
- WiFi signal strength (percentage, 0-100%)
- Network configuration (IP address)
- Hardware monitoring (CPU temperature - *currently placeholder implementation*)
- Firmware version information
- System reliability (watchdog restart count)

> **Note**: CPU temperature currently returns a placeholder value (25-45°C range). A full implementation would require enabling the ESP-IDF temperature sensor component in menuconfig.

#### 2. **Core Pack Data**
- Voltage, current, SOC, cell count
- Cell voltage statistics (min, max, delta)
- Essential operational parameters

#### 3. **Temperature Monitoring**
- MOSFET temperature (power electronics)
- Temperature probe readings (2 external probes)
- Temperature protection and recovery thresholds

#### 4. **System Status**
- MOSFET switch states (charge/discharge)
- Active balancing status
- Operational mode indicators

#### 5. **Configuration Parameters**
- Battery specifications (capacity, type, strings)
- Calibration values (current, voltage)
- Protection settings and thresholds

#### 6. **System Information**
- Device identification and version info
- Production date and factory data
- Operational statistics (working time)

#### 7. **Cell Data**
- Individual cell voltages
- Cell-level monitoring data

### Data Reliability
- **Payload size**: ~2,900-2,920 characters (includes processor metrics)
- **Update frequency**: Every 5-6 seconds (configurable)
- **Comprehensive coverage**: All 30+ available BMS protocol fields
- **Multiple formats**: Structured sections + individual named fields + raw hex data
- **Backward compatibility**: Named fields maintain compatibility with existing consumers

### Integration Examples

**Node-RED**: Use the structured JSON to create comprehensive dashboards
**InfluxDB**: Store time-series data for all parameters
**Grafana**: Create detailed monitoring dashboards
**Home Assistant**: Monitor battery health and status
**Custom applications**: Use raw field data for specialized analysis

## Quick Start
1. Wire up the hardware as described above.
2. Flash the firmware to your ESP32.
3. On first boot, hold the boot button (GPIO0) to enter Wi-Fi AP config mode.
4. Configure Wi-Fi, MQTT, and BMS Topic via the captive portal.
5. System will begin reading BMS data and publishing to MQTT. Watch the onboard LED for heartbeat.

## How to Build

This project uses [PlatformIO](https://platformio.org/) for building and flashing the firmware to the ESP32.

### Prerequisites
- [PlatformIO Core](https://platformio.org/install) (or use the PlatformIO extension for VS Code)
- Supported OS: Windows, macOS, or Linux
- USB cable for ESP32

### Build & Upload Steps
1. Clone or download this repository.
2. Open the project folder in VS Code with PlatformIO, or use a terminal.
3. Connect your ESP32 board via USB.
4. **Build the firmware:**
   ```sh
   pio run
   ```
5. **Build the filesystem image (for /data web content):**
   ```sh
   pio run --target buildfs
   ```
6. **Upload (flash) the firmware to the ESP32:**
   ```sh
   pio run --target upload
   ```
7. **Upload the filesystem image:**
   ```sh
   pio run --target uploadfs
   ```
8. **Monitor the serial output:**
   ```sh
   pio device monitor
   ```

The default environment is set for `esp32doit-devkit-v1` in `platformio.ini`. If you use a different ESP32 board, adjust the environment accordingly.

## License
See LICENSE file.

## Version Management

This project uses a dual-file version management system for consistency between development and runtime display.

### Version Files
- **Master Version File:** `version.txt` (project root)
  - **Purpose:** Source of truth for project version
  - **Usage:** Update this file when releasing new versions
  - **Version Control:** Track version changes in git commits
  
- **Runtime Version File:** `data/version.txt` (SPIFFS filesystem)
  - **Purpose:** Displays version in web interface
  - **Usage:** Must be kept in sync with master version file
  - **Deployment:** Gets uploaded to ESP32 via SPIFFS

### Version Update Process
1. **Update master version:** Edit `version.txt` in project root
2. **Sync to runtime version:** Copy the same version to `data/version.txt`
3. **Build filesystem:** `pio run --target buildfs`
4. **Upload filesystem:** `pio run --target uploadfs`
5. **Build and upload firmware:** `pio run --target upload`

### Important Notes
- **Keep files in sync:** Both version files must contain identical version numbers
- **Web interface:** Reads version from `/spiffs/version.txt` (the data/ directory file)
- **Documentation:** Update README and any version references when releasing
- **Format:** Use semantic versioning (e.g., 1.2.1) without extra whitespace

**Current Version:** 1.3.1

## Programming Documentation

This project includes comprehensive programming documentation to help developers understand the codebase architecture and implementation details.

### High-Level Programming Overview
**Location:** `Planning-prompts/high-level-programming-overview.md`

**Purpose:** Provides a high-level architectural overview of all functions in the firmware, organized by functional area. This document is ideal for:
- **New developers** getting familiar with the codebase
- **System architects** understanding overall design
- **Code reviewers** needing functional context
- **Feature planning** and system modifications

**Contents:**
- Function signatures with clear purpose descriptions
- Organization by functional categories (UART, MQTT, WiFi, etc.)
- Program flow summary and system architecture
- Key data structures and configuration constants
- No implementation details - focuses on "what" rather than "how"

### Low-Level Programming Overview  
**Location:** `Planning-prompts/low-level-programming-overview.md`

**Purpose:** Provides detailed implementation analysis including internal logic, parameter specifications, and protocol details. This document is essential for:
- **Debugging** and troubleshooting issues
- **Code modification** and feature development
- **Performance optimization** and tuning
- **Protocol understanding** and integration work

**Contents:**
- Complete parameter lists with types and descriptions
- Step-by-step internal logic breakdown for each function
- Data structure definitions with field explanations
- BMS communication protocol byte-by-byte documentation
- Error handling mechanisms and memory management details
- Hardware interface specifications (GPIO, UART, WiFi, etc.)
- Network protocol implementation details

### Usage Recommendations
- **Start with high-level overview** to understand system architecture
- **Reference low-level overview** when modifying specific functions
- **Use both together** for comprehensive understanding during development
- **Keep documents updated** when making significant code changes

## Software Watchdog Timer (System Reliability)

This firmware includes a robust software watchdog timer to ensure the ESP32 remains responsive and automatically recovers from communication failures, network issues, or system hangs.

### How It Works
- **Timeout Calculation:** The watchdog timeout is set to **10× the sample interval** (as configured in the captive portal, in milliseconds).
- **Automatic Enable/Inactive:** 
  - **Enabled** when timeout (10× sample interval) > 10,000 ms (i.e., sample interval > 1,000 ms)
  - **Inactive** when timeout ≤ 10,000 ms (i.e., sample interval ≤ 1,000 ms, for safety with frequent sampling)
- **Reset Condition:** The watchdog timer is reset **only after successful MQTT publish confirmation** (`MQTT_EVENT_PUBLISHED`) - this occurs at the same time the LED flashes.
- **Timeout Action:** If no successful MQTT publish occurs within the timeout period, the ESP32 will automatically reboot and restart from the beginning (including the 10-second boot button wait).

### Examples
| Sample Interval | Watchdog Status | Timeout Period | 
|-----------------|----------------|----------------|
| 500 ms (0.5s)   | Inactive       | N/A (timeout: 5,000 ms) |
| 1,000 ms (1s)   | Inactive       | N/A (timeout: 10,000 ms) |
| 1,500 ms (1.5s) | Enabled        | 15,000 ms (15 seconds) |
| 5,000 ms (5s)   | Enabled        | 50,000 ms (50 seconds) |
| 15,000 ms (15s) | Enabled        | 150,000 ms (2.5 min) |
| 30,000 ms (30s) | Enabled        | 300,000 ms (5 min) |
| 60,000 ms (1m)  | Enabled        | 600,000 ms (10 min) |

### Visual Feedback
- **LED Heartbeat:** The onboard LED (GPIO2) flashes **only** when an MQTT publish is successful, providing visual confirmation of system health.
- **Watchdog Reset:** The software watchdog timer is reset at the exact same moment the LED flashes (both triggered by successful MQTT publish confirmation).
- **No Flash:** If the LED stops flashing, it indicates MQTT communication issues, and the watchdog will eventually trigger a recovery reboot.

### Logging
- Watchdog status and timeout values are logged at startup.
- Timeout events are logged before system restart for troubleshooting.
- Most debug logging is conditional to reduce serial output volume, but watchdog logs are always active.

### Purpose
This feature guarantees robust, unattended operation by automatically recovering from:
- Network connectivity issues
- MQTT broker unavailability
- BMS communication failures
- System software hangs or deadlocks

**No user configuration required** — the watchdog operates automatically based on your sample interval setting.

### Important: Two Watchdog Systems

This firmware uses **two different watchdog systems**:

#### 1. Software Watchdog (MQTT Communication Monitor)
- **Purpose:** Monitors MQTT communication health
- **Trigger:** Only when timeout (10× sample interval) > 10,000ms
- **Timeout:** 10× sample interval
- **Reset Condition:** Successful MQTT publish confirmation (same moment LED flashes)
- **Action:** System restart with logs: "Software watchdog timeout! Forcing system restart..."

#### 2. ESP-IDF Task Watchdog (TWDT) - System Monitor
- **Purpose:** Monitors overall system task execution
- **Always Active:** 30-second timeout (configured in firmware, overrides default 5s config setting)
- **Normal Behavior:** May show warnings during BMS communication delays, but does not cause reboot
- **Logs:** "Task watchdog got triggered" with backtrace (informational only)
- **Auto-Recovery:** The main task feeds the TWDT regularly, including during BMS operations

The TWDT warnings you may see in logs are **normal** when no BMS is connected or MQTT broker is unavailable. These are informational and help with debugging but do not indicate a problem. The system automatically feeds the TWDT during normal operation to prevent false timeouts.

### Task Watchdog Timer (TWDT) Configuration and Interaction

The ESP-IDF Task Watchdog Timer (TWDT) is configured via menuconfig. The most important setting is:

- **Task Watchdog timeout period (seconds):** This sets the maximum time any watched task (including the main task) can go without feeding the TWDT. The default is 5 seconds, but for this firmware you should set it to **30 seconds** or higher.

**How to set:**
1. Run `pio run -t menuconfig`
2. Go to: `Component config` → `ESP System Settings` → `Task Watchdog Timer`
3. Set **Task Watchdog timeout period (seconds)** to `30` (or higher if needed)
4. Save and rebuild

**IMPORTANT:**
- The **software watchdog timeout** (which is `sample interval × 10`, in milliseconds) must always be **less than the Task Watchdog timeout period** (converted to milliseconds) set in menuconfig. 
    - For example, if the Task Watchdog timeout is 30 seconds, then `sample interval × 10` must be **less than 30,000 ms**.
    - If the software watchdog timeout is longer than the TWDT, the system may reset due to the TWDT before the software watchdog can trigger.
- Both watchdogs are independent, but the Task Watchdog is a system-level safety net and will always take precedence if its timeout is reached first.

## Debug Logging

The firmware includes a debug logging control to manage serial output verbosity for development and troubleshooting.

### Debug Logging Control
- **Location:** `src/main.c`, line ~71
- **Variable:** `static bool debug_logging = false;`
- **Default:** Disabled (`false`) for production use

### What It Controls
When `debug_logging = true`, the following additional logs are enabled:
- **BMS Data Parsing:** Individual cell voltages, temperatures, pack voltage/current/SOC
- **Raw Protocol Data:** Hex dump of BMS response frames (first 50 bytes)
- **MQTT Publishing:** Payload lengths, topic information, publish status
- **Detailed Operation:** Step-by-step BMS communication and data processing

### Always Active (Independent of Debug Flag)
The following logs are **always** shown regardless of the debug setting:
- **Watchdog logs:** Timeout values, enable/disable status, timeout events
- **System startup:** Initialization, configuration values, Wi-Fi connection
- **MQTT events:** Connection status, basic publish confirmations, errors
- **Critical errors:** BMS communication failures, system issues
- **ESP System Logs:** At ESP_LOG_WARN level and above

### How to Enable Debug Logging
1. Open `src/main.c`
2. Find the line: `static bool debug_logging = false;` (around line 71)
3. Change to: `static bool debug_logging = true;`
4. Rebuild and upload the firmware: `pio run --target upload`

**Optional: Increase ESP Log Level for More System Details**
1. Find line ~1583: `esp_log_level_set("*", ESP_LOG_WARN);`
2. Change to: `esp_log_level_set("*", ESP_LOG_INFO);` for detailed system logs
3. Or use: `esp_log_level_set("*", ESP_LOG_DEBUG);` for maximum verbosity

### When to Use
- **Production/Normal Use:** Keep disabled (`false`) for cleaner logs and better performance
- **Development/Troubleshooting:** Enable (`true`) for detailed BMS data analysis
- **System Monitoring:** Watchdog and critical logs are always visible

This approach ensures essential system information is always logged while allowing detailed debugging when needed.

## Debug Logging Configuration

This project supports detailed debug logging to help with troubleshooting and development.

### Global Log Level Configuration

**Method 1: PlatformIO menuconfig (Recommended)**
```sh
pio run --target menuconfig
```
Navigate to: `Component config > Log output > Default log verbosity`
- Set to **Debug** for ESP_LOGD() messages
- Set to **Verbose** for ESP_LOGV() messages  
- Set to **Info** for normal operation (default level)

**Method 2: Runtime Configuration**
```c
// In your code, set global log level
esp_log_level_set("*", ESP_LOG_DEBUG);        // All modules to DEBUG
esp_log_level_set("BMS_READER", ESP_LOG_DEBUG); // Specific module only
```

### Per-File Debug Logging

To enable debug logging for the main BMS code specifically:

1. **Edit `src/main.c`** - Uncomment the line at the top:
   ```c
   #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
   ```

2. **Rebuild the project:**
   ```sh
   pio run
   ```

### Log Levels (Priority: High → Low)
- **ESP_LOG_ERROR (E)** - Error conditions, always shown
- **ESP_LOG_WARN (W)** - Warning conditions  
- **ESP_LOG_INFO (I)** - Normal operational messages (default level)
- **ESP_LOG_DEBUG (D)** - Debug information for troubleshooting
- **ESP_LOG_VERBOSE (V)** - Detailed verbose output

### Example Debug Output
When debug logging is enabled, you'll see detailed information about:
- BMS data parsing and field extraction
- MQTT connection and publish events
- WiFi scanning and connection details
- Configuration loading/saving
- Device ID code processing
- JSON generation and structure

### Monitoring Serial Output
```sh
pio device monitor --baud 115200
```

**Note:** Debug logging significantly increases serial output and may impact performance. Use INFO level for production deployments.

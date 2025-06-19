# JKBMS2ESPMQTT Firmware

## Overview
This firmware enables an ESP32 (esp32doit-devkit-v1) to interface with a JK BMS (Jikong) over RS485, collect battery data, and publish it to an MQTT broker. The system is designed for robust, hands-off operation, with a heartbeat LED blink each time a message is sent to MQTT.

## Features
- Reads data from JK BMS via RS485 (using MAX485 module)
- Publishes battery data to MQTT topics
- Heartbeat: onboard LED blinks each time a message is sent to MQTT (visual confirmation of operation)
- Captive portal for Wi-Fi and MQTT configuration
- MQTT topic prefix is settable via the BMS “User Private Data” field

## Heartbeat LED
Each time a message is successfully sent to the MQTT broker, the onboard LED (GPIO2) will blink briefly. This provides a visual heartbeat to indicate the system is running and communicating.

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

### 1. Set MQTT Topic Prefix
Use the Jikong configuration app to change the “User Private Data” field in your JK BMS. The MQTT topic will be `BMS/` plus the contents of this field. For example, if you set "MyPack", the topic will be `BMS/MyPack/pack` and `BMS/MyPack/cells`.

### 2. Wi-Fi & MQTT Setup (Captive Portal)
- On boot, if the boot button (GPIO0) is pressed within 10 seconds, the ESP32 will enter Wi-Fi Access Point mode.
- Connect to the `ESP32-CONFIG` Wi-Fi network from your phone or tablet.
- A captive portal page will appear, allowing you to set:
  - Wi-Fi SSID
  - Wi-Fi Password
  - MQTT Broker URL (e.g., `mqtt://192.168.1.23`)
  - Sample interval (milliseconds between each BMS read)
- Save and reboot to apply settings.

### 3. Operation
- After booting, the ESP32 will connect to your Wi-Fi and MQTT broker.
- The onboard LED (GPIO2) will flash briefly each time a message is posted to the MQTT server—this is your heartbeat indicator.
- Data is published to MQTT topics with the prefix you set in the BMS.

### 4. Additional Notes
- Ensure all hardware is wired as described above.
- The system is designed for continuous, unattended operation.
- For troubleshooting, monitor the serial log output via USB.

## Quick Start
1. Wire up the hardware as described above.
2. Flash the firmware to your ESP32.
3. Configure the BMS “User Private Data” field for your desired MQTT topic prefix.
4. On first boot, hold the boot button (GPIO0) to enter Wi-Fi AP config mode.
5. Configure Wi-Fi and MQTT via the captive portal.
6. System will begin reading BMS data and publishing to MQTT. Watch the onboard LED for heartbeat.

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

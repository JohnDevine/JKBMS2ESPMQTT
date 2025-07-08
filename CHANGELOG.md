# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.1] - 2025-07-08

### Added
- **Pack Name Configuration**: Added pack name field to web configuration interface
- **Pack Name in MQTT**: Pack name now appears as first field in MQTT JSON payload under "pack" object
- **Persistent Pack Name**: Pack name is stored in NVS and persists across reboots

### Changed
- **Simplified WiFi Configuration**: Removed WiFi network scan and dropdown functionality
- **Manual SSID Entry**: WiFi SSID configuration now uses simple text input field
- **Improved Reliability**: Eliminated unreliable WiFi scan logic that caused configuration issues

### Removed
- **WiFi Scan Feature**: Removed automatic WiFi network detection and dropdown selection
- **Scan/Manual Toggle**: Removed UI toggle between scan and manual SSID entry modes
- **Scan JavaScript**: Removed all WiFi scanning related JavaScript code

### Fixed
- **Configuration Reliability**: Web configuration interface now works consistently
- **Pack Name Handling**: Fixed NVS storage and retrieval of pack name
- **MQTT JSON Structure**: Pack name properly positioned in JSON payload
- **Code Cleanup**: Removed unused functions and variables

### Technical Details
- **NVS Storage**: Pack name stored with "pack_name" key in NVS
- **JSON Position**: Pack name appears as first field: `"pack": {"packName": "value", ...}`
- **Default Value**: Pack name defaults to "BatteryPack" if not configured
- **Web Interface**: Streamlined parameters.html with simplified JavaScript

## [1.3.0] - 2025-07-08

### Added
- **Comprehensive BMS Data Coverage**: Added all available BMS protocol fields (30+ parameters)
- **Structured MQTT Payload**: Organized data into logical sections:
  - `systemStatus`: MOSFET states, balancing status
  - `systemConfig`: Battery configuration, calibration, protection settings
  - `temperatureProtection`: Complete temperature protection parameters
  - `systemInfo`: Device identification, version info, operational statistics
- **Raw Field Access**: Added `rawExtraFields` section with hex field IDs for debugging
- **Enhanced Temperature Monitoring**: All temperature protection thresholds and recovery values
- **Configuration Parameters**: Battery type, capacity, strings, calibration values
- **System Information**: Device ID, production date, software version, working time
- **Individual Named Fields**: Backward compatibility with existing field names

### Changed
- **Increased Field Limit**: Expanded from 25 to all available BMS fields (32 max)
- **Payload Size**: Increased to ~2,760-2,780 characters (from ~2,000)
- **Field Coverage**: Now includes ALL protocol-defined fields for maximum data richness

### Technical Details
- **New Structured Sections**: 5 organized data categories in JSON payload
- **Field Expansion**: Covers field IDs 0x99-0xBA (all available BMS parameters)
- **Data Reliability**: Maintains 5-6 second update frequency with larger payload
- **System Stability**: No performance impact with expanded data set

### Documentation
- **MQTT Data Structure**: Added comprehensive JSON payload documentation
- **Field Reference**: Detailed explanation of all data categories
- **Integration Examples**: Usage patterns for various monitoring systems
- **Version Management**: Updated all version references to 1.3.0

## [1.2.1] - Previous Version

### Features
- Basic BMS data reading and MQTT publishing
- Core pack data (voltage, current, SOC, cell voltages)
- Temperature sensors (MOSFET, 2 probes)
- WiFi captive portal configuration
- Software watchdog timer
- LED heartbeat indicator
- Limited BMS field coverage (25 fields)

---

## Upgrade Notes

### From 1.3.0 to 1.3.1
- **WiFi Configuration**: WiFi SSID must now be entered manually (no more dropdown scan)
- **Pack Name**: New pack name field available in configuration interface
- **Better Reliability**: Web configuration interface is more stable and consistent
- **No Data Loss**: All existing configuration values preserved during upgrade

### From 1.2.1 to 1.3.0
- **Backward Compatibility**: All existing fields remain available
- **Enhanced Data**: Additional fields provide deeper BMS insights
- **Payload Size**: Larger JSON payload (verify MQTT broker limits)
- **New Features**: Utilize structured sections for better data organization

### Configuration
- **WiFi Setup**: Enter SSID manually instead of selecting from dropdown
- **Pack Name**: Configure pack identifier in web interface (optional)
- **All Other Settings**: Remain unchanged and preserved

### Integration
- **Pack Name**: New field available as first item in "pack" object
- **Existing Fields**: All continue to work unchanged
- **MQTT Structure**: Pack name addition does not break existing consumers

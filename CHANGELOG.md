# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

### From 1.2.1 to 1.3.0
- **Backward Compatibility**: All existing fields remain available
- **Enhanced Data**: Additional fields provide deeper BMS insights
- **Payload Size**: Larger JSON payload (verify MQTT broker limits)
- **New Features**: Utilize structured sections for better data organization

### Configuration
- No configuration changes required
- All existing settings preserved
- New fields available immediately after update

### Integration
- Existing consumers continue to work unchanged
- New structured sections available for enhanced integrations
- Raw field access enables specialized analysis applications

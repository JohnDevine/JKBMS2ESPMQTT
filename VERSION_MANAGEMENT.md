# Version Management Notes

## When updating version:

1. Update `version.txt` (this file's directory - project root)
2. Copy the same version to `data/version.txt` 
3. Update the fallback version in `src/main.c` (search for "Default fallback")
4. Update "Current Version" in README.md
5. Build and deploy:
   ```bash
   pio run --target buildfs
   pio run --target uploadfs
   pio run --target upload
   ```

## Current Status:
- Master version: 1.4.2 (version.txt)
- Runtime version: 1.4.2 (data/version.txt)
- Code fallback: 1.4.2 (src/main.c)
- README reference: 1.4.2

All version references are in sync ✓

## Version 1.4.2 Changes:
- **WiFi Configuration Improvements**: Manual text input for SSID eliminates unreliable network scanning
- **Better Reliability**: Improved configuration system stability and consistency
- **Version Management Documentation**: Comprehensive dual-file version management system (version.txt + data/version.txt)
- **Comprehensive MQTT Data**: Maintains all 30+ BMS parameters with processor metrics and watchdog monitoring
- **Stable Operation**: Production-ready firmware with automatic recovery capabilities

## Previous Version (1.4.1) Includes:
- **Enhanced BMS Data Coverage**: Comprehensive BMS data fields including all protection parameters
- **Structured MQTT Payload**: Organized data into logical sections (systemStatus, systemConfig, temperatureProtection, systemInfo)
- **Raw Field Access**: rawExtraFields section for debugging and specialized analysis

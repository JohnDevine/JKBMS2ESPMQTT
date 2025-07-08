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
- Master version: 1.3.0 (version.txt)
- Runtime version: 1.3.0 (data/version.txt)
- Code fallback: 1.3.0 (src/main.c)
- README reference: 1.3.0

All version references are in sync ✓

## Version 1.3.0 Changes:
- **Enhanced BMS Data Coverage**: Added comprehensive BMS data fields including all protection parameters, calibration settings, and system information
- **Structured MQTT Payload**: Organized data into logical sections (systemStatus, systemConfig, temperatureProtection, systemInfo)
- **Raw Field Access**: Added rawExtraFields section for debugging and specialized analysis
- **Increased Field Limit**: Expanded from 25 to all available BMS fields (32 max)
- **Extended Documentation**: Added comprehensive MQTT data structure documentation
- **Payload Size**: Increased to ~2,760-2,780 characters with all available BMS data
- **System Stability**: Maintained reliable operation with larger payload size

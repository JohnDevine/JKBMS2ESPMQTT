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
- Master version: 1.2.1 (version.txt)
- Runtime version: 1.2.1 (data/version.txt)
- Code fallback: 1.2.1 (src/main.c)
- README reference: 1.2.1

All version references are in sync ✓

# BMS-RS485 Communication Protocol

## Version History

- **V20191124**: First draft  
- **V20200325**: Update description info, change 0xa10 to 0xd2, add special charger switch command  
- **V20200325**: Define baud rate as 115200  
- **V20200329**: Redefine data ID code, optimize instruction table  
- **V20200427**: Add write ID and factory date  
- **V20200429**: Add 0xb7 (software version), detailed 0x8b/0x8c descriptions  
- **V20200508–20200512**: Optimize address units, redefines, and alarm bits  
- **V20200526–20200713**: Add restart ID, factory reset version v2.1  
- **V20200825**: Add 0xBE, 0xBF  
- **V2.4–V2.5**: Redefine 0x84, add reporting field descriptions

---

## 1. Summary

Defines the communication protocol between monitoring platform and battery terminal, including message format, transmission and communication modes.

---

## 2. Reference Standard

- TCP over 2G (GPRS), GAT1 over 4G
- Socket interface mode, RS232TTL
- Baud rate: **115200**

---

## 3. Network Topology

Point-to-point or bus mode between BMS, GPS, Bluetooth, PC, and terminals.

---

## 4. Protocol Content

### 4.1 Communication Rules

- Active reporting and passive response frames
- Min packet interval: **100ms**
- Max reply delay: **5s**
- Broadcast regularly
- Send activation info if device is asleep

---

### 4.2 Frame Format

Each frame includes:
1. **Start Frame**: `0x4E` ("N") + `0x57` ("W")
2. **Length**: 2 bytes (includes checksum and length)
3. **Terminal Number**: 4 bytes (FF FF FF FF...)
4. **Command Word**: 1 byte (see below)
5. **Frame Source**: 1 byte (0 = BMS, 1 = Bluetooth, 2 = GPS, 3 = PC)
6. **Transmission Type**: 1 byte (0 = read, 1 = reply, 2 = BMS active upload)
7. **Frame Info**: BMS data fields
8. **Record Number**: 4 bytes (high byte = random, low 3 = record code)
9. **End ID**: `0x68`
10. **Checksum**: 4 bytes (CRC optional; top 2 bytes = 0 if unused)

---

### 4.3 Command Words

| Command | Meaning |
|---------|---------|
| `0x01`  | Activate BMS |
| `0x02`  | Write parameters |
| `0x03`  | Read parameters |
| `0x05`  | Password auth |
| `0x06`  | Read all data |

---

### 5.1 Data Identification Codes (Selection)

| ID     | Meaning                          | Type | Unit / Notes                       |
|--------|----------------------------------|------|------------------------------------|
| `0x79` | Single cell voltage              | R    | 3*n bytes, per cell                |
| `0x80` | Tube temperature                 | R    | 2 bytes, HEX                       |
| `0x81` | Battery box temp                 | R    | 2 bytes, 0–140 (°C)                |
| `0x83` | Total battery voltage            | R    | 2 bytes, 0.01V                     |
| `0x84` | Current                          | R    | 2 bytes, 0.01A, signed             |
| `0x85` | Remaining battery (SOC)         | R    | 1 byte, 0–100%                     |
| `0x87` | Battery cycle count              | R    | 2 bytes                            |
| `0x8B` | Battery warnings                 | R    | 2 bytes, bitfield (low capacity, over-temp, etc.) |
| `0x8C` | Battery status                   | R    | Bit flags: MOS on/off, balancing   |
| `0xAA` | Capacity setting                 | RW   | 4 bytes, Ah                        |
| `0xAB` | Charge MOS switch               | RW   | 1 byte: 0 = off, 1 = on            |
| `0xB1` | Low capacity alarm              | RW   | 1 byte: 0–80%                      |
| `0xB2` | Parameter write password        | RW   | 10 bytes                           |
| `0xB3` | Special charger switch          | RW   | 1 byte: 0 = off, 1 = on            |
| `0xB7` | Software version                | R    | String                             |
| `0xBB` | Restart system                  | W    | 1 byte: 1 = restart                |
| `0xBC` | Restore factory settings        | W    | 1 byte: 1 = reset                  |
| `0xC0` | Protocol version                | R    | 1 byte                             |

> Many more IDs are included in the full list; contact support if needed for legacy or advanced features.

---

## Notes

- IDs marked **R** are read-only, **RW** are readable/writable, **W** are write-only.
- Always allow firmware upgrades for compatibility.
- Field `0xBA` (factory ID) is required for cabinet switching systems.

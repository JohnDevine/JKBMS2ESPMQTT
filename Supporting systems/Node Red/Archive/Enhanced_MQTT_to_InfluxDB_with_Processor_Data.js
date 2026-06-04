// === Enhanced Node Red Function Node for InfluxDB ===
// Includes both battery pack data AND processor data fields

// === InfluxDB Auth Token ===
const token = "CQcuM3A6AWK7p6z4wDl6eCkWPN-4QAeya-MVy7Yv7NEby2QVN0GMXJmyhnpFsS9h6YzOn0H9rW0yeXvVs9oxag==";  // Replace this with your actual token

// === Line Protocol Setup ===
const measurement = "battery";
const tags = {
    packName: msg.payload.pack.packName,
    deviceId: msg.payload.pack.Device_ID_Code,
    factory: msg.payload.pack.Naming_of_factory_ID
};

// === Extract Pack Fields ===
const pack = msg.payload.pack;
const temps = pack.tempSensorValues || {};
const cells = msg.payload.cells || {};
const processor = msg.payload.processor || {};

const fields = {
    // === PROCESSOR DATA ===
    wifiRSSI: processor.WiFiRSSI,
    ipAddress: processor.IPAddress ? `"${processor.IPAddress}"` : undefined,
    cpuTemperature: processor.CPUTemperature,
    softwareVersion: processor.SoftwareVersion ? `"${processor.SoftwareVersion}"` : undefined,
    wdtRestartCount: processor.WDTRestartCount,
    
    // === BATTERY PACK DATA ===
    packV: pack.packV,
    packA: pack.packA,
    packNumberOfCells: pack.packNumberOfCells,
    packSOC: pack.packSOC,
    packMinCellV: pack.packMinCellV,
    packMaxCellV: pack.packMaxCellV,
    packCellVDelta: pack.packCellVDelta,
    numberOfStrings: pack.Number_of_battery_strings_settings,
    capacityAh: pack.Battery_Capacity_Settings,
    chargeMosfet: pack.Charging_MOS_switch,
    dischargeMosfet: pack.Discharge_MOS_switch,
    boardAddr: pack.Protective_Board_1_Address,
    batteryType: pack.Battery_type,
    productionDate: pack.Date_of_production ? `"${pack.Date_of_production}"` : undefined,
    workingTime: pack.System_working_time,
    softwareVer: pack.Software_Version_Number ? `"${pack.Software_Version_Number}"` : undefined,
    currentCal: pack.Start_Current_Calibration,
    actualCapacity: pack.Actual_battery_capacity,
    
    // === TEMPERATURE SENSORS ===
    NTC0: temps.NTC0,
    NTC1: temps.NTC1,
    NTC2: temps.NTC2
};

// Add cell voltages (up to 16 if present)
Object.keys(cells).forEach(cell => {
    const val = cells[cell];
    if (typeof val === 'number') {
        fields[cell] = val;
    }
});

// === Convert to Line Protocol ===
const tagString = Object.entries(tags)
    .map(([k, v]) => `${k}=${v}`)
    .join(',');

const fieldString = Object.entries(fields)
    .filter(([_, v]) => v !== undefined && v !== null)
    .map(([k, v]) => {
        if (typeof v === "string") return `${k}=${v}`;
        return `${k}=${v}`;
    })
    .join(',');

const timestamp = Date.now() * 1_000_000; // nanoseconds
msg.payload = `${measurement},${tagString} ${fieldString} ${timestamp}`;

// === HTTP Request Setup ===
msg.headers = {
    "Authorization": "Token " + token,
    "Content-Type": "text/plain"
};
msg.method = "POST";
msg.url = "http://influxdb2:8086/api/v2/write?org=solarblue&bucket=BMS&precision=ns";

return msg;

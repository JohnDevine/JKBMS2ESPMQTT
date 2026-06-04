[
    {
        "id": "79076cf9a50d2158",
        "type": "tab",
        "label": "BMS2Influx",
        "disabled": false,
        "info": "",
        "env": []
    },
    {
        "id": "f5758cf2ab98606b",
        "type": "influxdb out",
        "z": "79076cf9a50d2158",
        "influxdb": "2ed0bc98bcc324f8",
        "name": "Write to Influx  V 1.8 measurement BMS",
        "measurement": "BMS",
        "precision": "",
        "retentionPolicy": "one_week",
        "database": "influxdb",
        "precisionV18FluxV20": "ms",
        "retentionPolicyV18Flux": "",
        "org": "organisation",
        "bucket": "bucket",
        "x": 610,
        "y": 400,
        "wires": []
    },
    {
        "id": "5d369b989e8a66a1",
        "type": "mqtt in",
        "z": "79076cf9a50d2158",
        "name": "Battery Bank pack Info",
        "topic": "BMS/JKBMS",
        "qos": "2",
        "datatype": "json",
        "broker": "4e9759aa5d998d78",
        "nl": false,
        "rap": true,
        "rh": 0,
        "inputs": 0,
        "x": 120,
        "y": 120,
        "wires": [
            [
                "e6599eefd14f9211"
            ]
        ]
    },
    {
        "id": "7b674e45de16e2b2",
        "type": "http request",
        "z": "79076cf9a50d2158",
        "name": "Write to INFLUX 2",
        "method": "use",
        "ret": "txt",
        "paytoqs": "ignore",
        "url": "",
        "tls": "",
        "persist": false,
        "proxy": "",
        "insecureHTTPParser": false,
        "authType": "",
        "senderr": false,
        "headers": [],
        "x": 730,
        "y": 120,
        "wires": [
            []
        ]
    },
    {
        "id": "e6599eefd14f9211",
        "type": "function",
        "z": "79076cf9a50d2158",
        "name": "Setup data for insert into database 2",
        "func": "// === Enhanced Node-RED Function Node for InfluxDB ===\n// Includes battery pack data AND processor data fields with packA calibration\n\n// === InfluxDB Auth Token ===\nconst token = \"CQcuM3A6AWK7p6z4wDl6eCkWPN-4QAeya-MVy7Yv7NEby2QVN0GMXJmyhnpFsS9h6YzOn0H9rW0yeXvVs9oxag==\";  // Replace with your actual token\n\n// === Line Protocol Setup ===\nconst measurement = \"battery\";\nconst tags = {\n    packName: msg.payload.pack.packName,\n    deviceId: msg.payload.pack.Device_ID_Code,\n    factory: msg.payload.pack.Naming_of_factory_ID\n};\n\n// === Extract Pack Fields ===\nconst pack = msg.payload.pack;\nconst temps = pack.tempSensorValues || {};\nconst cells = msg.payload.cells || {};\nconst processor = msg.payload.processor || {};\n\n// === Calibration for packA ===\n/* \nif (typeof pack.packA === 'number' &&\n    pack.Software_Version_Number?.startsWith(\"11.XA_S11.45\")) {\n\n    let R = pack.packA;   // raw reading\n    let S;\n\n    // --- Special calibration anchor ---\n    if (Math.abs(R - 100) < 2.5) {  // allow a small tolerance, e.g. 99.5–100.5\n        S = 0;\n    } else {\n        // --- Normal region calibration ---\n        const zeroPoint = 13.04; // Reads value corresponding to true zero\n        const a = -0.00116;\n        const b = 0.762;\n\n        if (R >= zeroPoint) {\n            // Positive region\n            S = a * Math.pow(R - zeroPoint, 2) + b * (R - zeroPoint);\n        } else if (R < 0) {\n            // Negative region: old linear fit for negative readings\n            S = -0.963 * R - 217.5;\n        } else {\n            // Smooth blend between 0 → zeroPoint\n            const pos = a * Math.pow(R - zeroPoint, 2) + b * (R - zeroPoint);\n            const neg = -0.963 * R - 217.5;\n            const w = R / zeroPoint; // weight from 0 → zeroPoint\n            S = neg * (1 - w) + pos * w;\n        }\n    }\n\n    pack.packA = S;\n}\n*/\n\n// === Construct Fields ===\nconst fields = {\n    // === PROCESSOR DATA ===\n    wifiRSSI: processor.WiFiRSSI,\n    ipAddress: processor.IPAddress ? `\"${processor.IPAddress}\"` : undefined,\n    cpuTemperature: processor.CPUTemperature,\n    softwareVersion: processor.SoftwareVersion ? `\"${processor.SoftwareVersion}\"` : undefined,\n    wdtRestartCount: processor.WDTRestartCount,\n\n    // === BATTERY PACK DATA ===\n    packV: pack.packV,\n    packA: pack.packA,\n    packNumberOfCells: pack.packNumberOfCells,\n    packSOC: pack.packSOC,\n    packMinCellV: pack.packMinCellV,\n    packMaxCellV: pack.packMaxCellV,\n    packCellVDelta: pack.packCellVDelta,\n    numberOfStrings: pack.Number_of_battery_strings_settings,\n    capacityAh: pack.Battery_Capacity_Settings,\n    chargeMosfet: pack.Charging_MOS_switch,\n    dischargeMosfet: pack.Discharge_MOS_switch,\n    boardAddr: pack.Protective_Board_1_Address,\n    batteryType: pack.Battery_type,\n    productionDate: pack.Date_of_production ? `\"${pack.Date_of_production}\"` : undefined,\n    workingTime: pack.System_working_time,\n    softwareVer: pack.Software_Version_Number ? `\"${pack.Software_Version_Number}\"` : undefined,\n    currentCal: pack.Start_Current_Calibration,\n    actualCapacity: pack.Actual_battery_capacity,\n\n    // === TEMPERATURE SENSORS ===\n    NTC0: temps.NTC0,\n    NTC1: temps.NTC1,\n    NTC2: temps.NTC2\n};\n\n// Add cell voltages (up to 16 if present)\nObject.keys(cells).forEach(cell => {\n    const val = cells[cell];\n    if (typeof val === 'number') {\n        fields[cell] = val;\n    }\n});\n\n// === Convert to Line Protocol ===\nconst tagString = Object.entries(tags)\n    .map(([k, v]) => `${k}=${v}`)\n    .join(',');\n\nconst fieldString = Object.entries(fields)\n    .filter(([_, v]) => v !== undefined && v !== null)\n    .map(([k, v]) => {\n        if (typeof v === \"string\") return `${k}=${v}`;\n        return `${k}=${v}`;\n    })\n    .join(',');\n\nconst timestamp = Date.now() * 1_000_000; // nanoseconds\nmsg.payload = `${measurement},${tagString} ${fieldString} ${timestamp}`;\n\n// === HTTP Request Setup ===\nmsg.headers = {\n    \"Authorization\": \"Token \" + token,\n    \"Content-Type\": \"text/plain\"\n};\nmsg.method = \"POST\";\nmsg.url = \"http://influxdb2:8086/api/v2/write?org=solarblue&bucket=BMS&precision=ns\";\n\nreturn msg;",
        "outputs": 1,
        "timeout": 0,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 430,
        "y": 120,
        "wires": [
            [
                "7b674e45de16e2b2"
            ]
        ]
    },
    {
        "id": "2ed0bc98bcc324f8",
        "type": "influxdb",
        "hostname": "influxdb",
        "port": 8086,
        "protocol": "http",
        "database": "BMS",
        "name": "New BMS database",
        "usetls": false,
        "tls": "",
        "influxdbVersion": "1.x",
        "url": "http://localhost:8086",
        "timeout": 10,
        "rejectUnauthorized": true
    },
    {
        "id": "4e9759aa5d998d78",
        "type": "mqtt-broker",
        "name": "MQTT on RaspberryJack",
        "broker": "mqtt://mosquitto:1883",
        "port": "1883",
        "clientid": "",
        "autoConnect": true,
        "usetls": false,
        "protocolVersion": "4",
        "keepalive": "60",
        "cleansession": true,
        "autoUnsubscribe": true,
        "birthTopic": "",
        "birthQos": "0",
        "birthRetain": "false",
        "birthPayload": "",
        "birthMsg": {},
        "closeTopic": "",
        "closeQos": "0",
        "closeRetain": "false",
        "closePayload": "",
        "closeMsg": {},
        "willTopic": "",
        "willQos": "0",
        "willRetain": "false",
        "willPayload": "",
        "willMsg": {},
        "userProps": "",
        "sessionExpiry": ""
    },
    {
        "id": "e306d2b43bce4465",
        "type": "global-config",
        "env": [],
        "modules": {
            "node-red-contrib-influxdb": "0.7.0"
        }
    }
]
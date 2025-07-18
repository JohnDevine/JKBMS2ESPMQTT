option task = {
  name: "battery_30s_aggregation",
  every: 1m,
  offset: 15s
}

from(bucket: "BMS")
  |> range(start: -3m, stop: -30s)  // More recent data window
  |> filter(fn: (r) => r._measurement == "battery")
  |> filter(fn: (r) => 
    r._field == "packV" or 
    r._field == "packA" or 
    r._field == "packSOC" or 
    r._field == "cell0V" or 
    r._field == "cell1V" or 
    r._field == "cell2V" or 
    r._field == "cell3V" or
    r._field == "NTC0" or
    r._field == "NTC1" or
    r._field == "NTC2" or
    r._field == "wifiRSSI" or
    r._field == "cpuTemperature"
  )
  |> aggregateWindow(every: 30s, fn: mean, createEmpty: false)
  |> to(bucket: "BMS_30s", org: "solarblue")

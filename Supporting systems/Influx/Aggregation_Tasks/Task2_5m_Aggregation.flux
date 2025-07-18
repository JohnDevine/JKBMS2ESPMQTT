option task = {
  name: "battery_5m_aggregation", 
  every: 5m,
  offset: 30s
}

from(bucket: "BMS_30s")
  |> range(start: -6m, stop: -1m)
  |> filter(fn: (r) => r._measurement == "battery")
  |> aggregateWindow(every: 5m, fn: mean, createEmpty: false)
  |> to(bucket: "BMS_5m", org: "solarblue")

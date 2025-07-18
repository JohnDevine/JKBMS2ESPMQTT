option task = {
  name: "battery_1h_aggregation",
  every: 1h, 
  offset: 2m
}

from(bucket: "BMS_5m")
  |> range(start: -65m, stop: -5m)
  |> filter(fn: (r) => r._measurement == "battery")
  |> aggregateWindow(every: 1h, fn: mean, createEmpty: false)
  |> to(bucket: "BMS_1h", org: "solarblue")

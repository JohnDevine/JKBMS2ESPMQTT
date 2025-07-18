// === COMPLETE QUERY FOR INDIVIDUAL CELL VOLTAGES - Query A (Battery Pack 01) ===
// Copy this entire block into your Grafana panel Query A

// Calculate time range duration in seconds (simpler approach)
timeRangeDuration = uint(v: v.timeRangeStop) - uint(v: v.timeRangeStart)

bucketName = if timeRangeDuration <= 21600000000000 then "BMS"              // ≤ 6 hours: raw 5s data
             else if timeRangeDuration <= 172800000000000 then "BMS_30s"    // ≤ 2 days: 30s aggregates
             else if timeRangeDuration <= 1209600000000000 then "BMS_5m"    // ≤ 2 weeks: 5m aggregates
             else "BMS_1h"                                                   // > 2 weeks: 1h aggregates

from(bucket: bucketName)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) =>
    r._measurement == "battery" and
    r.packName == "${Battery_pack_01}" and
    (r._field == "cell0V" or r._field == "cell1V" or r._field == "cell2V" or r._field == "cell3V" or
     r._field == "cell4V" or r._field == "cell5V" or r._field == "cell6V" or r._field == "cell7V" or
     r._field == "cell8V" or r._field == "cell9V" or r._field == "cell10V" or r._field == "cell11V" or
     r._field == "cell12V" or r._field == "cell13V" or r._field == "cell14V" or r._field == "cell15V")
  )
  |> map(fn: (r) => ({
    _time: r._time,
    _value: r._value,
    _field: "${Battery_pack_01}_" + r._field
  }))

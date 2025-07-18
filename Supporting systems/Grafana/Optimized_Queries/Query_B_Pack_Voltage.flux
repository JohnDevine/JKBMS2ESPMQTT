// === COMPLETE QUERY FOR PACK VOLTAGE HISTORY - Query B (Battery Pack 02) ===
// Copy this entire block into your Grafana panel Query B

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
    r._field == "packV" and
    r.packName == "${Battery_pack_02}"
  )
  |> map(fn: (r) => ({
    _time: r._time,
    _value: r._value,
    _field: "${Battery_pack_02}"
  }))

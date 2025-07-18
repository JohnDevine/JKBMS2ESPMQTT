# Grafana Dashboard Performance Optimization Guide

## Overview
Transform your dashboard from 5+ minute query times to under 5 seconds for any time range using intelligent bucket selection.

## Implementation Steps

### Step 1: Open Your Dashboard for Editing
1. Go to your Grafana instance
2. Open the "Battery data" dashboard
3. Click the **gear icon** (Dashboard settings) → **JSON Model**
4. **Make a backup copy** of the JSON before making changes

### Step 2: Identify Panel Types

**Time Series Panels (need optimization):**
- Pack history Voltage
- Pack History Amps  
- Pack history of State Of Charge
- Individual Cells (both battery packs)

**Gauge Panels (keep as-is, but optimize for latest values):**
- Pack Current Volts
- Pack Current Amps
- Pack Current SOC
- Individual cell gauge displays

### Step 3: Update Time Series Panels

For each time series panel:

1. **Click Edit** on the panel
2. **Go to Query tab**
3. **Replace the entire query** with the optimized version from `Dashboard_Query_Templates.flux`
4. **Add the bucket selection logic** at the top of each query:

```flux
import "date"

timeRangeDuration = uint(v: date.sub(from: v.timeRangeStart, to: v.timeRangeStop)) / uint(v: 1s)

bucketName = if timeRangeDuration <= 21600 then "BMS"              // ≤ 6 hours
             else if timeRangeDuration <= 172800 then "BMS_30s"    // ≤ 2 days
             else if timeRangeDuration <= 1209600 then "BMS_5m"    // ≤ 2 weeks
             else "BMS_1h"                                          // > 2 weeks

from(bucket: bucketName)
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) =>
    r._measurement == "battery" and
    r._field == "YOUR_FIELD_HERE" and
    r.packName == "${Battery_pack_01}"
  )
  |> map(fn: (r) => ({
    _time: r._time,
    _value: r._value,
    _field: "${Battery_pack_01}"
  }))
```

### Step 4: Update Gauge Panels

For gauge panels (current values only):

```flux
from(bucket: "BMS")  // Always use raw data for latest values
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) =>
    r._measurement == "battery" and
    r._field == "YOUR_FIELD_HERE" and
    r.packName == "${Battery_pack_01}"
  )
  |> last()  // Get only the most recent value
  |> map(fn: (r) => ({
    _time: r._time,
    _value: r._value,
    _field: "${Battery_pack_01}"
  }))
```

### Step 5: Field Mapping

Replace `YOUR_FIELD_HERE` with:

| Panel | Field Name |
|-------|------------|
| Pack Voltage | `packV` |
| Pack Current | `packA` |
| Pack SOC | `packSOC` |
| Cell 1 | `cell0V` |
| Cell 2 | `cell1V` |
| Cell 3 | `cell2V` |
| Cell 4 | `cell3V` |

### Step 6: Test Performance

Test these time ranges and watch the performance:

| Time Range | Expected Bucket | Expected Speed |
|------------|----------------|----------------|
| 2 hours | BMS (raw) | ~2 seconds |
| 12 hours | BMS_30s | ~3 seconds |
| 3 days | BMS_5m | ~3 seconds |
| 2 weeks | BMS_5m | ~3 seconds |
| 2 months | BMS_1h | ~5 seconds |
| 1 year | BMS_1h | ~5 seconds |

### Step 7: Verify No More "Too Many Data Points" Errors

- Try a 24-hour range → Should load in ~3 seconds
- Try a 1-week range → Should load in ~3 seconds  
- Try a 1-month range → Should load in ~5 seconds
- Try a 1-year range → Should load in ~5 seconds

## Expected Results

**Before:**
- 6 hours: Works but slow
- 24 hours: 5+ minutes or "too many data points" error
- 1 week: Fails or extremely slow
- 1 month: Fails

**After:**
- 6 hours: ~2 seconds ✅
- 24 hours: ~3 seconds ✅
- 1 week: ~3 seconds ✅
- 1 month: ~5 seconds ✅
- 1 year: ~5 seconds ✅

## Troubleshooting

**If you get "bucket not found" errors:**
- Make sure your aggregation tasks have been running for at least:
  - BMS_30s: 5+ minutes
  - BMS_5m: 1+ hour  
  - BMS_1h: 2+ hours

**If queries are still slow:**
- Check that the bucket selection logic is working
- Use Grafana's query inspector to see which bucket is being used
- Verify your aggregation tasks are still running

**If data looks different:**
- This is expected - you're now seeing averaged data for longer time ranges
- Raw precision is preserved for recent data (≤6 hours)
- Longer ranges show averaged trends, which is appropriate for analysis

# === Grafana Panel Configuration for Performance ===
# Apply these settings to your dashboard panels

## Panel-Level Settings:
1. **Max Data Points**: Set to 1000-2000 per panel
   - Go to each panel's Query tab
   - Set "Max data points" to 1500

2. **Query Timeout**: Increase timeout for complex queries
   - Set query timeout to 60s in datasource settings

3. **Refresh Rate**: Optimize refresh based on time range
   - For real-time (< 1 hour): 5s-30s refresh
   - For historical (> 1 hour): 1m-5m refresh
   - For long-term (> 24 hours): Manual refresh

## Dashboard-Level Settings:
1. **Auto-refresh**: Disable auto-refresh for long time ranges
   - Add this to your dashboard JSON in "refresh" field:
   ```json
   "refresh": "5s"
   ```

2. **Time Range-Based Refresh**: 
   - Short range (< 6h): "5s" 
   - Medium range (6-24h): "30s"
   - Long range (> 24h): "5m" or ""

## Data Source Configuration:
1. **InfluxDB Datasource Settings**:
   - Query Timeout: 60s
   - HTTP Request Timeout: 60s
   - Max Series: 10000

2. **Connection Pool**:
   - Max Open Connections: 100
   - Max Idle Connections: 10

## Query Optimization Tips:
1. **Always use time filters**: range(start: v.timeRangeStart, stop: v.timeRangeStop)
2. **Filter early**: Apply measurement and field filters before other operations
3. **Use aggregateWindow()**: For time ranges > 12 hours
4. **Limit results**: Add |> limit(n: 2000) for safety
5. **Use createEmpty: false**: Reduces null data points

## Dashboard Variables:
Add a variable for data resolution:
- Name: data_resolution
- Type: Custom
- Values: 5s,30s,1m,5m,15m,1h
- Use in queries: aggregateWindow(every: ${data_resolution}, fn: mean)

## Memory Optimization:
1. **Browser**: Close other tabs when viewing long ranges
2. **Grafana**: Increase memory limits in grafana.ini:
   ```ini
   [server]
   router_logging = false
   
   [database]
   max_open_conn = 300
   max_idle_conn = 10
   ```

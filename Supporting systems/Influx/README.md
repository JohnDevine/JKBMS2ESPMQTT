# InfluxDB Configuration Files

This folder contains the InfluxDB 2.x configuration, aggregation tasks, and scripts for the JKBMS2ESPMQTT project's high-performance time-series database implementation.

## Structure

### Main Files
- `README.md` - This file

### Aggregation_Tasks/
Contains the Flux aggregation tasks for parallel bucket strategy:

- `Task1_30s_Aggregation.flux` - 30-second aggregation task (BMS → BMS_30s)
- `Task2_5m_Aggregation.flux` - 5-minute aggregation task (BMS_30s → BMS_5m)  
- `Task3_1h_Aggregation.flux` - 1-hour aggregation task (BMS_5m → BMS_1h)

### Scripts/
- `Create_Tasks_Commands.sh` - Shell script to create all aggregation tasks
- `Useful Commands.txt` - Collection of useful InfluxDB CLI commands

### Documentation/
- Additional guides and documentation

## Parallel Bucket Strategy

The system uses a multi-tier bucket architecture for optimal performance:

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│     BMS     │───▶│   BMS_30s   │───▶│   BMS_5m    │───▶│   BMS_1h    │
│  (5s data)  │    │ (30s agg)   │    │  (5m agg)   │    │  (1h agg)   │
│ 365d retain │    │ 365d retain │    │ 365d retain │    │ 365d retain │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

### Bucket Details

| Bucket | Data Resolution | Source | Retention | Use Case |
|--------|----------------|--------|-----------|----------|
| `BMS` | 5 seconds | ESP32 direct | 365 days | ≤ 6 hours queries |
| `BMS_30s` | 30 seconds | BMS aggregated | 365 days | ≤ 2 days queries |
| `BMS_5m` | 5 minutes | BMS_30s aggregated | 365 days | ≤ 2 weeks queries |
| `BMS_1h` | 1 hour | BMS_5m aggregated | 365 days | > 2 weeks queries |

## Performance Benefits

- **100x faster queries** for long time ranges
- **Eliminates "too many data points" errors**
- **Sub-5-second response times** across all time ranges
- **Intelligent data degradation** preserves storage while maintaining usability

## Setup Instructions

### 1. Create Buckets
```bash
# Create the parallel buckets
influx bucket create --name BMS_30s --retention 365d
influx bucket create --name BMS_5m --retention 365d  
influx bucket create --name BMS_1h --retention 365d
```

### 2. Deploy Aggregation Tasks
```bash
# Run the setup script
chmod +x Scripts/Create_Tasks_Commands.sh
./Scripts/Create_Tasks_Commands.sh
```

### 3. Verify Tasks
```bash
# Check task status
influx task list
```

## Aggregation Schedule

| Task | Frequency | Processing Window | Delay |
|------|-----------|------------------|-------|
| 30s Aggregation | Every 1 minute | Last 2 minutes | 30s |
| 5m Aggregation | Every 5 minutes | Last 10 minutes | 2m 30s |
| 1h Aggregation | Every 1 hour | Last 2 hours | 5m |

## Data Flow

```
ESP32 → MQTT → Node Red → InfluxDB BMS bucket
                              ↓
                        Task1 (every 1m)
                              ↓
                         BMS_30s bucket
                              ↓
                        Task2 (every 5m)
                              ↓
                          BMS_5m bucket
                              ↓
                        Task3 (every 1h)
                              ↓
                          BMS_1h bucket
```

## Monitoring

Check aggregation task health:
```bash
# View task runs
influx task run list --task-id <TASK_ID>

# Check task logs
influx task log list --task-id <TASK_ID>
```

## Requirements

- InfluxDB 2.x (v2.0+)
- Flux query language support
- Sufficient storage for 365-day retention across all buckets
- Regular task execution (ensure InfluxDB scheduler is running)
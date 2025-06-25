# Supporting Systems

## Custom Instructions
The following assumes that you have setup docker using the .yml file in the Docker instructions.

The database neewds to be created first from the cli. To do that open a terminal.
from there enter:
docker exec -it influxdb influx

You will get a > prompt. At the prompt enter:

CREATE DATABASE BMS
CREATE RETENTION POLICY "one_week" ON "BMS" DURATION 7d REPLICATION 1 DEFAULT
EXIT
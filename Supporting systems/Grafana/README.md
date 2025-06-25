# Supporting Systems

## Custom Instructions

This assumes you have already installed docker.

Do the following in this order:

1. via a browser "http://localhost:3000"
    This will load Grafana

2. Set up the Database connection via
    connections/Data Sources
    Select Influx as the DB
    Name = influxdb-BMS
    Url=http://influxdb:8086
    Database=BMS

    Then click on "Save & Test"

3. Import the script "Grafana Dashboard.txt"

    Go to Dashboards and you should find a dashboard called "Dual BMS" it is a dashboard  I created to view two BMSs together. My system is a pair of 12v LiFEPo4 battery banks (Each bank has 4 cells and a JK BMS connected to the ESP32) connected in parallel.

    I had a problem in that it would not display the panel. If so just go into edit on    each of the panels the save them. No changes needed, just edit, do nothing, then save.

## Dashboard Screenshot

![Grafana BMS Dashboard](../Images/GrafanaBMS.jpg)


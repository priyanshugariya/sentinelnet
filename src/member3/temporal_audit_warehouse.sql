-- DBMS Unit 3 & 4: Materialized Threat Rollup Warehouse (Aanya)
CREATE TABLE IF NOT EXISTS m3_raw_events (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    hour_bucket TEXT NOT NULL,
    src_ip TEXT NOT NULL,
    attack_type TEXT NOT NULL,
    packet_count INTEGER NOT NULL,
    total_bytes INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS mv_hourly_threat_rollup (
    hour_bucket TEXT NOT NULL,
    attack_type TEXT NOT NULL,
    distinct_attackers INTEGER NOT NULL,
    total_packets INTEGER NOT NULL,
    total_mb REAL NOT NULL,
    refreshed_at TEXT DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (hour_bucket, attack_type)
);

INSERT INTO m3_raw_events (hour_bucket, src_ip, attack_type, packet_count, total_bytes) VALUES
('2026-11-12 10:00', '192.168.1.45', 'SYN_FLOOD', 4500, 6660000),
('2026-11-12 10:00', '172.16.4.99',  'SYN_FLOOD', 3200, 4736000),
('2026-11-12 10:00', '10.24.8.12',   'SQLI_PROBE', 180,  270000);

-- Materialized View Refresh (Atomic Upsert)
INSERT OR REPLACE INTO mv_hourly_threat_rollup (hour_bucket, attack_type, distinct_attackers, total_packets, total_mb)
SELECT hour_bucket, attack_type, COUNT(DISTINCT src_ip), SUM(packet_count), ROUND(SUM(total_bytes) / 1048576.0, 2)
FROM m3_raw_events
GROUP BY hour_bucket, attack_type;

SELECT * FROM mv_hourly_threat_rollup;

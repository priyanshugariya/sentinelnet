PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE m3_raw_events (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    hour_bucket TEXT NOT NULL,
    src_ip TEXT NOT NULL,
    attack_type TEXT NOT NULL,
    packet_count INTEGER NOT NULL,
    total_bytes INTEGER NOT NULL
);
INSERT INTO m3_raw_events VALUES(1,'2026-11-12 10:00','192.168.1.45','SYN_FLOOD',4500,6660000);
INSERT INTO m3_raw_events VALUES(2,'2026-11-12 10:00','172.16.4.99','SYN_FLOOD',3200,4736000);
INSERT INTO m3_raw_events VALUES(3,'2026-11-12 10:00','10.24.8.12','SQLI_PROBE',180,270000);
CREATE TABLE mv_hourly_threat_rollup (
    hour_bucket TEXT NOT NULL,
    attack_type TEXT NOT NULL,
    distinct_attackers INTEGER NOT NULL,
    total_packets INTEGER NOT NULL,
    total_mb REAL NOT NULL,
    refreshed_at TEXT DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (hour_bucket, attack_type)
);
INSERT INTO mv_hourly_threat_rollup VALUES('2026-11-12 10:00','SQLI_PROBE',1,180,0.260000000000000008,'2026-11-12 19:16:26');
INSERT INTO mv_hourly_threat_rollup VALUES('2026-11-12 10:00','SYN_FLOOD',2,7700,10.8699999999999992,'2026-11-12 19:16:26');
DELETE FROM sqlite_sequence;
INSERT INTO sqlite_sequence VALUES('m3_raw_events',3);
COMMIT;

-- DBMS Unit 3: Relational Model, Constraints, Relational Algebra, SQL & Three-Schema Views (Anjali)
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS m2_protocols (
    proto_code TEXT PRIMARY KEY,
    layer TEXT NOT NULL CHECK(layer IN ('TRANSPORT', 'NETWORK', 'APPLICATION'))
);

CREATE TABLE IF NOT EXISTS m2_traffic_log (
    log_id INTEGER PRIMARY KEY AUTOINCREMENT,
    src_ip TEXT NOT NULL,
    dst_ip TEXT NOT NULL,
    proto_code TEXT NOT NULL REFERENCES m2_protocols(proto_code) ON DELETE RESTRICT,
    packet_bytes INTEGER NOT NULL CHECK(packet_bytes BETWEEN 20 AND 65535),
    threat_flag INTEGER DEFAULT 0 CHECK(threat_flag IN (0, 1))
);

-- Schema Evolution (ALTER TABLE)
-- (Safe idempotent seed data)
INSERT OR IGNORE INTO m2_protocols VALUES ('TCP', 'TRANSPORT'), ('UDP', 'TRANSPORT'), ('ICMP', 'NETWORK');

INSERT INTO m2_traffic_log (src_ip, dst_ip, proto_code, packet_bytes, threat_flag) VALUES
('192.168.1.45', '10.0.0.1', 'TCP', 1480, 1),
('192.168.1.45', '10.0.0.1', 'UDP', 512, 1),
('192.168.1.45', '10.0.0.1', 'ICMP', 64, 0),
('10.24.8.12',   '10.0.0.5', 'TCP', 900, 0),
('172.16.4.99',  '10.0.0.2', 'UDP', 1200, 1);

-- Three-Schema Architecture: External Level Role-Based Views
DROP VIEW IF EXISTS vw_viewer_summary;
CREATE VIEW vw_viewer_summary AS
SELECT proto_code, COUNT(*) AS total_packets, SUM(packet_bytes) AS total_bytes
FROM m2_traffic_log
GROUP BY proto_code;

DROP VIEW IF EXISTS vw_analyst_threats;
CREATE VIEW vw_analyst_threats AS
SELECT t.log_id, t.src_ip, t.dst_ip, p.layer, t.packet_bytes
FROM m2_traffic_log t
JOIN m2_protocols p ON t.proto_code = p.proto_code
WHERE t.threat_flag = 1;

-- Relational Algebra DIVISION Query: Find source IPs that sent traffic across ALL protocols in m2_protocols
SELECT DISTINCT t1.src_ip
FROM m2_traffic_log t1
WHERE NOT EXISTS (
    SELECT p.proto_code FROM m2_protocols p
    WHERE NOT EXISTS (
        SELECT 1 FROM m2_traffic_log t2
        WHERE t2.src_ip = t1.src_ip AND t2.proto_code = p.proto_code
    )
);

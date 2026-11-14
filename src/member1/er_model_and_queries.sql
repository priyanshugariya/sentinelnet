PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS er_ip_host (
    ip_address TEXT PRIMARY KEY,
    hostname TEXT,
    subnet_cidr TEXT NOT NULL,
    reputation_score INTEGER DEFAULT 100 CHECK(reputation_score BETWEEN 0 AND 100)
);

CREATE TABLE IF NOT EXISTS er_protocol (
    proto_id INTEGER PRIMARY KEY,
    proto_name TEXT UNIQUE NOT NULL,
    default_port INTEGER
);

CREATE TABLE IF NOT EXISTS er_security_analyst (
    analyst_id INTEGER PRIMARY KEY,
    full_name TEXT NOT NULL,
    clearance_level TEXT CHECK(clearance_level IN ('ADMIN', 'ANALYST', 'VIEWER'))
);

CREATE TABLE IF NOT EXISTS er_packet (
    packet_id INTEGER PRIMARY KEY AUTOINCREMENT,
    src_ip TEXT NOT NULL REFERENCES er_ip_host(ip_address),
    dst_ip TEXT NOT NULL REFERENCES er_ip_host(ip_address),
    proto_id INTEGER NOT NULL REFERENCES er_protocol(proto_id),
    payload_bytes INTEGER NOT NULL CHECK(payload_bytes > 0),
    captured_at TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS er_packet_fragment (
    packet_id INTEGER NOT NULL REFERENCES er_packet(packet_id) ON DELETE CASCADE,
    frag_offset INTEGER NOT NULL,
    more_fragments INTEGER NOT NULL CHECK(more_fragments IN (0, 1)),
    frag_bytes INTEGER NOT NULL,
    PRIMARY KEY (packet_id, frag_offset)
);

CREATE TABLE IF NOT EXISTS er_threat_event (
    threat_id INTEGER PRIMARY KEY AUTOINCREMENT,
    packet_id INTEGER NOT NULL REFERENCES er_packet(packet_id),
    severity TEXT NOT NULL CHECK(severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    threat_category TEXT NOT NULL CHECK(threat_category IN ('DDOS', 'SQLI'))
);

CREATE TABLE IF NOT EXISTS er_ddos_alert (
    threat_id INTEGER PRIMARY KEY REFERENCES er_threat_event(threat_id) ON DELETE CASCADE,
    syn_rate_pps INTEGER NOT NULL,
    botnet_signature TEXT
);

CREATE TABLE IF NOT EXISTS er_sqli_alert (
    threat_id INTEGER PRIMARY KEY REFERENCES er_threat_event(threat_id) ON DELETE CASCADE,
    matched_pattern TEXT NOT NULL,
    target_endpoint TEXT
);

CREATE TABLE IF NOT EXISTS er_mitigation_rule (
    rule_id INTEGER PRIMARY KEY,
    action_type TEXT NOT NULL CHECK(action_type IN ('DROP', 'QUARANTINE', 'RATE_LIMIT'))
);

CREATE TABLE IF NOT EXISTS er_incident_response_ternary (
    analyst_id INTEGER NOT NULL REFERENCES er_security_analyst(analyst_id),
    threat_id INTEGER NOT NULL REFERENCES er_threat_event(threat_id),
    rule_id INTEGER NOT NULL REFERENCES er_mitigation_rule(rule_id),
    enforced_at TEXT DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (analyst_id, threat_id, rule_id)
);

INSERT OR IGNORE INTO er_ip_host VALUES ('192.168.1.45', 'rogue-node-45', '192.168.1.0/24', 15);
INSERT OR IGNORE INTO er_ip_host VALUES ('10.0.0.1', 'core-gateway', '10.0.0.0/16', 99);
INSERT OR IGNORE INTO er_protocol VALUES (6, 'TCP', 80), (17, 'UDP', 53);
INSERT OR IGNORE INTO er_security_analyst VALUES (1, 'Priyanshu Gariya', 'ADMIN');
INSERT OR IGNORE INTO er_mitigation_rule VALUES (101, 'QUARANTINE');

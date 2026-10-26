-- ============================================================================
-- SENTINELNET DATABASE ARCHITECTURE (MEMBER 3: DBMS UNIT 2 & UNIT 3)
-- Relational Schema Decomposed into Boyce-Codd Normal Form (BCNF / 3NF)
-- Eliminates Redundancy, Insertion Anomalies, and Update Anomalies
-- ============================================================================

PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

-- ----------------------------------------------------------------------------
-- 1. PROTOCOL CATALOG (1NF, 2NF, 3NF, BCNF)
-- FD: protocol_id -> protocol_name, default_port, transport_type
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS ref_protocols (
    protocol_id INTEGER PRIMARY KEY,
    protocol_name TEXT NOT NULL UNIQUE,
    default_port INTEGER,
    transport_type TEXT CHECK(transport_type IN ('TCP', 'UDP', 'ICMP', 'RAW'))
);

INSERT OR IGNORE INTO ref_protocols (protocol_id, protocol_name, default_port, transport_type) VALUES
(1, 'ICMP', 0, 'ICMP'),
(6, 'TCP', 0, 'TCP'),
(17, 'UDP', 0, 'UDP'),
(80, 'HTTP', 80, 'TCP'),
(443, 'HTTPS', 443, 'TCP'),
(53, 'DNS', 53, 'UDP');

-- ----------------------------------------------------------------------------
-- 2. IP HOST REGISTRY (BCNF Normalization: Decouples Host Attributes from Ingestion)
-- FD: ip_address -> asn_number, country_code, risk_score, reputation_status
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS host_registry (
    ip_address TEXT PRIMARY KEY,
    asn_number INTEGER,
    country_code TEXT DEFAULT 'US',
    reputation_score REAL DEFAULT 0.0 CHECK(reputation_score BETWEEN 0.0 AND 100.0),
    is_internal BOOLEAN DEFAULT 0,
    first_seen_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_seen_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ----------------------------------------------------------------------------
-- 3. NETWORK INTERFACES (Decouples Physical Network Adapter Metadata)
-- FD: iface_id -> mac_address, mtu_size, link_speed_mbps
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS network_interfaces (
    iface_id TEXT PRIMARY KEY,
    mac_address TEXT NOT NULL UNIQUE,
    mtu_size INTEGER DEFAULT 1500,
    link_speed_mbps INTEGER DEFAULT 1000,
    promiscuous_mode BOOLEAN DEFAULT 1
);

INSERT OR IGNORE INTO network_interfaces (iface_id, mac_address, mtu_size, link_speed_mbps) VALUES
('eth0', '02:42:AC:11:00:02', 1500, 10000),
('wlan0', '3A:89:C4:21:EE:51', 1500, 1000);

-- ----------------------------------------------------------------------------
-- 4. NORMALIZED RAW PACKET LOG (Core High-Throughput Ledger)
-- Fully decomposed: No transitive dependencies. Pure BCNF.
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS packet_stream (
    packet_id INTEGER PRIMARY KEY AUTOINCREMENT,
    arrival_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    iface_id TEXT NOT NULL,
    src_ip TEXT NOT NULL,
    dst_ip TEXT NOT NULL,
    src_port INTEGER CHECK(src_port BETWEEN 0 AND 65535),
    dst_port INTEGER CHECK(dst_port BETWEEN 0 AND 65535),
    protocol_id INTEGER NOT NULL,
    payload_size INTEGER NOT NULL,
    checksum_valid BOOLEAN DEFAULT 1,
    FOREIGN KEY (iface_id) REFERENCES network_interfaces(iface_id) ON DELETE CASCADE,
    FOREIGN KEY (src_ip) REFERENCES host_registry(ip_address) ON DELETE CASCADE,
    FOREIGN KEY (dst_ip) REFERENCES host_registry(ip_address) ON DELETE CASCADE,
    FOREIGN KEY (protocol_id) REFERENCES ref_protocols(protocol_id) ON DELETE RESTRICT
);

-- ----------------------------------------------------------------------------
-- 5. THREAT SIGNATURE CATALOG (Decouples Rules from Alert Events)
-- FD: signature_id -> rule_name, cve_identifier, default_severity
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS threat_signatures (
    signature_id TEXT PRIMARY KEY,
    rule_name TEXT NOT NULL,
    cve_identifier TEXT,
    default_severity TEXT CHECK(default_severity IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL')),
    mitigation_action TEXT DEFAULT 'DROP'
);

INSERT OR IGNORE INTO threat_signatures (signature_id, rule_name, cve_identifier, default_severity, mitigation_action) VALUES
('SIG-1001', 'SYN Flood Volumetric Attack', 'CVE-2023-44487', 'HIGH', 'RATE_LIMIT'),
('SIG-1002', 'SQL Injection Inbound Payload', 'CVE-2022-21907', 'CRITICAL', 'BLOCK_IP'),
('SIG-1003', 'ICMP Tunneling Exfiltration', 'CVE-2021-34527', 'MEDIUM', 'LOG_AND_ALERT'),
('SIG-1004', 'Unauthorized Port Sweep', 'CVE-2020-0796', 'LOW', 'MONITOR');

-- ----------------------------------------------------------------------------
-- 6. SECURITY INCIDENT DISPATCH LOG (3NF Normalized Audit Trail)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS threat_incidents (
    incident_id INTEGER PRIMARY KEY AUTOINCREMENT,
    packet_id INTEGER NOT NULL,
    signature_id TEXT NOT NULL,
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    confidence_level REAL DEFAULT 0.95,
    action_taken TEXT NOT NULL,
    analyst_review_state TEXT DEFAULT 'UNREVIEWED',
    FOREIGN KEY (packet_id) REFERENCES packet_stream(packet_id) ON DELETE CASCADE,
    FOREIGN KEY (signature_id) REFERENCES threat_signatures(signature_id) ON DELETE RESTRICT
);

-- ----------------------------------------------------------------------------
-- PERFORMANCE ACCELERATION INDEXES (Query Optimization)
-- ----------------------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_packet_stream_src ON packet_stream(src_ip, arrival_timestamp);
CREATE INDEX IF NOT EXISTS idx_packet_stream_dst ON packet_stream(dst_ip, dst_port);
CREATE INDEX IF NOT EXISTS idx_incidents_sig ON threat_incidents(signature_id, detected_at);

-- ----------------------------------------------------------------------------
-- ANALYTICAL VIEW: DENORMALIZED TELEMETRY VIEW FOR DASHBOARD (P1-P4)
-- Provides zero-redundancy query acceleration for SOC operations
-- ----------------------------------------------------------------------------
CREATE VIEW IF NOT EXISTS v_active_threat_matrix AS
SELECT 
    i.incident_id,
    p.packet_id,
    p.arrival_timestamp,
    p.src_ip,
    p.dst_ip,
    s.rule_name,
    s.default_severity,
    i.action_taken,
    proto.protocol_name
FROM threat_incidents i
JOIN packet_stream p ON i.packet_id = p.packet_id
JOIN threat_signatures s ON i.signature_id = s.signature_id
JOIN ref_protocols proto ON p.protocol_id = proto.protocol_id;

-- ----------------------------------------------------------------------------
-- AUTOMATED ACTIVE DEFENSE TRIGGER (ACID Consistency & Instant Enforcement)
-- ----------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_auto_escalate_critical_threat
AFTER INSERT ON threat_incidents
WHEN NEW.signature_id IN ('SIG-1002')
BEGIN
    UPDATE host_registry
    SET reputation_score = 100.0,
        last_seen_timestamp = CURRENT_TIMESTAMP
    WHERE ip_address = (SELECT src_ip FROM packet_stream WHERE packet_id = NEW.packet_id);
END;

-- ============================================================================
-- SENTINELNET ACTIVE THREAT MITIGATION & AUDIT TRIGGERS (MEMBER 4: DBMS UNIT 4)
-- ACID Integrity Enforcement & Automated State-Machine Threat Containment
-- ============================================================================

PRAGMA foreign_keys = ON;

-- ----------------------------------------------------------------------------
-- 1. IP BLACKLIST TABLE (Enforcement State Machine)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS security_blacklist (
    blacklist_id INTEGER PRIMARY KEY AUTOINCREMENT,
    target_ip TEXT NOT NULL UNIQUE,
    reason TEXT NOT NULL,
    threat_tier TEXT CHECK(threat_tier IN ('LOW', 'ELEVATED', 'SEVERE', 'CRITICAL')),
    quarantine_start TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    quarantine_expiry TIMESTAMP,
    active_status BOOLEAN DEFAULT 1,
    admin_override BOOLEAN DEFAULT 0
);

-- ----------------------------------------------------------------------------
-- 2. DEADLOCK & RESOURCE AUDIT LOG (Correlates Banker Algorithm Events in DB)
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS resource_lock_events (
    event_id INTEGER PRIMARY KEY AUTOINCREMENT,
    process_tag TEXT NOT NULL,
    resource_type TEXT NOT NULL,
    requested_units INTEGER NOT NULL,
    decision TEXT CHECK(decision IN ('GRANTED', 'BLOCKED', 'PREEMPTED')),
    evaluation_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ----------------------------------------------------------------------------
-- 3. HISTORICAL ANOMALY COUNTERS
-- ----------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS threat_frequency_ledger (
    ip_address TEXT PRIMARY KEY,
    cumulative_hits INTEGER DEFAULT 1,
    last_hit_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_auto_quarantined BOOLEAN DEFAULT 0
);

-- ----------------------------------------------------------------------------
-- TRIGGER 1: Automated Quarantine on High-Frequency Malicious Activity
-- ----------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_detect_frequency_threshold
AFTER INSERT ON threat_incidents
BEGIN
    INSERT INTO threat_frequency_ledger (ip_address, cumulative_hits, last_hit_timestamp)
    VALUES (
        (SELECT src_ip FROM packet_stream WHERE packet_id = NEW.packet_id),
        1,
        CURRENT_TIMESTAMP
    )
    ON CONFLICT(ip_address) DO UPDATE SET
        cumulative_hits = cumulative_hits + 1,
        last_hit_timestamp = CURRENT_TIMESTAMP;

    -- Automatically escalate to blacklist if more than 3 attacks recorded
    INSERT OR REPLACE INTO security_blacklist (target_ip, reason, threat_tier, active_status)
    SELECT 
        src_ip, 
        'Threshold Exceeded (>3 Critical Triggers)', 
        'CRITICAL', 
        1
    FROM packet_stream
    WHERE packet_id = NEW.packet_id
      AND (SELECT cumulative_hits FROM threat_frequency_ledger WHERE ip_address = packet_stream.src_ip) >= 3;
END;

-- ----------------------------------------------------------------------------
-- TRIGGER 2: Prevent Blacklisted Hosts from Generating New Valid Logs
-- ----------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_block_quarantined_packets
BEFORE INSERT ON packet_stream
WHEN EXISTS (
    SELECT 1 FROM security_blacklist 
    WHERE target_ip = NEW.src_ip 
      AND active_status = 1 
      AND admin_override = 0
)
BEGIN
    SELECT RAISE(ABORT, 'REJECTED: Source IP address is currently quarantined in security_blacklist.');
END;

-- ----------------------------------------------------------------------------
-- TRIGGER 3: Audit Resource Allocation Grants & Revocations
-- ----------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_audit_lock_preemption
AFTER UPDATE OF action_taken ON threat_incidents
WHEN NEW.action_taken = 'PREEMPT_RESOURCES'
BEGIN
    INSERT INTO resource_lock_events (process_tag, resource_type, requested_units, decision)
    VALUES (
        'ThreatIncident-' || NEW.incident_id,
        'DB_ROW_EXCLUSIVE_LOCK',
        1,
        'PREEMPTED'
    );
END;

-- ----------------------------------------------------------------------------
-- SAMPLE SEED DATA & VERIFICATION QUERIES
-- ----------------------------------------------------------------------------
INSERT OR IGNORE INTO security_blacklist (target_ip, reason, threat_tier, active_status)
VALUES ('198.51.100.44', 'Manual Operator Ban - C2 Beacon', 'SEVERE', 1);

INSERT INTO resource_lock_events (process_tag, resource_type, requested_units, decision)
VALUES 
('Worker-P1', 'IPC_PIPE_HANDLE', 2, 'GRANTED'),
('Worker-P4', 'BUFFER_POOL_SECTOR', 3, 'BLOCKED');

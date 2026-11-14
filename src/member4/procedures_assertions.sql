-- DBMS Unit 3: Automated Threat Scoring Triggers & Integrity Assertions (Drishti)
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS m4_ip_reputation (
    ip_address TEXT PRIMARY KEY,
    threat_score INTEGER NOT NULL DEFAULT 0 CHECK(threat_score BETWEEN 0 AND 100),
    status TEXT NOT NULL DEFAULT 'CLEAN' CHECK(status IN ('CLEAN', 'WATCHLIST', 'QUARANTINED'))
);

CREATE TABLE IF NOT EXISTS m4_packet_stream (
    pkt_id INTEGER PRIMARY KEY AUTOINCREMENT,
    src_ip TEXT NOT NULL,
    dst_port INTEGER NOT NULL CHECK(dst_port BETWEEN 1 AND 65535),
    syn_flag INTEGER NOT NULL CHECK(syn_flag IN (0, 1)),
    payload_bytes INTEGER NOT NULL CHECK(payload_bytes > 0)
);

CREATE TABLE IF NOT EXISTS m4_quarantine_list (
    ip_address TEXT PRIMARY KEY,
    reason TEXT NOT NULL,
    quarantined_at TEXT DEFAULT CURRENT_TIMESTAMP
);

-- Assertion Trigger: Reject spoofed loopback traffic entering external interface
CREATE TRIGGER IF NOT EXISTS trg_assert_no_spoofed_loopback
BEFORE INSERT ON m4_packet_stream
WHEN NEW.src_ip LIKE '127.%'
BEGIN
    SELECT RAISE(ABORT, 'ASSERTION FAILED: Spoofed loopback source IP rejected');
END;

-- Stored-Procedure Equivalent Trigger: Compute Threat Score & Auto-Quarantine at >= 80
CREATE TRIGGER IF NOT EXISTS trg_auto_threat_score
AFTER INSERT ON m4_packet_stream
BEGIN
    INSERT OR IGNORE INTO m4_ip_reputation (ip_address, threat_score, status)
    VALUES (NEW.src_ip, 0, 'CLEAN');

    UPDATE m4_ip_reputation
    SET threat_score = MIN(100, threat_score + (CASE WHEN NEW.syn_flag = 1 THEN 35 ELSE 5 END) + (CASE WHEN NEW.payload_bytes > 1400 THEN 20 ELSE 0 END)),
        status = CASE
            WHEN MIN(100, threat_score + (CASE WHEN NEW.syn_flag = 1 THEN 35 ELSE 5 END) + (CASE WHEN NEW.payload_bytes > 1400 THEN 20 ELSE 0 END)) >= 80 THEN 'QUARANTINED'
            WHEN MIN(100, threat_score + (CASE WHEN NEW.syn_flag = 1 THEN 35 ELSE 5 END) + (CASE WHEN NEW.payload_bytes > 1400 THEN 20 ELSE 0 END)) >= 40 THEN 'WATCHLIST'
            ELSE 'CLEAN'
        END
    WHERE ip_address = NEW.src_ip;

    INSERT OR IGNORE INTO m4_quarantine_list (ip_address, reason)
    SELECT ip_address, 'Automated Trigger Escalation: Threat Score >= 80'
    FROM m4_ip_reputation
    WHERE ip_address = NEW.src_ip AND status = 'QUARANTINED';
END;

-- Seed 2 Heavy SYN Packets from 192.168.1.45 -> Triggers Auto-Quarantine (55 + 55 = 100)
INSERT INTO m4_packet_stream (src_ip, dst_port, syn_flag, payload_bytes) VALUES
('192.168.1.45', 80, 1, 1480),
('192.168.1.45', 80, 1, 1490),
('10.24.8.12',  443, 0, 512);

SELECT * FROM m4_ip_reputation;
SELECT * FROM m4_quarantine_list;

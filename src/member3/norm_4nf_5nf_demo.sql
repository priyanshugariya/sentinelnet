-- DBMS Unit 4: Complete Normalization Decomposition (1NF -> BCNF -> 4NF MVD -> 5NF JD) (Aanya)
PRAGMA foreign_keys = ON;

-- 1. BCNF Host Master Table (Eliminates Transitive Dependency: ip -> subnet -> gateway_region)
CREATE TABLE IF NOT EXISTS m3_subnet_region (
    subnet_cidr TEXT PRIMARY KEY,
    gateway_region TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS m3_host_bcnf (
    ip_address TEXT PRIMARY KEY,
    hostname TEXT NOT NULL,
    subnet_cidr TEXT NOT NULL REFERENCES m3_subnet_region(subnet_cidr) -- Inclusion Dependency
);

-- 2. 4NF Decomposition (Eliminates Multivalued Dependencies: IP ->> OpenPort | IP ->> ThreatTag)
-- Without 4NF, storing (IP, Port, Tag) creates Cartesian update anomalies.
CREATE TABLE IF NOT EXISTS m3_host_open_ports_4nf (
    ip_address TEXT NOT NULL REFERENCES m3_host_bcnf(ip_address),
    open_port INTEGER NOT NULL,
    PRIMARY KEY (ip_address, open_port)
);

CREATE TABLE IF NOT EXISTS m3_host_threat_tags_4nf (
    ip_address TEXT NOT NULL REFERENCES m3_host_bcnf(ip_address),
    threat_tag TEXT NOT NULL,
    PRIMARY KEY (ip_address, threat_tag)
);

-- 3. 5NF (Project-Join Normal Form) Decomposition (Eliminates Cyclic Join Dependency)
-- Decomposes (Analyst, AttackSignature, DefenseZone) into 3 lossless binary projections
CREATE TABLE IF NOT EXISTS m3_analyst_sig_5nf (
    analyst_name TEXT NOT NULL,
    attack_sig TEXT NOT NULL,
    PRIMARY KEY (analyst_name, attack_sig)
);

CREATE TABLE IF NOT EXISTS m3_sig_zone_5nf (
    attack_sig TEXT NOT NULL,
    defense_zone TEXT NOT NULL,
    PRIMARY KEY (attack_sig, defense_zone)
);

CREATE TABLE IF NOT EXISTS m3_analyst_zone_5nf (
    analyst_name TEXT NOT NULL,
    defense_zone TEXT NOT NULL,
    PRIMARY KEY (analyst_name, defense_zone)
);

-- Seed Normalized Data
INSERT OR IGNORE INTO m3_subnet_region VALUES ('192.168.1.0/24', 'DMZ-NORTH');
INSERT OR IGNORE INTO m3_host_bcnf VALUES ('192.168.1.45', 'rogue-host-45', '192.168.1.0/24');

INSERT OR IGNORE INTO m3_host_open_ports_4nf VALUES ('192.168.1.45', 22), ('192.168.1.45', 80), ('192.168.1.45', 443);
INSERT OR IGNORE INTO m3_host_threat_tags_4nf VALUES ('192.168.1.45', 'SYN_FLOOD'), ('192.168.1.45', 'PORT_SCANNER');

INSERT OR IGNORE INTO m3_analyst_sig_5nf VALUES ('Aanya', 'SQLI_UNION'), ('Aanya', 'SYN_FLOOD');
INSERT OR IGNORE INTO m3_sig_zone_5nf VALUES ('SQLI_UNION', 'WEB_TIER'), ('SYN_FLOOD', 'EDGE_ROUTER');
INSERT OR IGNORE INTO m3_analyst_zone_5nf VALUES ('Aanya', 'WEB_TIER'), ('Aanya', 'EDGE_ROUTER');

-- Lossless 5NF Natural Join Verification Query
SELECT a.analyst_name, a.attack_sig, b.defense_zone
FROM m3_analyst_sig_5nf a
JOIN m3_sig_zone_5nf b ON a.attack_sig = b.attack_sig
JOIN m3_analyst_zone_5nf c ON a.analyst_name = c.analyst_name AND b.defense_zone = c.defense_zone;

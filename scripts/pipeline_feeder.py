import sqlite3
import random
import time

DB_PATH = "data/sentinel_stress.db"

# Sample network data pools
SAMPLE_IPS = [
    "192.168.1.10", "192.168.1.15", "10.0.0.5", 
    "172.16.0.4", "198.51.100.12", "203.0.113.45"
]
MALICIOUS_IPS = ["198.51.100.12", "203.0.113.45"]
PROTOCOLS = [1, 6, 17]  # ICMP, TCP, UDP
PORTS = [80, 443, 22, 53, 8080, 3306]

def init_db_seeds(conn):
    """Ensure baseline interface and protocol lookups exist."""
    cur = conn.cursor()
    cur.execute("INSERT OR IGNORE INTO network_interfaces (iface_id, mac_address) VALUES ('eth0', '02:42:AC:11:00:02');")
    
    # Common protocols
    protocols = [
        (1, 'ICMP'),
        (6, 'TCP'),
        (17, 'UDP')
    ]
    cur.executemany("INSERT OR IGNORE INTO protocols (protocol_id, protocol_name) VALUES (?, ?);", protocols)
    conn.commit()

def generate_live_burst(conn, count=60):
    """Generates and inserts a batch of packet frames into the pipeline."""
    cur = conn.cursor()
    inserted_count = 0
    quarantined_count = 0

    for _ in range(count):
        src = random.choice(SAMPLE_IPS)
        dst = "192.168.1.1"
        src_port = random.choice(PORTS)
        dst_port = random.choice(PORTS)
        proto = random.choice(PROTOCOLS)
        payload_sz = random.randint(64, 1500)

        try:
            cur.execute("""
                INSERT INTO packet_stream (iface_id, src_ip, dst_ip, src_port, dst_port, protocol_id, payload_size)
                VALUES ('eth0', ?, ?, ?, ?, ?, ?);
            """, (src, dst, src_port, dst_port, proto, payload_sz))
            inserted_count += 1
        except sqlite3.IntegrityError:
            # Trigger caught an incoming packet from a blacklisted or offending IP
            quarantined_count += 1

    conn.commit()
    print(f"[+] Successfully ingested: {inserted_count} packets | Quarantined/Dropped: {quarantined_count} packets")

def main():
    print("[*] Ingesting packet burst into SQLite pipeline...")
    conn = sqlite3.connect(DB_PATH)
    init_db_seeds(conn)
    generate_live_burst(conn, count=60)
    conn.close()

if __name__ == "__main__":
    main()

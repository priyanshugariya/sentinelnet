#!/usr/bin/env python3
"""DBMS Unit 1: Traditional File System vs. RDBMS Benchmark & Three-Schema Architecture."""
import csv
import json
import os
import sqlite3
import sys
import time

FLAT_FILE = "data/flat_packets.csv"
DB_FILE = "data/unit1_rdbms.db"
NUM_RECORDS = 2000


def run_benchmark():
    os.makedirs("data", exist_ok=True)
    if os.path.exists(FLAT_FILE):
        os.remove(FLAT_FILE)
    if os.path.exists(DB_FILE):
        os.remove(DB_FILE)

    # 1. Flat File Write (Repeats full hostname/subnet strings on every single packet row -> Redundancy)
    t0 = time.perf_counter()
    with open(FLAT_FILE, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["pkt_id", "src_ip", "hostname", "subnet", "proto", "bytes"])
        for i in range(1, NUM_RECORDS + 1):
            ip = f"192.168.1.{(i % 50) + 1}"
            writer.writerow([i, ip, f"endpoint-node-{(i % 50) + 1}.sentinel.local", "192.168.1.0/24", "TCP", 512 + (i % 900)])
    flat_write_ms = (time.perf_counter() - t0) * 1000
    flat_size_kb = os.path.getsize(FLAT_FILE) / 1024.0

    # 2. Flat File Linear Scan Query (O(N) sequential read)
    t0 = time.perf_counter()
    flat_matches = 0
    with open(FLAT_FILE, "r") as f:
        for row in csv.DictReader(f):
            if row["src_ip"] == "192.168.1.25":
                flat_matches += 1
    flat_query_ms = (time.perf_counter() - t0) * 1000

    # 3. RDBMS Normalized Write (Physical & Logical Data Independence + Zero Hostname Redundancy)
    t0 = time.perf_counter()
    conn = sqlite3.connect(DB_FILE)
    cur = conn.cursor()
    cur.execute("PRAGMA journal_mode=WAL;")
    cur.execute("CREATE TABLE hosts (ip TEXT PRIMARY KEY, hostname TEXT, subnet TEXT);")
    cur.execute("CREATE TABLE packets (pkt_id INTEGER PRIMARY KEY, src_ip TEXT REFERENCES hosts(ip), proto TEXT, bytes INT);")
    cur.execute("CREATE INDEX idx_pkt_src ON packets(src_ip);")

    hosts = [(f"192.168.1.{h}", f"endpoint-node-{h}.sentinel.local", "192.168.1.0/24") for h in range(1, 51)]
    cur.executemany("INSERT INTO hosts VALUES (?, ?, ?);", hosts)

    pkts = [(i, f"192.168.1.{(i % 50) + 1}", "TCP", 512 + (i % 900)) for i in range(1, NUM_RECORDS + 1)]
    cur.executemany("INSERT INTO packets VALUES (?, ?, ?, ?);", pkts)
    conn.commit()
    db_write_ms = (time.perf_counter() - t0) * 1000
    db_size_kb = os.path.getsize(DB_FILE) / 1024.0

    # 4. RDBMS Indexed Lookup (O(log N) B-Tree Index Seek)
    t0 = time.perf_counter()
    cur.execute("SELECT COUNT(*) FROM packets WHERE src_ip = ?;", ("192.168.1.25",))
    db_matches = cur.fetchone()[0]
    db_query_ms = (time.perf_counter() - t0) * 1000
    conn.close()

    return {
        "records": NUM_RECORDS,
        "flat_file": {
            "storage_kb": round(flat_size_kb, 2),
            "write_ms": round(flat_write_ms, 2),
            "query_ms": round(flat_query_ms, 3),
            "matches": flat_matches,
            "anomalies": "High data redundancy; no concurrent write locks; O(N) full scan"
        },
        "rdbms": {
            "storage_kb": round(db_size_kb, 2),
            "write_ms": round(db_write_ms, 2),
            "query_ms": round(db_query_ms, 3),
            "matches": db_matches,
            "advantages": "Normalized hosts table; B-Tree index; Three-Schema External Views"
        },
        "query_speedup_x": round(flat_query_ms / max(db_query_ms, 0.001), 1)
    }


if __name__ == "__main__":
    res = run_benchmark()
    if "--json" in sys.argv:
        print(json.dumps(res, indent=2))
    else:
        print("=== DBMS Unit 1: Flat File System vs. RDBMS Benchmark (Anjali) ===")
        print(f"Flat File ({res['records']} rows) -> Size: {res['flat_file']['storage_kb']} KB | Query: {res['flat_file']['query_ms']} ms")
        print(f"RDBMS     ({res['records']} rows) -> Size: {res['rdbms']['storage_kb']} KB | Query: {res['rdbms']['query_ms']} ms ({res['query_speedup_x']}x faster)")

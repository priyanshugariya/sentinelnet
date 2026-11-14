#!/usr/bin/env python3
"""DBMS Unit 3: B-Tree/Composite Index EXPLAIN QUERY PLAN Benchmark & NoSQL Document Store (Aanya)."""
import json
import os
import sqlite3
import sys
import time

DB_PATH = "data/unit3_index_bench.db"
NOSQL_PATH = "data/threat_intel_nosql.json"


def run_benchmark():
    os.makedirs("data", exist_ok=True)
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)

    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute("""
        CREATE TABLE perf_packets (
            id INTEGER PRIMARY KEY,
            src_ip TEXT,
            proto TEXT,
            severity TEXT,
            bytes INT
        );
    """)

    rows = [
        (i, f"10.0.{i % 40}.{i % 250}", "TCP" if i % 2 == 0 else "UDP",
         "CRITICAL" if i % 25 == 0 else "LOW", 128 + (i % 1300))
        for i in range(1, 5001)
    ]
    cur.executemany("INSERT INTO perf_packets VALUES (?, ?, ?, ?, ?);", rows)
    conn.commit()

    query = "SELECT COUNT(*), SUM(bytes) FROM perf_packets WHERE src_ip = '10.0.12.112' AND severity = 'CRITICAL';"

    # 1. Execution Plan & Latency BEFORE Indexes (Full Table Scan)
    cur.execute(f"EXPLAIN QUERY PLAN {query}")
    plan_before = " | ".join(str(r[3]) for r in cur.fetchall())
    t0 = time.perf_counter()
    for _ in range(50):
        cur.execute(query).fetchone()
    ms_before = ((time.perf_counter() - t0) / 50.0) * 1000

    # 2. Create Composite B-Tree Index (src_ip, severity)
    cur.execute("CREATE INDEX idx_src_sev_composite ON perf_packets(src_ip, severity);")
    conn.commit()

    cur.execute(f"EXPLAIN QUERY PLAN {query}")
    plan_after = " | ".join(str(r[3]) for r in cur.fetchall())
    t0 = time.perf_counter()
    for _ in range(50):
        cur.execute(query).fetchone()
    ms_after = ((time.perf_counter() - t0) / 50.0) * 1000
    conn.close()

    # 3. NoSQL Key-Value / JSON Document Store Demonstration (Schema-less Threat Feed)
    nosql_docs = {
        "ip:192.168.1.45": {
            "type": "DocumentStore",
            "asn": "AS64512",
            "tags": ["syn-flood", "tor-exit"],
            "nested_geo": {"country": "IN", "city": "Dehradun"},
            "cap_profile": "AP (Available + Partition Tolerant, Eventual Consistency)"
        },
        "ip:172.16.4.99": {
            "type": "KeyValueStore",
            "reputation": 12,
            "ttl_seconds": 3600
        }
    }
    with open(NOSQL_PATH, "w") as nf:
        json.dump(nosql_docs, nf, indent=2)

    t0 = time.perf_counter()
    with open(NOSQL_PATH, "r") as nf:
        loaded = json.load(nf)
    doc = loaded.get("ip:192.168.1.45")
    nosql_lookup_ms = (time.perf_counter() - t0) * 1000

    return {
        "rows_tested": 5000,
        "before_index": {"plan": plan_before, "avg_query_ms": round(ms_before, 4)},
        "after_composite_index": {"plan": plan_after, "avg_query_ms": round(ms_after, 4)},
        "index_speedup_x": round(ms_before / max(ms_after, 0.0001), 1),
        "nosql_demo": {
            "store_type": "JSON Document & Key-Value Store",
            "sample_key": "ip:192.168.1.45",
            "tags": doc["tags"],
            "lookup_ms": round(nosql_lookup_ms, 4),
            "cap_theorem_note": doc["cap_profile"]
        }
    }


if __name__ == "__main__":
    out = run_benchmark()
    if "--json" in sys.argv:
        print(json.dumps(out, indent=2))
    else:
        print("=== DBMS Unit 3: Index EXPLAIN Benchmark & NoSQL Store (Aanya) ===")
        print(f"Before Index : {out['before_index']['avg_query_ms']} ms [{out['before_index']['plan']}]")
        print(f"After Index  : {out['after_composite_index']['avg_query_ms']} ms [{out['after_composite_index']['plan']}] ({out['index_speedup_x']}x faster)")
        print(f"NoSQL Lookup : Key '{out['nosql_demo']['sample_key']}' -> Tags {out['nosql_demo']['tags']} ({out['nosql_demo']['cap_theorem_note']})")

#!/usr/bin/env python3
"""DBMS Unit 3: Relational Algebra, Constraint Violation Tests & Heuristic Query Cost Optimizer."""
import json
import sqlite3
import sys

DB_PATH = "data/unit3_anjali.db"


def run_demo():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    with open("src/member2/relational_algebra_queries.sql", "r") as f:
        cur.executescript(f.read())

    # 1. Deliberate Constraint Violation Tests (PK, FK, CHECK)
    violations = []
    for label, bad_sql in [
        ("PRIMARY KEY Violation", "INSERT INTO m2_protocols VALUES ('TCP', 'TRANSPORT');"),
        ("FOREIGN KEY Violation", "INSERT INTO m2_traffic_log (src_ip, dst_ip, proto_code, packet_bytes) VALUES ('1.1.1.1','2.2.2.2','FAKE_PROTO',100);"),
        ("CHECK Constraint Violation", "INSERT INTO m2_traffic_log (src_ip, dst_ip, proto_code, packet_bytes) VALUES ('1.1.1.1','2.2.2.2','TCP',-50);"),
    ]:
        try:
            cur.execute(bad_sql)
        except sqlite3.IntegrityError as exc:
            violations.append({"constraint": label, "blocked": True, "error": str(exc)})

    # 2. Relational Division Result (IPs using every protocol)
    cur.execute("""
        SELECT DISTINCT t1.src_ip FROM m2_traffic_log t1
        WHERE NOT EXISTS (
            SELECT p.proto_code FROM m2_protocols p
            WHERE NOT EXISTS (
                SELECT 1 FROM m2_traffic_log t2
                WHERE t2.src_ip = t1.src_ip AND t2.proto_code = p.proto_code
            )
        );
    """)
    division_ips = [r[0] for r in cur.fetchall()]

    # 3. Correlated Subquery + GROUP BY HAVING
    cur.execute("""
        SELECT src_ip, COUNT(*) AS pkt_count, AVG(packet_bytes) AS avg_bytes
        FROM m2_traffic_log t1
        WHERE packet_bytes >= (SELECT AVG(packet_bytes) FROM m2_traffic_log t2 WHERE t2.proto_code = t1.proto_code)
        GROUP BY src_ip
        HAVING COUNT(*) >= 1;
    """)
    heavy_talkers = [{"src_ip": r[0], "packets": r[1], "avg_bytes": round(r[2], 1)} for r in cur.fetchall()]

    # 4. Heuristic Query Optimization (Cartesian Join -> Filter vs. Predicate Pushdown)
    cur.execute("SELECT COUNT(*) FROM m2_traffic_log;")
    n_logs = cur.fetchone()[0]
    cur.execute("SELECT COUNT(*) FROM m2_protocols;")
    n_protos = cur.fetchone()[0]
    naive_tuples_evaluated = n_logs * n_protos  # Cross product before selection

    cur.execute("SELECT COUNT(*) FROM m2_traffic_log WHERE threat_flag = 1;")
    filtered_logs = cur.fetchone()[0]
    optimized_tuples_evaluated = filtered_logs  # Selection pushed down before join

    conn.close()
    return {
        "constraint_violations_tested": violations,
        "relational_division_all_protocols_ips": division_ips,
        "correlated_having_heavy_talkers": heavy_talkers,
        "heuristic_optimizer": {
            "unoptimized_plan": "PI(src_ip) (SIGMA(threat_flag=1) (Traffic_Log X Protocols))",
            "optimized_plan": "PI(src_ip) ((SIGMA(threat_flag=1)(Traffic_Log)) JOIN Protocols)",
            "naive_intermediate_tuples": naive_tuples_evaluated,
            "optimized_intermediate_tuples": optimized_tuples_evaluated,
            "reduction_pct": round(100.0 * (1 - optimized_tuples_evaluated / max(naive_tuples_evaluated, 1)), 1)
        }
    }


if __name__ == "__main__":
    out = run_demo()
    if "--json" in sys.argv:
        print(json.dumps(out, indent=2))
    else:
        print("=== DBMS Unit 3: Relational Algebra, Constraints & Cost Optimizer (Anjali) ===")
        for v in out["constraint_violations_tested"]:
            print(f"  [BLOCKED] {v['constraint']}: {v['error']}")
        print("Relational Division (IPs across ALL protocols):", out["relational_division_all_protocols_ips"])
        print(f"Predicate Pushdown Optimization: {out['heuristic_optimizer']['naive_intermediate_tuples']} -> {out['heuristic_optimizer']['optimized_intermediate_tuples']} tuples ({out['heuristic_optimizer']['reduction_pct']}% reduction)")

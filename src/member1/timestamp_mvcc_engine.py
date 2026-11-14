#!/usr/bin/env python3
import json
import sys


def analyze_2pl_and_recoverability(schedule):
    phase = {}
    locks_held = {}
    uncommitted_writes = {}
    dirty_reads = []
    commit_order = []
    two_pl_violations = []

    for tx, op, item in schedule:
        phase.setdefault(tx, "GROWING")
        locks_held.setdefault(tx, set())

        if op in ("R", "W"):
            if phase[tx] == "SHRINKING":
                two_pl_violations.append(f"{tx} acquired lock on {item} after shrinking phase")
            locks_held[tx].add(item)

            if op == "R" and item in uncommitted_writes and uncommitted_writes[item] != tx:
                dirty_reads.append((tx, uncommitted_writes[item], item))
            if op == "W":
                uncommitted_writes[item] = tx

        elif op == "U":
            phase[tx] = "SHRINKING"
            locks_held[tx].discard(item)

        elif op == "C":
            phase[tx] = "SHRINKING"
            locks_held[tx].clear()
            commit_order.append(tx)
            for k in [k for k, v in uncommitted_writes.items() if v == tx]:
                del uncommitted_writes[k]

    recoverability = "Strict (Cascadeless & Recoverable)"
    if dirty_reads:
        recoverability = "Cascadeless Violated (Dirty Read Detected)"
        for reader, writer, _ in dirty_reads:
            if reader in commit_order and writer in commit_order:
                if commit_order.index(reader) < commit_order.index(writer):
                    recoverability = "Non-Recoverable Schedule"
                    break
            elif reader in commit_order and writer not in commit_order:
                recoverability = "Non-Recoverable Schedule"
                break
        else:
            if recoverability != "Non-Recoverable Schedule":
                recoverability = "Recoverable (but NOT Cascadeless)"

    return {
        "two_pl_valid": len(two_pl_violations) == 0,
        "two_pl_violations": two_pl_violations,
        "dirty_reads": dirty_reads,
        "recoverability_class": recoverability,
        "commit_order": commit_order,
    }


def simulate_mvcc_thomas_write():
    versions = [{"version": 1, "ts": 10, "value": "IP_STATUS=CLEAN"}]
    events = []

    snap = [v for v in versions if v["ts"] <= 15][-1]
    events.append(f"T1 (TS=15) reads {snap['value']} (v{snap['version']}) without blocking writers")

    versions.append({"version": 2, "ts": 25, "value": "IP_STATUS=QUARANTINED"})
    events.append("T2 (TS=25) creates v2: IP_STATUS=QUARANTINED")

    latest_ts = versions[-1]["ts"]
    if 18 < latest_ts:
        events.append(f"T3 (TS=18) write ignored via Thomas' Write Rule (18 < W-TS {latest_ts})")

    return {"versions": versions, "events": events}


if __name__ == "__main__":
    sample_schedule = [
        ("T1", "W", "IP_192_168_1_45"),
        ("T2", "R", "IP_192_168_1_45"),
        ("T1", "C", None),
        ("T2", "C", None),
    ]
    res = {
        "schedule_analysis": analyze_2pl_and_recoverability(sample_schedule),
        "mvcc_thomas_write": simulate_mvcc_thomas_write(),
    }
    if "--json" in sys.argv:
        print(json.dumps(res, indent=2))
    else:
        print("=== DBMS Unit 5: 2PL, Recoverability & MVCC Engine ===")
        print("Schedule Classification:", res["schedule_analysis"]["recoverability_class"])
        print("2PL Compliant:", res["schedule_analysis"]["two_pl_valid"])
        for ev in res["mvcc_thomas_write"]["events"]:
            print(" -", ev)

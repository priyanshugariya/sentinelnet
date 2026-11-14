#!/usr/bin/env python3
"""DBMS Unit 5: Write-Ahead Logging (WAL), Checkpoints & ARIES Undo/Redo Crash Recovery (Drishti)."""
import json
import sys


def simulate_aries_recovery():
    # Initial stable disk state before crash
    disk_state = {"IP_192_168_1_45": "CLEAN", "IP_172_16_4_99": "CLEAN", "ALERT_QUOTA": 100}

    # Write-Ahead Log records (LSN, TX, Type, Key, BeforeImage, AfterImage)
    wal_records = [
        {"lsn": 1, "tx": "T1", "type": "UPDATE", "key": "IP_192_168_1_45", "before": "CLEAN", "after": "QUARANTINED"},
        {"lsn": 2, "tx": "T1", "type": "COMMIT"},
        {"lsn": 3, "type": "CHECKPOINT", "active_txs": ["T2"]},
        {"lsn": 4, "tx": "T2", "type": "UPDATE", "key": "IP_172_16_4_99", "before": "CLEAN", "after": "BLOCKED"},
        {"lsn": 5, "tx": "T2", "type": "UPDATE", "key": "ALERT_QUOTA", "before": 100, "after": 0},
        # POWER LOSS / KERNEL PANIC OCCURS HERE BEFORE T2 COMMITS!
    ]

    # Simulate dirty buffer flush before crash (uncommitted T2 leaked onto disk, T1 was still in buffer)
    corrupted_disk = {"IP_192_168_1_45": "CLEAN", "IP_172_16_4_99": "BLOCKED", "ALERT_QUOTA": 0}

    # Phase 1: Analysis
    committed = {r["tx"] for r in wal_records if r.get("type") == "COMMIT"}
    all_txs = {r["tx"] for r in wal_records if "tx" in r}
    uncommitted = sorted(all_txs - committed)

    recovered_disk = dict(corrupted_disk)
    redo_actions = []
    undo_actions = []

    # Phase 2: REDO (Forward pass for committed transactions)
    for r in wal_records:
        if r.get("type") == "UPDATE" and r["tx"] in committed:
            recovered_disk[r["key"]] = r["after"]
            redo_actions.append(f"LSN {r['lsn']} ({r['tx']}): REDO {r['key']} -> {r['after']}")

    # Phase 3: UNDO (Backward pass for uncommitted loser transactions)
    for r in reversed(wal_records):
        if r.get("type") == "UPDATE" and r["tx"] in uncommitted:
            recovered_disk[r["key"]] = r["before"]
            undo_actions.append(f"LSN {r['lsn']} ({r['tx']}): UNDO {r['key']} -> {r['before']}")

    return {
        "crash_event": "SIMULATED_POWER_LOSS_AT_LSN_6",
        "committed_txs": sorted(committed),
        "loser_uncommitted_txs": uncommitted,
        "corrupted_disk_at_boot": corrupted_disk,
        "redo_log": redo_actions,
        "undo_log": undo_actions,
        "recovered_consistent_disk": recovered_disk
    }


if __name__ == "__main__":
    out = simulate_aries_recovery()
    if "--json" in sys.argv:
        print(json.dumps(out, indent=2))
    else:
        print("=== DBMS Unit 5: WAL Checkpoints & ARIES Undo/Redo Crash Recovery (Drishti) ===")
        print("Corrupted Disk at Boot :", out["corrupted_disk_at_boot"])
        for a in out["redo_log"]:
            print("  [REDO]", a)
        for a in out["undo_log"]:
            print("  [UNDO]", a)
        print("Recovered Disk State   :", out["recovered_consistent_disk"])

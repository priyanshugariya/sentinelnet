#!/usr/bin/env bash
# OS Unit 1 & 5: SentinelNet Master CLI Verification Controller (Drishti)
set -euo pipefail

echo "=================================================================="
echo "   SENTINELNET PHASE 3: FULL 4-MEMBER SYSTEM VERIFICATION"
echo "=================================================================="

echo ""
echo "[1/4] Member 1 (Priyanshu) -> Syscalls, Sync Bench, Paging/TLB, Embedded SQL, MVCC"
./bin_syscall_profiler
./bin_sync_bench
./bin_paging_seg
./bin_embedded_sql
python3 src/member1/timestamp_mvcc_engine.py

echo ""
echo "[2/4] Member 2 (Anjali) -> Process/IPC, 1:1 Threads, MLFQ, FlatFile vs DB, Query Optimizer"
./bin_process_supervisor
./bin_thread_models
./bin_mlfq_scheduler
python3 src/member2/flatfile_vs_db_bench.py
python3 src/member2/query_cost_optimizer.py

echo ""
echo "[3/4] Member 3 (Aanya) -> Working Set/Belady, Log FS/Access Matrix, 4NF/5NF, Index/NoSQL, Media Recovery"
./bin_working_set
./bin_log_fs
python3 src/member3/index_nosql_bench.py
./src/member3/media_recovery.sh

echo ""
echo "[4/4] Member 4 (Drishti) -> Banker's/WFG Deadlock, RAID-5/Disk, Block Alloc, ACID/SAVEPOINT, ARIES Crash"
./bin_deadlock_engine
./bin_raid_storage
./bin_block_allocator
python3 src/member4/tx_locking_demo.py
python3 src/member4/crash_recovery_sim.py

echo ""
echo "=================================================================="
echo "   ALL 16 PHASE 3 OS & DBMS MODULES VERIFIED SUCCESSFULLY!"
echo "=================================================================="

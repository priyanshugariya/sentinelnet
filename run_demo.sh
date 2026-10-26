#!/bin/bash

# Move to project root
cd ~/sentinel_workspace/sentinelnet

# 1. Compile C binaries beforehand
echo "[*] Compiling Member 1 modules..."
gcc -Wall -O2 src/member1/mem_allocator.c -o src/member1/mem_allocator
gcc -Wall -O2 src/member1/fragmentation_stress.c -o src/member1/fragmentation_stress
gcc -Wall -O2 src/member1/producer_ringbuffer.c -o src/member1/producer_ringbuffer -lpthread

echo "[*] Launching terminals for demonstration..."

# Terminal 1: Member 1 (Memory & Ring Buffer)
gnome-terminal --title="MEMBER 1: Memory & Synchronization" -- bash -c "
cd ~/sentinel_workspace/sentinelnet/src/member1;
echo '=== MEMBER 1 TERMINAL ===';
echo '1) ./mem_allocator';
echo '2) ./fragmentation_stress';
echo '3) ./producer_ringbuffer';
echo '4) python3 db_concurrency_stress.py';
echo '';
exec bash"

# Terminal 2: Member 2 (Pipeline Ingestion & Scheduler)
gnome-terminal --title="MEMBER 2: Pipeline Feeder & CPU Dispatch" -- bash -c "
cd ~/sentinel_workspace/sentinelnet;
echo '=== MEMBER 2 TERMINAL ===';
echo 'Run: python3 scripts/pipeline_feeder.py';
echo '';
exec bash"

# Terminal 3: Member 3 (Virtual Memory & BCNF Schema)
gnome-terminal --title="MEMBER 3: Virtual Memory & BCNF DB" -- bash -c "
cd ~/sentinel_workspace/sentinelnet;
echo '=== MEMBER 3 TERMINAL ===';
echo 'Run: python3 -c \"import sqlite3; c=sqlite3.connect(\\\"data/sentinel_stress.db\\\"); print(c.execute(\\\"SELECT name FROM sqlite_master WHERE type=\'table\';\\\").fetchall())\"';
echo '';
exec bash"

# Terminal 4: Member 4 (Storage & Security Triggers)
gnome-terminal --title="MEMBER 4: Disk Seek & Security Triggers" -- bash -c "
cd ~/sentinel_workspace/sentinelnet;
echo '=== MEMBER 4 TERMINAL ===';
echo 'Run: python3 -c \"import sqlite3; c=sqlite3.connect(\\\"data/sentinel_stress.db\\\"); print(\'Blacklisted IPs:\', c.execute(\\\"SELECT * FROM security_blacklist;\\\").fetchall())\"';
echo '';
exec bash"

# Terminal 5: Flask Web Server (Running in background)
fuser -k 5000/tcp 2>/dev/null
python3 src/dashboard/app.py

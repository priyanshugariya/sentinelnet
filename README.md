# SentinelNet: Real-Time Network Threat Telemetry & Resource Engine

SentinelNet is a high-performance network threat analysis and telemetry framework built on top of OS-level resource management primitives and a relational storage backend. The system integrates low-level C components for memory allocation, CPU scheduling, and deadlock avoidance with a Python/SQLite pipeline and web dashboard.

---

## Key Features

- **Custom Memory Management:** Dynamic free-list memory allocator with internal/external fragmentation tracking and POSIX multithreaded ring-buffer ingestion.
- **CPU & Disk Scheduling:** Simulation engines supporting FCFS, SJF, Round Robin, LOOK, and SCAN algorithms for packet ingestion prioritization.
- **Deadlock Management:** Resource Allocation Graph (RAG) cycle detection and Dijkstra's Banker's Algorithm verification.
- **BCNF Relational Engine:** SQLite schema normalized to Boyce-Codd Normal Form with database triggers for automatic threat quarantine and audit logging.
- **Live Telemetry Dashboard:** Flask and JavaScript monitoring interface to track packet flow, memory usage, and threat metrics.

---

## Repository Structure

```text
sentinelnet/
├── dashboard/                  # Telemetry dashboard server and UI
├── include/                    # Header files (packet structures, interfaces)
├── scripts/                    # Network traffic generators and feeder scripts
├── src/
│   ├── bridge/                 # IPC to SQLite streaming bridge
│   ├── member1/                # Memory allocation & concurrency stress tests
│   ├── member2/                # CPU scheduler simulations & worker parser
│   ├── member3/                # BCNF schema, page replacement, and mmap tracking
│   └── member4/                # Banker's algorithm, RAG analyzer, and triggers
├── Makefile                    # Automated build script
└── run_demo.sh                 # End-to-end execution script

<div align="center">

# 🛡️ SENTINELNET
**Real-Time Network Threat Telemetry & OS/DBMS Resource Engine**

![Phase](https://img.shields.io/badge/Release-Phase%203%20Final-00E5FF?style=flat-square)
![C](https://img.shields.io/badge/Kernel%20%26%20Memory-C%20%2F%20POSIX-00599C?style=flat-square&logo=c&logoColor=white)
![Python](https://img.shields.io/badge/Backend%20%26%20Engines-Python%203-3776AB?style=flat-square&logo=python&logoColor=white)
![SQLite](https://img.shields.io/badge/Storage-SQLite3%20%2F%204NF--5NF-003B57?style=flat-square&logo=sqlite&logoColor=white)
![Three.js](https://img.shields.io/badge/UI-Three.js%20WebGL-000000?style=flat-square&logo=threedotjs&logoColor=white)

*Bridging low-level Operating System kernel primitives with ACID-compliant DBMS storage inside a 3D Security Operations Center.*

</div>

---

## ⚡ System Architecture

```text
  [ Incoming Traffic ] ──► [ Syscall Audit & Ring Buffer ] ──► [ MLFQ Scheduler & Memory ]
                                                                          │
  [ 3D WebGL SOC HUD ] ◄── [ Flask Telemetry REST API ]    ◄── [ 4NF/5NF DB, MVCC & WAL ]
```

**SentinelNet** simulates an end-to-end threat-quarantine pipeline where kernel-level packet ingestion, custom memory allocation, thread synchronization, and transactional database concurrency work together in real time.

---

## 🔥 Core Capabilities

| Layer | Engineering Focus | Key Implementations |
| :--- | :--- | :--- |
| **Kernel & I/O** | Syscall tracing & lock-free ingestion | `strace` profiling, POSIX circular ring buffer, `mutex` vs. `spinlock` latency benchmarks |
| **Memory & CPU** | Virtual addressing & thread scheduling | Free-list allocator, fragmentation stress tests, paging/segmentation, MLFQ real-time scheduler |
| **Fault Tolerance** | Deadlocks & storage reliability | Resource Allocation Graph (RAG) cycle recovery, working-set thrash guard, RAID parity blocks |
| **DBMS Engine** | Normalization & ACID concurrency | 4NF/5NF temporal warehouse, embedded dynamic SQL, Timestamp MVCC, 2PL locking & WAL recovery |
| **Visualization** | Real-time SOC telemetry | Flask REST API paired with an interactive **Three.js WebGL** 3D simulation interface |

---

## 👥 Team Modules (`src/`)

<details open>
<summary><b>Click to expand/collapse contributor breakdown</b></summary>
<br>

| Module | Contributor | Focus Areas |
| :--- | :--- | :--- |
| **`member1`** | **Priyanshu** | Syscall profiler, POSIX ring buffer, sync benchmarks, memory allocator, paging, embedded SQL & MVCC |
| **`member2`** | **Anjali** | Process supervisor, thread models, MLFQ scheduler, relational algebra & query cost optimizer |
| **`member3`** | **Aanya** | Working-set thrashing guard, log-structured FS, media recovery, B-Tree/NoSQL & 4NF/5NF warehouse |
| **`member4`** | **Drishti** | Deadlock recovery engine, FS block allocator, RAID parity, DB triggers, 2PL locking & WAL recovery |

</details>

---

## 📁 Repository Layout

```text
sentinelnet/
├── dashboard/                  # Flask server (app.py) & 3D WebGL UI (index.html)
├── config/                     # System & telemetry configuration files
├── data/                       # SQLite databases, audit logs & benchmark datasets
└── src/
    ├── member1/                # Kernel syscalls, ring buffer, memory & MVCC
    ├── member2/                # Process/thread models, MLFQ & query optimizer
    ├── member3/                # Thrashing guard, B-Tree/NoSQL & 4NF/5NF warehouse
    └── member4/                # Deadlock recovery, RAID parity, 2PL & WAL recovery
```

---

## 🚀 Quick Start

```bash
# 1. Clone the repository
git clone [https://github.com/priyanshugariya/sentinelnet.git](https://github.com/priyanshugariya/sentinelnet.git) && cd sentinelnet

# 2. Install Flask
pip install flask

# 3. Launch the 3D SOC Dashboard
python3 dashboard/app.py
```

> Open **`http://127.0.0.1:5000`** in your browser to interact with the live packet quarantine pipeline and all four OS/DBMS simulator suites.

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

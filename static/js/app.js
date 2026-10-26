let currentMemPolicy = 'FIRST_FIT';
let currentSchedAlgo = 'RR';
let currentPagingFrames = 3;
let currentDiskAlgo = 'LOOK';

// --- LIVE DB POLLING ---
function fetchLiveDbMetrics() {
    fetch('/api/member1/concurrency_stats')
        .then(res => res.json())
        .then(data => {
            const pkts = data.committed_txns || 0;
            const threats = data.threats_logged || 0;
            
            const ovPkts = document.getElementById('ov-pkts');
            const ovThreats = document.getElementById('ov-threats');
            const dbTxns = document.getElementById('db-txns');
            
            if (ovPkts) ovPkts.innerText = `${pkts} Packets`;
            if (ovThreats) ovThreats.innerText = `${threats} Threats`;
            if (dbTxns) dbTxns.innerText = `Committed: ${pkts} Rows | ${threats} Threats Logged`;
        })
        .catch(err => console.error("Error fetching live DB stats:", err));
}

// --- MEMBER 1 LOGIC: Dynamic Allocator ---
function setMemPolicy(policy, btn) {
    currentMemPolicy = policy;
    btn.parentElement.querySelectorAll('button').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    fetchMemoryState();
}

function fetchMemoryState() {
    fetch(`/api/member1/memory_state?policy=${currentMemPolicy}`)
        .then(res => res.json())
        .then(data => {
            const grid = document.getElementById('heapGrid');
            grid.innerHTML = '';
            data.blocks.forEach(b => {
                const el = document.createElement('div');
                el.className = `heap-unit status-${b.status.toLowerCase()}`;
                el.style.flex = `${b.span} 0 auto`;
                if (b.status === 'ALLOCATED') el.innerText = `#${b.tag}`;
                grid.appendChild(el);
            });
            document.getElementById('fragMetric').innerText = `External Fragmentation: ${data.external_frag_pct}%`;
        });
}

// --- MEMBER 2 LOGIC: CPU Scheduler ---
function setSchedAlgo(algo, btn) {
    currentSchedAlgo = algo;
    btn.parentElement.querySelectorAll('button').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    fetchSchedulerMetrics();
}

function fetchSchedulerMetrics() {
    fetch(`/api/member2/scheduler_metrics?algo=${currentSchedAlgo}`)
        .then(res => res.json())
        .then(data => {
            const gantt = document.getElementById('ganttBar');
            gantt.innerHTML = '';
            const colors = ['#2563eb', '#059669', '#7c3aed', '#d97706', '#dc2626'];

            data.gantt.forEach((slice) => {
                const span = slice.end - slice.start;
                const el = document.createElement('div');
                el.className = 'gantt-slice';
                el.style.flex = `${span} 0 auto`;
                el.style.backgroundColor = colors[slice.pid % colors.length];
                el.innerText = `P${slice.pid}`;
                gantt.appendChild(el);
            });

            document.getElementById('m_awt').innerText = `${data.awt.toFixed(2)} ms`;
            document.getElementById('m_atat').innerText = `${data.atat.toFixed(2)} ms`;
            document.getElementById('m_art').innerText = `${data.art.toFixed(2)} ms`;
            document.getElementById('m_csw').innerText = data.csw;
        });
}

// --- MEMBER 3 LOGIC: Virtual Memory Paging ---
function setPagingFrames(frames, btn) {
    currentPagingFrames = frames;
    btn.parentElement.querySelectorAll('button').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    fetchPagingStats();
}

function fetchPagingStats() {
    fetch(`/api/member3/paging_stats?frames=${currentPagingFrames}`)
        .then(res => res.json())
        .then(data => {
            document.getElementById('f_fifo').innerText = `${data.fault_comparison.FIFO} Faults`;
            document.getElementById('f_lru').innerText = `${data.fault_comparison.LRU} Faults`;
            document.getElementById('f_clk').innerText = `${data.fault_comparison.CLOCK} Faults`;
            document.getElementById('f_opt').innerText = `${data.fault_comparison.OPT} Faults`;
        });
}

// --- MEMBER 4 LOGIC: Disk Seek Arm ---
function setDiskAlgo(algo, btn) {
    currentDiskAlgo = algo;
    btn.parentElement.querySelectorAll('button').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    fetchDiskState();
}

function fetchDiskState() {
    fetch(`/api/member4/disk_and_deadlock?disk_algo=${currentDiskAlgo}`)
        .then(res => res.json())
        .then(data => {
            document.getElementById('diskHeadText').innerText = `Total Seek: ${data.total_seek} Cylinders`;
            const head = document.getElementById('diskHead');
            const targetPct = (data.total_seek % 180) / 1.8;
            head.style.left = `${Math.min(92, Math.max(4, targetPct))}%`;
            head.innerText = `${Math.round(targetPct * 2)}`;
            document.getElementById('ov-seek').innerText = `${data.total_seek} Cylinders (${currentDiskAlgo})`;
        });
}

// Initial load + poll live DB metrics every 2.5 seconds
document.addEventListener('DOMContentLoaded', () => {
    fetchLiveDbMetrics();
    fetchMemoryState();
    fetchSchedulerMetrics();
    fetchPagingStats();
    fetchDiskState();
    setInterval(fetchLiveDbMetrics, 2500);
});

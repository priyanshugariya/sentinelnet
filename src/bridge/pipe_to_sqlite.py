import os
import struct
import sqlite3

DB_PATH = "sentinel.db"
PIPE_PATH = "/tmp/sentinel_pipe"
PACKET_STRUCT_FMT = "=I16s16sHH8sIB32s"

def init_db():
    conn = sqlite3.connect(DB_PATH)
    with open("src/member3/schema_bcnf.sql") as f:
        conn.executescript(f.read())
    with open("src/member4/triggers_threat.sql") as f:
        conn.executescript(f.read())
    conn.commit()
    conn.close()

def main():
    init_db()
    if not os.path.exists(PIPE_PATH):
        os.mkfifo(PIPE_PATH)

    print("[Bridge] Ingestion daemon active. Waiting for worker packets on FIFO...")
    struct_size = struct.calcsize(PACKET_STRUCT_FMT)

    conn = sqlite3.connect(DB_PATH, isolation_level=None)
    cursor = conn.cursor()

    while True:
        with open(PIPE_PATH, "rb") as pipe:
            while True:
                data = pipe.read(struct_size)
                if not data or len(data) < struct_size:
                    break
                unpacked = struct.unpack(PACKET_STRUCT_FMT, data)
                pkt_id = unpacked[0]
                src_ip = unpacked[1].decode("utf-8", errors="ignore").split("\x00")[0]
                dst_ip = unpacked[2].decode("utf-8", errors="ignore").split("\x00")[0]
                src_port = unpacked[3]
                dst_port = unpacked[4]
                proto = unpacked[5].decode("utf-8", errors="ignore").split("\x00")[0]
                size = unpacked[6]
                malformed = unpacked[7]
                ts = unpacked[8].decode("utf-8", errors="ignore").split("\x00")[0]

                cursor.execute("""
                    INSERT OR REPLACE INTO PACKETS 
                    (packet_id, src_ip, dst_ip, src_port, dst_port, protocol, packet_size, is_malformed, captured_at)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """, (pkt_id, src_ip, dst_ip, src_port, dst_port, proto, size, malformed, ts))

if __name__ == "__main__":
    main()

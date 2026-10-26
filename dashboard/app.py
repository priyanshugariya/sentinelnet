from flask import Flask, render_template, jsonify
import sqlite3

app = Flask(__name__)
DB_PATH = "sentinel.db"

def query_db(query, args=(), one=False):
    try:
        conn = sqlite3.connect(DB_PATH)
        conn.row_factory = sqlite3.Row
        cur = conn.cursor()
        cur.execute(query, args)
        r = cur.fetchall()
        conn.close()
        return (r[0] if r else None) if one else r
    except Exception:
        return []

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/stats")
def api_stats():
    total_packets = query_db("SELECT COUNT(*) as cnt FROM PACKETS", one=True)
    threats = query_db("SELECT COUNT(*) as cnt FROM THREAT_EVENTS", one=True)
    blocked = query_db("SELECT COUNT(*) as cnt FROM IP_TRACKING WHERE status='BLOCKED'", one=True)
    recent_packets = query_db("SELECT * FROM PACKETS ORDER BY packet_id DESC LIMIT 10")
    recent_threats = query_db("SELECT * FROM THREAT_EVENTS ORDER BY event_id DESC LIMIT 5")
    blocked_ips = query_db("SELECT * FROM IP_TRACKING WHERE status='BLOCKED'")

    return jsonify({
        "total_packets": total_packets["cnt"] if total_packets else 0,
        "total_threats": threats["cnt"] if threats else 0,
        "blocked_ips_count": blocked["cnt"] if blocked else 0,
        "packets": [dict(ix) for ix in recent_packets],
        "threats": [dict(ix) for ix in recent_threats],
        "blocked_ips": [dict(ix) for ix in blocked_ips]
    })

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)

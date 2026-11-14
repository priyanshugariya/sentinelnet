#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);
    sqlite3 *db = NULL;

    if (sqlite3_open("data/sentinel_er.db", &db) != SQLITE_OK) {
        fprintf(stderr, "DB open error: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    FILE *f = fopen("src/member1/er_model_and_queries.sql", "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *sql = malloc(sz + 1);
        if (fread(sql, 1, sz, f) > 0) {
            sql[sz] = '\0';
            sqlite3_exec(db, sql, NULL, NULL, NULL);
        }
        free(sql);
        fclose(f);
    }

    const char *host_src = "192.168.1.45";
    const char *host_dst = "10.0.0.1";
    int host_proto = 6;
    int host_bytes = 1480;

    sqlite3_stmt *ins_stmt;
    sqlite3_prepare_v2(db,
        "INSERT INTO er_packet (src_ip, dst_ip, proto_id, payload_bytes) VALUES (?1, ?2, ?3, ?4);",
        -1, &ins_stmt, NULL);
    sqlite3_bind_text(ins_stmt, 1, host_src, -1, SQLITE_STATIC);
    sqlite3_bind_text(ins_stmt, 2, host_dst, -1, SQLITE_STATIC);
    sqlite3_bind_int(ins_stmt, 3, host_proto);
    sqlite3_bind_int(ins_stmt, 4, host_bytes);
    sqlite3_step(ins_stmt);
    sqlite3_finalize(ins_stmt);

    long long new_pkt_id = sqlite3_last_insert_rowid(db);

    sqlite3_stmt *frag_stmt;
    sqlite3_prepare_v2(db,
        "INSERT INTO er_packet_fragment (packet_id, frag_offset, more_fragments, frag_bytes) VALUES (?1, 0, 1, 740);",
        -1, &frag_stmt, NULL);
    sqlite3_bind_int64(frag_stmt, 1, new_pkt_id);
    sqlite3_step(frag_stmt);
    sqlite3_finalize(frag_stmt);

    const char *user_filter_ip = "192.168.1.45' OR '1'='1";
    sqlite3_stmt *dyn_stmt;
    sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM er_packet WHERE src_ip = ?1;",
        -1, &dyn_stmt, NULL);
    sqlite3_bind_text(dyn_stmt, 1, user_filter_ip, -1, SQLITE_STATIC);
    int sqli_matched = 0;
    if (sqlite3_step(dyn_stmt) == SQLITE_ROW) sqli_matched = sqlite3_column_int(dyn_stmt, 0);
    sqlite3_finalize(dyn_stmt);

    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM er_packet WHERE src_ip = ?1;", -1, &dyn_stmt, NULL);
    sqlite3_bind_text(dyn_stmt, 1, host_src, -1, SQLITE_STATIC);
    int legitimate_rows = 0;
    if (sqlite3_step(dyn_stmt) == SQLITE_ROW) legitimate_rows = sqlite3_column_int(dyn_stmt, 0);
    sqlite3_finalize(dyn_stmt);

    sqlite3_close(db);

    if (json_mode) {
        printf("{\n"
               "  \"inserted_packet_id\": %lld,\n"
               "  \"weak_entity_fragment_linked\": true,\n"
               "  \"legitimate_query_count\": %d,\n"
               "  \"sqli_injection_probe_rows\": %d,\n"
               "  \"binding_protection\": \"ACTIVE\"\n"
               "}\n",
               new_pkt_id, legitimate_rows, sqli_matched);
    } else {
        printf("=== DBMS Unit 3: Embedded & Dynamic SQL (C -> SQLite) ===\n");
        printf("Inserted Packet ID: %lld (with Weak Entity Fragment offset=0)\n", new_pkt_id);
        printf("Legitimate Parameterized Lookup ('192.168.1.45'): %d rows\n", legitimate_rows);
        printf("SQLi Payload Probe (\"192.168.1.45' OR '1'='1\"): %d rows (Neutralized)\n", sqli_matched);
    }
    return 0;
}

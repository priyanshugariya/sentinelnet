#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

typedef struct {
    int packet_id;
    char src_ip[20];
    int bytes;
    char verdict[16];
} BinaryLogRecord;

static const char *check_access_matrix(const char *role, const char *resource, const char *action) {
    if (strcmp(role, "ADMIN") == 0) return "ALLOW";
    if (strcmp(role, "ANALYST") == 0) {
        if (strcmp(resource, "ThreatEvents") == 0 && strcmp(action, "READ") == 0) return "ALLOW";
        if (strcmp(resource, "BlockedIPs") == 0 && strcmp(action, "WRITE") == 0) return "ALLOW";
        return "DENY (Least Privilege)";
    }
    if (strcmp(role, "VIEWER") == 0) {
        if (strcmp(action, "READ") == 0 && strcmp(resource, "RawLogs") != 0) return "ALLOW";
        return "DENY (Read-Only Summary)";
    }
    return "DENY";
}

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

    /* 1. Hierarchical Date-Based Directory Tree */
    mkdir("data/logs", 0755);
    mkdir("data/logs/2026", 0755);
    mkdir("data/logs/2026/11", 0755);
    mkdir("data/logs/2026/11/12", 0750);

    const char *log_path = "data/logs/2026/11/12/packets.bin";
    int fd = open(log_path, O_CREAT | O_RDWR | O_TRUNC, 0640);
    chmod(log_path, 0640);

    BinaryLogRecord recs[4] = {
        {101, "192.168.1.10", 512,  "CLEAN"},
        {102, "192.168.1.45", 1480, "QUARANTINED"},
        {103, "10.24.8.12",   256,  "CLEAN"},
        {104, "172.16.4.99",  1500, "BLOCKED"}
    };
    if (write(fd, recs, sizeof(recs)) < 0) perror("write");

    /* 2. Indexed Direct Access via lseek() to Record #3 (index 2) without sequential scan */
    int target_index = 2;
    off_t offset = (off_t)(target_index * sizeof(BinaryLogRecord));
    lseek(fd, offset, SEEK_SET);
    BinaryLogRecord fetched = {0};
    if (read(fd, &fetched, sizeof(BinaryLogRecord)) < 0) perror("read");
    close(fd);

    struct stat st;
    stat(log_path, &st);
    unsigned int perms = st.st_mode & 0777;

    const char *r1 = check_access_matrix("ADMIN", "ThresholdRules", "DELETE");
    const char *r2 = check_access_matrix("ANALYST", "BlockedIPs", "WRITE");
    const char *r3 = check_access_matrix("ANALYST", "ThresholdRules", "DELETE");
    const char *r4 = check_access_matrix("VIEWER", "RawLogs", "READ");

    if (json_mode) {
        printf("{\n"
               "  \"hierarchical_path\": \"%s\",\n"
               "  \"octal_permissions\": \"0%o\",\n"
               "  \"direct_lseek_access\": {\n"
               "    \"target_record_index\": %d,\n"
               "    \"byte_offset\": %ld,\n"
               "    \"fetched_packet_id\": %d,\n"
               "    \"fetched_src_ip\": \"%s\",\n"
               "    \"fetched_verdict\": \"%s\"\n"
               "  },\n"
               "  \"access_matrix_checks\": [\n"
               "    {\"role\": \"ADMIN\", \"resource\": \"ThresholdRules\", \"action\": \"DELETE\", \"decision\": \"%s\"},\n"
               "    {\"role\": \"ANALYST\", \"resource\": \"BlockedIPs\", \"action\": \"WRITE\", \"decision\": \"%s\"},\n"
               "    {\"role\": \"ANALYST\", \"resource\": \"ThresholdRules\", \"action\": \"DELETE\", \"decision\": \"%s\"},\n"
               "    {\"role\": \"VIEWER\", \"resource\": \"RawLogs\", \"action\": \"READ\", \"decision\": \"%s\"}\n"
               "  ]\n"
               "}\n",
               log_path, perms, target_index, (long)offset,
               fetched.packet_id, fetched.src_ip, fetched.verdict,
               r1, r2, r3, r4);
    } else {
        printf("=== OS Unit 5: File System Hierarchy, Direct lseek & Access Matrix (Aanya) ===\n");
        printf("Created Log File: %s (Permissions: 0%o)\n", log_path, perms);
        printf("Direct lseek() at Byte Offset %ld -> Packet #%d | IP: %s | Verdict: %s\n",
               (long)offset, fetched.packet_id, fetched.src_ip, fetched.verdict);
        printf("Access Matrix Enforcement:\n");
        printf("  ADMIN   -> DELETE ThresholdRules : %s\n", r1);
        printf("  ANALYST -> WRITE  BlockedIPs     : %s\n", r2);
        printf("  ANALYST -> DELETE ThresholdRules : %s\n", r3);
        printf("  VIEWER  -> READ   RawLogs        : %s\n", r4);
    }
    return 0;
}

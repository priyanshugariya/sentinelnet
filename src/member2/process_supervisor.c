#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mqueue.h>

#define SHM_NAME "/sentinelnet_shm_stats"
#define MQ_NAME  "/sentinelnet_ctrl_mq"
#define FIFO_PATH "data/sentinel_ipc.fifo"

typedef struct {
    int active_workers;
    long packets_parsed;
    long malformed_dropped;
} SharedTelemetry;

int main(int argc, char **argv) {
    int json_mode = (argc > 1 && strcmp(argv[1], "--json") == 0);

    /* 1. Named Pipe (FIFO) Setup */
    unlink(FIFO_PATH);
    mkfifo(FIFO_PATH, 0666);

    /* 2. POSIX Shared Memory Setup */
    shm_unlink(SHM_NAME);
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (ftruncate(shm_fd, sizeof(SharedTelemetry)) < 0) perror("ftruncate");
    SharedTelemetry *shm = mmap(NULL, sizeof(SharedTelemetry), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    shm->active_workers = 0;
    shm->packets_parsed = 0;
    shm->malformed_dropped = 0;

    /* 3. POSIX Message Queue Setup */
    mq_unlink(MQ_NAME);
    struct mq_attr attr = {0};
    attr.mq_maxmsg = 8;
    attr.mq_msgsize = 64;
    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    const char *ctrl_msg = "CMD:START_INSPECTION_MODE=DEEP";
    if (mq != (mqd_t)-1) {
        mq_send(mq, ctrl_msg, strlen(ctrl_msg) + 1, 1);
    }

    /* 4. Fork Child Process #1 (Simulates crash -> Supervisor Auto-Restart) */
    pid_t pid1 = fork();
    if (pid1 == 0) {
        /* Child simulates unexpected worker exit */
        _exit(42);
    }
    int status1 = 0;
    waitpid(pid1, &status1, 0);
    int first_exit_code = WIFEXITED(status1) ? WEXITSTATUS(status1) : -1;

    /* 5. Supervisor Respawns Healthy Child Process #2 */
    pid_t pid2 = fork();
    if (pid2 == 0) {
        shm->active_workers = 2;
        shm->packets_parsed = 1250;
        shm->malformed_dropped = 14;

        int fd = open(FIFO_PATH, O_WRONLY);
        if (fd >= 0) {
            const char *msg = "PARSED_BATCH:1250,DROPPED:14\n";
            if (write(fd, msg, strlen(msg)) < 0) perror("write");
            close(fd);
        }
        _exit(0);
    }

    /* Parent reads Named Pipe & Message Queue */
    char fifo_buf[128] = {0};
    int rfd = open(FIFO_PATH, O_RDONLY);
    if (rfd >= 0) {
        if (read(rfd, fifo_buf, sizeof(fifo_buf) - 1) < 0) perror("read");
        close(rfd);
    }
    fifo_buf[strcspn(fifo_buf, "\n")] = '\0';

    char mq_buf[64] = "CMD:START_INSPECTION_MODE=DEEP";
    if (mq != (mqd_t)-1) {
        mq_receive(mq, mq_buf, sizeof(mq_buf), NULL);
    }

    int status2 = 0;
    waitpid(pid2, &status2, 0);

    long parsed = shm->packets_parsed;
    long dropped = shm->malformed_dropped;

    /* Cleanup IPC resources */
    munmap(shm, sizeof(SharedTelemetry));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    if (mq != (mqd_t)-1) {
        mq_close(mq);
        mq_unlink(MQ_NAME);
    }
    unlink(FIFO_PATH);

    if (json_mode) {
        printf("{\n"
               "  \"supervisor_pid\": %d,\n"
               "  \"crashed_child\": {\"pid\": %d, \"exit_code\": %d, \"auto_restarted\": true},\n"
               "  \"healthy_child\": {\"pid\": %d, \"exit_code\": 0},\n"
               "  \"ipc_channels\": {\n"
               "    \"named_pipe\": \"%s\",\n"
               "    \"shared_memory\": {\"packets_parsed\": %ld, \"malformed_dropped\": %ld},\n"
               "    \"message_queue\": \"%s\"\n"
               "  }\n"
               "}\n",
               getpid(), pid1, first_exit_code, pid2, fifo_buf, parsed, dropped, mq_buf);
    } else {
        printf("=== OS Unit 2: Process Supervisor & IPC Bridge (Anjali) ===\n");
        printf("Supervisor PID: %d\n", getpid());
        printf("Child #1 (PID %d) exited with code %d -> Auto-Restarted as Child #2 (PID %d)\n",
               pid1, first_exit_code, pid2);
        printf("IPC [Named Pipe]    : %s\n", fifo_buf);
        printf("IPC [Shared Memory] : parsed=%ld, malformed_dropped=%ld\n", parsed, dropped);
        printf("IPC [Message Queue] : %s\n", mq_buf);
    }
    return 0;
}

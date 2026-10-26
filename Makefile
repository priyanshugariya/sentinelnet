CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -pthread

all: pipeline_producer pipeline_worker mem_allocator scheduler_sim page_replace_sim mmap_fault_tracker banker_wfg_sim disk_scheduler

pipeline_producer: src/member1/producer_ringbuffer.c
	$(CC) $(CFLAGS) -o $@ $^

pipeline_worker: src/member2/worker_parser.c
	$(CC) $(CFLAGS) -o $@ $^

mem_allocator: src/member1/mem_allocator.c
	$(CC) $(CFLAGS) -o $@ $^

scheduler_sim: src/member2/scheduler_sim.c
	$(CC) $(CFLAGS) -o $@ $^

page_replace_sim: src/member3/page_replace_sim.c
	$(CC) $(CFLAGS) -o $@ $^

mmap_fault_tracker: src/member3/mmap_fault_tracker.c
	$(CC) $(CFLAGS) -o $@ $^

banker_wfg_sim: src/member4/banker_wfg_sim.c
	$(CC) $(CFLAGS) -o $@ $^

disk_scheduler: src/member4/disk_scheduler.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f pipeline_producer pipeline_worker mem_allocator scheduler_sim page_replace_sim mmap_fault_tracker banker_wfg_sim disk_scheduler sentinel.db /tmp/sentinel_pipe

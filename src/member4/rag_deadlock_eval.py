#!/usr/bin/env python3
"""
SentinelNet Resource Allocation Graph (RAG) Deadlock Detection Engine
Member 4: Drishti Verma (OS Unit 3: Deadlock Detection & Graph Reduction)
"""

from collections import defaultdict, deque

class ResourceAllocationGraph:
    def __init__(self):
        # Directed bipartite graph: Process -> Resource (Request), Resource -> Process (Assignment)
        self.edges = defaultdict(list)
        self.processes = set()
        self.resources = set()

    def add_assignment(self, resource, process):
        """Resource is allocated to Process: R -> P"""
        self.edges[resource].append(process)
        self.resources.add(resource)
        self.processes.add(process)

    def add_request(self, process, resource):
        """Process requests Resource: P -> R"""
        self.edges[process].append(resource)
        self.processes.add(process)
        self.resources.add(resource)

    def detect_cycles(self):
        visited = {}
        rec_stack = {}
        all_nodes = list(self.processes | self.resources)
        cycle_nodes = []

        for node in all_nodes:
            visited[node] = False
            rec_stack[node] = False

        def dfs(curr, path):
            visited[curr] = True
            rec_stack[curr] = True
            path.append(curr)

            for neighbor in self.edges.get(curr, []):
                if not visited[neighbor]:
                    if dfs(neighbor, path):
                        return True
                elif rec_stack[neighbor]:
                    cycle_start = path.index(neighbor)
                    cycle_nodes.extend(path[cycle_start:] + [neighbor])
                    return True

            rec_stack[curr] = False
            path.pop()
            return False

        for node in all_nodes:
            if not visited[node]:
                if dfs(node, []):
                    return True, cycle_nodes
        return False, []

    def print_assessment(self, scenario_title):
        has_cycle, cycle = self.detect_cycles()
        print("=" * 70)
        print(f" RESOURCE ALLOCATION GRAPH (RAG) EVALUATION: {scenario_title.upper()}")
        print("=" * 70)
        print(f" Active Process Nodes  : {sorted(list(self.processes))}")
        print(f" Resource Instance Pool: {sorted(list(self.resources))}")
        print("-" * 70)
        print(" Directed Dependency Edges:")
        for src, dests in sorted(self.edges.items()):
            for dst in dests:
                edge_type = "Requests" if src in self.processes else "Assigned To"
                print(f"   [{src}] ---({edge_type})---> [{dst}]")

        print("-" * 70)
        if has_cycle:
            print(" GRAPH REDUCTION STATUS: DEADLOCK DETECTED! (Circular Wait)")
            print(f" Cycle Loop Path       : {' -> '.join(cycle)}")
            print(" Resolution Strategy   : Abort youngest transaction or roll back locks.")
        else:
            print(" GRAPH REDUCTION STATUS: COMPLETELY REDUCIBLE (SAFE STATE)")
            print(" Deadlock Assessment   : No circular wait. All processes can resolve.")
        print("=" * 70 + "\n")

def main():
    # Scenario 1: Safe Concurrent Dispatch
    rag_safe = ResourceAllocationGraph()
    rag_safe.add_assignment("R_DISK_IO", "P1")
    rag_safe.add_assignment("R_SHARED_MEM", "P2")
    rag_safe.add_request("P1", "R_SHARED_MEM")
    rag_safe.print_assessment("Normal High-Throughput Packet Dispatch")

    # Scenario 2: Deadlock Condition (Circular wait: P1 -> R2 -> P2 -> R1 -> P1)
    rag_deadlock = ResourceAllocationGraph()
    rag_deadlock.add_assignment("R_PACKET_RING", "P1")
    rag_deadlock.add_request("P1", "R_SQL_WRITE_LOCK")
    rag_deadlock.add_assignment("R_SQL_WRITE_LOCK", "P2")
    rag_deadlock.add_request("P2", "R_PACKET_RING")
    rag_deadlock.print_assessment("Worker Deadlock via Mutual Contention")

if __name__ == "__main__":
    main()

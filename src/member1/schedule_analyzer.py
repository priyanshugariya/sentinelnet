#!/usr/bin/env python3
import json
import sys
from collections import defaultdict, deque

class PrecedenceGraphAnalyzer:
    def __init__(self, schedule_raw):
        self.raw_schedule = schedule_raw
        self.transactions = []
        self.operations = []
        self.conflict_edges = defaultdict(set)
        self.conflict_pairs = []
        self.in_degree = defaultdict(int)
        self.all_nodes = set()
        self._parse_schedule()
        self._build_conflict_graph()

    def _parse_schedule(self):
        for item in self.raw_schedule:
            txn, op, data = item[0], item[1].upper(), item[2].upper()
            self.operations.append({'txn': txn, 'op': op, 'data': data})
            self.all_nodes.add(txn)
        self.transactions = sorted(list(self.all_nodes))

    def _build_conflict_graph(self):
        for t in self.transactions:
            self.in_degree[t] = 0

        n = len(self.operations)
        for i in range(n):
            for j in range(i + 1, n):
                op1 = self.operations[i]
                op2 = self.operations[j]

                if op1['txn'] != op2['txn'] and op1['data'] == op2['data']:
                    if op1['op'] == 'W' or op2['op'] == 'W':
                        src = op1['txn']
                        dst = op2['txn']
                        if dst not in self.conflict_edges[src]:
                            self.conflict_edges[src].add(dst)
                            self.conflict_pairs.append({
                                'from': src,
                                'to': dst,
                                'data': op1['data'],
                                'cause': f"{src}:{op1['op']}({op1['data']}) -> {dst}:{op2['op']}({op2['data']})"
                            })

        for src, neighbors in self.conflict_edges.items():
            for dst in neighbors:
                self.in_degree[dst] += 1

    def detect_cycles_dfs(self):
        visited = {t: False for t in self.transactions}
        rec_stack = {t: False for t in self.transactions}
        cycle_path = []

        def dfs_visit(node, current_path):
            visited[node] = True
            rec_stack[node] = True
            current_path.append(node)

            for neighbor in sorted(list(self.conflict_edges[node])):
                if not visited[neighbor]:
                    res = dfs_visit(neighbor, current_path)
                    if res:
                        return True
                elif rec_stack[neighbor]:
                    cycle_start = current_path.index(neighbor)
                    cycle_path.extend(current_path[cycle_start:] + [neighbor])
                    return True

            rec_stack[node] = False
            current_path.pop()
            return False

        for node in self.transactions:
            if not visited[node]:
                if dfs_visit(node, []):
                    return True, cycle_path
        return False, []

    def get_topological_sort(self):
        has_cycle, _ = self.detect_cycles_dfs()
        if has_cycle:
            return None

        in_deg = self.in_degree.copy()
        queue = deque([t for t in self.transactions if in_deg[t] == 0])
        order = []

        while queue:
            node = queue.popleft()
            order.append(node)
            for neighbor in self.conflict_edges[node]:
                in_deg[neighbor] -= 1
                if in_deg[neighbor] == 0:
                    queue.append(neighbor)

        if len(order) == len(self.transactions):
            return order
        return None

    def export_summary(self):
        has_cycle, cycle = self.detect_cycles_dfs()
        topo_order = self.get_topological_sort()

        edges_serializable = {k: list(v) for k, v in self.conflict_edges.items()}
        return {
            'transactions': self.transactions,
            'total_operations': len(self.operations),
            'conflict_edges': edges_serializable,
            'conflicts_count': len(self.conflict_pairs),
            'conflict_details': self.conflict_pairs,
            'is_conflict_serializable': not has_cycle,
            'cycle_detected': cycle if has_cycle else None,
            'equivalent_serial_order': " -> ".join(topo_order) if topo_order else "NONE (Cycle Exists)"
        }

    def print_formal_report(self, schedule_name="Schedule Analysis"):
        summary = self.export_summary()
        print("=" * 72)
        print(f" SENTINELNET DBMS CONCURRENCY ANALYZER: {schedule_name.upper()}")
        print(" (Member 1: DBMS Unit 5 - Serializability & Precedence Graph)")
        print("=" * 72)
        print(f" Transactions Involved : {', '.join(summary['transactions'])}")
        print(f" Total Operations      : {summary['total_operations']}")
        print(f" Conflicting Pairs     : {summary['conflicts_count']}")
        print("-" * 72)
        print(" Identified Conflict Dependencies:")
        for c in summary['conflict_details']:
            print(f"  * {c['cause']}")

        print("-" * 72)
        print(" Precedence Adjacency Graph:")
        for t in summary['transactions']:
            targets = summary['conflict_edges'].get(t, [])
            target_str = ", ".join(targets) if targets else "(none)"
            print(f"  * Node [{t}] --> {target_str}")

        print("-" * 72)
        print(" Verification Assessment:")
        if summary['is_conflict_serializable']:
            print("  STATUS : CONFLICT SERIALIZABLE (Graph is an Acyclic Directed Graph / DAG)")
            print(f"  EQUIVALENT SERIAL SCHEDULE: {summary['equivalent_serial_order']}")
            print("  SAFETY : Safe for concurrent execution without transaction rollback.")
        else:
            print("  STATUS : NON-SERIALIZABLE (Cycle Detected in Precedence Graph)")
            print(f"  CYCLE PATH : {' -> '.join(summary['cycle_detected'])}")
            print("  SAFETY : Risk of Dirty Reads, Lost Updates, or Deadlocks. Strict 2PL required.")
        print("=" * 72 + "\n")

if __name__ == "__main__":
    schedule_safe = [
        ('T1', 'R', 'PACKET_RING'),
        ('T2', 'R', 'PACKET_RING'),
        ('T1', 'W', 'PACKET_RING'),
        ('T2', 'W', 'PACKET_RING'),
        ('T3', 'R', 'PACKET_RING'),
        ('T3', 'W', 'PACKET_RING'),
    ]

    schedule_unsafe = [
        ('T1', 'R', 'LOG_STORAGE'),
        ('T2', 'W', 'LOG_STORAGE'),
        ('T3', 'R', 'THREAT_TABLE'),
        ('T1', 'W', 'LOG_STORAGE'),
        ('T2', 'R', 'LOG_STORAGE'),
        ('T3', 'W', 'LOG_STORAGE'),
    ]

    analyzer_safe = PrecedenceGraphAnalyzer(schedule_safe)
    analyzer_safe.print_formal_report("Safe Ingestion Schedule")

    analyzer_unsafe = PrecedenceGraphAnalyzer(schedule_unsafe)
    analyzer_unsafe.print_formal_report("High-Contention Conflicting Schedule")

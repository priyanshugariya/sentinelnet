#!/usr/bin/env python3
"""
SentinelNet Relational Decomposition & Normal Form Analyzer
Member 3: Aanya Godiyal (DBMS Unit 2 & 3: Normalization & BCNF Proof)
"""

import sys

class FunctionalDependency:
    def __init__(self, determinant, dependent):
        self.determinant = set(determinant)
        self.dependent = set(dependent)

    def __repr__(self):
        return f"{','.join(sorted(self.determinant))} -> {','.join(sorted(self.dependent))}"

    def __str__(self):
        return f"{','.join(sorted(self.determinant))} -> {','.join(sorted(self.dependent))}"


class RelationalSchemaValidator:
    def __init__(self, relation_name, attributes, fds, candidate_keys):
        self.relation_name = relation_name
        self.attributes = set(attributes)
        self.fds = [FunctionalDependency(d[0], d[1]) for d in fds]
        self.candidate_keys = [set(k) for k in candidate_keys]

    def compute_attribute_closure(self, attr_subset):
        closure = set(attr_subset)
        changed = True
        while changed:
            changed = False
            for fd in self.fds:
                if fd.determinant.issubset(closure) and not fd.dependent.issubset(closure):
                    closure.update(fd.dependent)
                    changed = True
        return closure

    def verify_superkey(self, attr_subset):
        closure = self.compute_attribute_closure(attr_subset)
        return self.attributes.issubset(closure)

    def evaluate_bcnf_compliance(self):
        violations = []
        for fd in self.fds:
            # BCNF condition: For every non-trivial X -> Y, X must be a superkey
            is_trivial = fd.dependent.issubset(fd.determinant)
            if not is_trivial:
                if not self.verify_superkey(fd.determinant):
                    violations.append(fd)
        return len(violations) == 0, violations

    def print_assessment(self):
        print("=" * 68)
        print(f" RELATIONAL SCHEMA AUDIT: {self.relation_name}")
        print("=" * 68)
        print(f" Relation Universal Set R : {{ {', '.join(sorted(self.attributes))} }}")
        print(f" Candidate Keys Defined   : {[list(k) for k in self.candidate_keys]}")
        print(" Functional Dependencies  :")
        for fd in self.fds:
            closure = self.compute_attribute_closure(fd.determinant)
            is_sk = self.verify_superkey(fd.determinant)
            fd_str = str(fd)
            print(f"   * {fd_str:<30} | Closure: {set(sorted(closure))} | Superkey: {is_sk}")

        is_bcnf, violations = self.evaluate_bcnf_compliance()
        print("-" * 68)
        if is_bcnf:
            print(" FORMAL PROOF RESULT      : STRICT BCNF (Boyce-Codd Normal Form)")
            print(" Redundancy Level         : 0% (Free of Update/Delete Anomalies)")
        else:
            print(" FORMAL PROOF RESULT      : VIOLATES BCNF")
            print(" Violating Dependencies   :")
            for v in violations:
                print(f"   ! Determinant '{','.join(v.determinant)}' is not a Superkey.")
        print("=" * 68 + "\n")


def main():
    print("\n====================================================================")
    print(" SENTINELNET DBMS NORMALIZATION ENGINE (MEMBER 3: DBMS UNIT 2/3)")
    print(" Algorithmic Closure & Superkey Dependency Assessment")
    print("====================================================================\n")

    # Table 1: host_registry (Normalized Host Entity)
    r1 = RelationalSchemaValidator(
        relation_name="host_registry",
        attributes=["ip_address", "asn_number", "country_code", "reputation_score"],
        fds=[
            (["ip_address"], ["asn_number", "country_code", "reputation_score"])
        ],
        candidate_keys=[["ip_address"]]
    )
    r1.print_assessment()

    # Table 2: packet_stream (Normalized Packet Ledger)
    r2 = RelationalSchemaValidator(
        relation_name="packet_stream",
        attributes=["packet_id", "src_ip", "dst_ip", "src_port", "dst_port", "protocol_id"],
        fds=[
            (["packet_id"], ["src_ip", "dst_ip", "src_port", "dst_port", "protocol_id"])
        ],
        candidate_keys=[["packet_id"]]
    )
    r2.print_assessment()

    # Table 3: Unnormalized Ingestion (Deliberate 2NF violation to illustrate contrast)
    r3 = RelationalSchemaValidator(
        relation_name="unnormalized_legacy_log (Flawed 2NF Design)",
        attributes=["packet_id", "src_ip", "src_country", "dst_ip"],
        fds=[
            (["packet_id"], ["src_ip", "dst_ip"]),
            (["src_ip"], ["src_country"])  # Transitive dependency anomaly
        ],
        candidate_keys=[["packet_id"]]
    )
    r3.print_assessment()


if __name__ == "__main__":
    main()

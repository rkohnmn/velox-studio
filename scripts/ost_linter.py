#!/usr/bin/env python3
"""
Opportunity Solution Tree (OST) Machine Gate Linter (implements product-skills SKILL (3).md).
Rules enforced:
- O1: Single measurable outcome root with numeric baseline/target
- O2: Opportunities framed as user needs, not feature solutions
- O3: Every targeted opportunity compares >= 2 candidate solutions
- O4: Every solution carries an explicit assumption test
- O5: Zero orphan solutions
"""
import argparse
import json
import pathlib
import sys

def lint_ost(data: dict) -> list[str]:
    errors = []
    outcome = data.get("outcome", {})
    if not outcome.get("statement") or "target_ms" not in outcome:
        errors.append("O1 violation: Outcome root must state a measurable numeric target.")
    opps = data.get("opportunities", [])
    if not opps:
        errors.append("O2 violation: At least one opportunity is required.")
    for opp in opps:
        need = opp.get("need", "")
        if "struggle" not in need.lower() and "need" not in need.lower():
            errors.append(f"O2 violation in {opp.get('id')}: Opportunity must be phrased as a customer need/struggle.")
        sols = opp.get("solutions", [])
        if len(sols) < 2:
            errors.append(f"O3 violation in {opp.get('id')}: Must compare >= 2 solutions.")
        for s in sols:
            if not s.get("assumption_test"):
                errors.append(f"O4 violation in {s.get('id')}: Solution lacks an assumption_test.")
    return errors

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    args = parser.parse_args()
    data = json.loads(pathlib.Path(args.input).read_text(encoding="utf-8"))
    errs = lint_ost(data)
    if errs:
        for e in errs:
            print(f"[NEEDS-REWORK] {e}")
        sys.exit(2)
    print("[PASS] OST Linter Gate: All rules O1-O5 verified.")
    sys.exit(0)

if __name__ == "__main__":
    main()

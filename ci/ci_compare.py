#!/usr/bin/env python3
import sys, json, math

def main(baseline_path, run_path):
    with open(baseline_path) as f:
        base = json.load(f)
    with open(run_path) as f:
        run = json.load(f)

    # Graceful handling if empty
    if not run:
        print("[ci_compare] WARN: run JSON empty — treating as failure.")
        sys.exit(1)

    bstats = base.get("stats", {})
    rstats = run.get("stats", {})

    # Allowed growth
    p99_growth_max = base.get("budgets", {}).get("p99_growth_pct_max", 5.0)
    cv_max = base.get("budgets", {}).get("cv_max", 0.08)

    def pct_growth(b, r):
        if b <= 0: 
            return 1e9
        return 100.0 * (r - b) / b

    p50_ok = rstats.get("p50", 1e12) <= base.get("budgets", {}).get("p50_cycles_max", 1e12)
    p99_ok = pct_growth(bstats.get("p99", 1), rstats.get("p99", 1e12)) <= p99_growth_max
    cv_ok  = rstats.get("cv", 1.0) <= cv_max

    print(f"[ci_compare] p50_ok={p50_ok} p99_ok={p99_ok} cv_ok={cv_ok}")
    if p50_ok and p99_ok and cv_ok:
        print("[ci_compare] PASS")
        sys.exit(0)
    else:
        print("[ci_compare] FAIL")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: ci_compare.py baseline.json run.json")
        sys.exit(2)
    main(sys.argv[1], sys.argv[2])

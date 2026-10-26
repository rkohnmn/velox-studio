#!/usr/bin/env python3
"""
Deterministic Repository Validator (implements spec-to-repo SKILL (6).md Phase 4
+ Strict Zero-HTML / Zero-Browser Native Bundled Executable Gate).
"""
import argparse
import json
import pathlib
import sys

def validate(root: pathlib.Path) -> dict:
    checks = {}
    checks["readme_exists"] = (root / "README.md").is_file() and (root / "README.md").stat().st_size > 500
    checks["design_md_exists"] = (root / "DESIGN.md").is_file() and (root / "DESIGN.md").stat().st_size > 500
    checks["gitignore_exists"] = (root / ".gitignore").is_file()
    checks["env_example_exists"] = (root / ".env.example").is_file()
    checks["manifest_exists"] = (root / "CMakeLists.txt").is_file() and (root / "Makefile").is_file()
    checks["no_committed_env"] = not (root / ".env").exists()
    checks["tests_exist"] = any((root / "tests").glob("test_*.cpp"))
    checks["native_gui_exists"] = (root / "include/velox/native_gui.hpp").is_file() and (root / "src/native_gui.cpp").is_file()

    html_files = list(root.rglob("*.html")) + list(root.rglob("*.htm")) + list(root.rglob("*.css")) + list(root.rglob("*.js"))
    checks["zero_html_files"] = (len(html_files) == 0)

    todo_hits = []
    browser_hits = []
    for ext in ("*.cpp", "*.hpp"):
        for p in root.rglob(ext):
            txt = p.read_text(encoding="utf-8", errors="ignore")
            if "TODO:" in txt or "FIXME:" in txt:
                todo_hits.append(str(p.relative_to(root)))
            if "<!DOCTYPE html>" in txt or "<html" in txt:
                browser_hits.append(str(p.relative_to(root)))
    checks["zero_todo_placeholders"] = (len(todo_hits) == 0)
    checks["zero_browser_or_html_in_cpp"] = (len(browser_hits) == 0)

    passed = all(checks.values())
    return {"passed": passed, "checks": checks, "todo_files": todo_hits, "browser_hits": browser_hits}

def main():
    parser = argparse.ArgumentParser(description="Validate generated repository")
    parser.add_argument("path", help="Path to repository root")
    parser.add_argument("--format", choices=["text", "json"], default="text")
    args = parser.parse_args()

    res = validate(pathlib.Path(args.path).resolve())
    if args.format == "json":
        print(json.dumps(res, indent=2))
    else:
        print("=== VELOX REPOSITORY & NATIVE GUI HYGIENE AUDIT ===")
        for k, v in res["checks"].items():
            print(f"  [{'PASS' if v else 'FAIL'}] {k}")
        print(f"Overall Verdict: {'PASS' if res['passed'] else 'FAIL'}")
    sys.exit(0 if res["passed"] else 1)

if __name__ == "__main__":
    main()

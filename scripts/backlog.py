#!/usr/bin/env python3
"""
Librova Backlog CLI
Usage:
  backlog.py list   [--priority=<p>] [--milestone=<m>] [--status=<s>]
  backlog.py show   <id>
  backlog.py add    --summary=<s> --type=<t> --priority=<p> [--milestone=<m>] [--note=<n>]
  backlog.py close  <id> --note=<n>
  backlog.py edit   <id> [--summary=<s>] [--status=<s>] [--type=<t>] [--priority=<p>] [--milestone=<m>] [--note=<n>]
  backlog.py next   [--milestone=<m>]
  backlog.py validate
  backlog.py -h | --help

Options:
  --priority=<p>   Critical | Major | Minor | Low
  --status=<s>     Open | Blocked | Needs Reproduction
  --type=<t>       Feature | Bug
  --milestone=<m>  Version string e.g. 1.1, or "unscheduled"
  --summary=<s>    One-line task summary
  --note=<n>       Context or completion note
"""

from __future__ import annotations

import sys
import os
import textwrap
from pathlib import Path
from typing import Optional

try:
    import yaml
except ImportError:
    print("ERROR: pyyaml is not installed. Run: pip install pyyaml", file=sys.stderr)
    sys.exit(1)

# -- Paths -------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT   = SCRIPT_DIR.parent
DOCS_DIR    = REPO_ROOT / "docs"
BACKLOG_FILE  = DOCS_DIR / "backlog.yaml"
ARCHIVE_FILE  = DOCS_DIR / "backlog-archive.yaml"

# -- Constants ----------------------------------------------------------------

VALID_STATUSES    = {"Open", "Blocked", "Needs Reproduction"}
VALID_TYPES       = {"Feature", "Bug"}
VALID_PRIORITIES  = ["Critical", "Major", "Minor", "Low"]
PRIORITY_ORDER    = {p: i for i, p in enumerate(VALID_PRIORITIES)}
ACTIVE_STATUSES   = VALID_STATUSES  # alias for readability

# -- YAML I/O -----------------------------------------------------------------

def _load(path: Path) -> dict:
    if not path.exists():
        die(f"File not found: {path}")
    with open(path, encoding="utf-8") as f:
        return yaml.safe_load(f) or {}


def _save(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        yaml.dump(data, f, allow_unicode=True, sort_keys=False,
                  default_flow_style=False, width=100)


def load_backlog() -> dict:
    return _load(BACKLOG_FILE)


def load_archive() -> dict:
    if not ARCHIVE_FILE.exists():
        return {"tasks": []}
    return _load(ARCHIVE_FILE)


def save_backlog(data: dict) -> None:
    _save(BACKLOG_FILE, data)


def save_archive(data: dict) -> None:
    _save(ARCHIVE_FILE, data)

# -- Helpers ------------------------------------------------------------------

def die(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def find_task(tasks: list[dict], task_id: int) -> Optional[dict]:
    for t in tasks:
        if t["id"] == task_id:
            return t
    return None


def fmt_task(t: dict, wide: bool = False) -> str:
    milestone = t.get("milestone", "unscheduled")
    line = (f"  #{t['id']:<5} [{t['priority']:<8}] [{t['type']:<7}] "
            f"[{t['status']:<18}] [{milestone:<12}]  {t['summary']}")
    if wide and t.get("note"):
        note = textwrap.fill(t["note"], width=90,
                             initial_indent="         ", subsequent_indent="         ")
        line += f"\n{note}"
    return line


def header(title: str) -> str:
    return f"\n{'-'*70}\n  {title}\n{'-'*70}"

# -- Commands -----------------------------------------------------------------

def cmd_list(args: list[str]) -> None:
    opts = parse_opts(args)
    data = load_backlog()
    tasks = data.get("tasks", [])

    pf = opts.get("--priority")
    mf = opts.get("--milestone")
    sf = opts.get("--status")

    if pf:
        pf = pf.capitalize()
        if pf not in VALID_PRIORITIES:
            die(f"Unknown priority: {pf}")
    if sf:
        if sf not in VALID_STATUSES:
            die(f"Unknown status: {sf}")

    filtered = [
        t for t in tasks
        if (not pf or t["priority"] == pf)
        and (not mf or str(t.get("milestone", "unscheduled")) == mf)
        and (not sf or t["status"] == sf)
    ]

    if not filtered:
        print("No tasks match the given filters.")
        return

    by_priority: dict[str, list] = {p: [] for p in VALID_PRIORITIES}
    for t in filtered:
        by_priority[t["priority"]].append(t)

    print(header(f"Open Backlog  ({len(filtered)} tasks)"))
    col = "  #id    [Priority ] [Type   ] [Status             ] [Milestone   ]  Summary"
    print(f"\n{col}")
    print("  " + "-" * 100)
    for prio in VALID_PRIORITIES:
        group = by_priority[prio]
        if group:
            print(f"\n  -- {prio} --")
            for t in group:
                print(fmt_task(t))
    print()


def cmd_show(args: list[str]) -> None:
    if not args:
        die("show requires a task id")
    task_id = parse_id(args[0])
    data = load_backlog()
    t = find_task(data.get("tasks", []), task_id)
    if not t:
        # Try archive
        arch = load_archive()
        t = find_task(arch.get("tasks", []), task_id)
        if t:
            print(f"\n  [ARCHIVED] Task #{task_id}")
        else:
            die(f"Task #{task_id} not found in backlog or archive")
    else:
        print(f"\n  Task #{task_id}")

    print(f"  {'-'*60}")
    print(f"  Summary  : {t['summary']}")
    print(f"  Status   : {t['status']}")
    print(f"  Type     : {t['type']}")
    print(f"  Priority : {t['priority']}")
    print(f"  Milestone: {t.get('milestone', 'unscheduled')}")
    if t.get("note"):
        note = textwrap.fill(t["note"], width=80,
                             initial_indent="             ",
                             subsequent_indent="             ").lstrip()
        print(f"  Note     : {note}")
    print()


def cmd_add(args: list[str]) -> None:
    opts = parse_opts(args)

    summary  = require_opt(opts, "--summary",  "summary is required")
    ttype    = require_opt(opts, "--type",     "type is required (Feature | Bug)")
    priority = require_opt(opts, "--priority", "priority is required (Critical | Major | Minor | Low)")
    milestone = opts.get("--milestone", "unscheduled")
    note      = opts.get("--note", "")

    ttype    = ttype.capitalize()
    priority = priority.capitalize()

    if ttype not in VALID_TYPES:
        die(f"Invalid type '{ttype}'. Must be: {', '.join(VALID_TYPES)}")
    if priority not in VALID_PRIORITIES:
        die(f"Invalid priority '{priority}'. Must be: {', '.join(VALID_PRIORITIES)}")

    data = load_backlog()
    new_id = data["meta"]["last_assigned_id"] + 1
    data["meta"]["last_assigned_id"] = new_id

    task: dict = {
        "id": new_id,
        "summary": summary,
        "status": "Open",
        "type": ttype,
        "priority": priority,
        "milestone": milestone,
        "note": note,
    }

    tasks = data.get("tasks", [])
    # Insert keeping priority order (append at end of same-priority group)
    insert_idx = len(tasks)
    for i, t in enumerate(tasks):
        if PRIORITY_ORDER[t["priority"]] > PRIORITY_ORDER[priority]:
            insert_idx = i
            break
    tasks.insert(insert_idx, task)
    data["tasks"] = tasks
    save_backlog(data)

    print(f"\n  [OK] Added task #{new_id}: {summary}")
    print(f"    Priority: {priority}  |  Type: {ttype}  |  Milestone: {milestone}\n")


def cmd_close(args: list[str]) -> None:
    if not args:
        die("close requires a task id")
    task_id = parse_id(args[0])
    opts = parse_opts(args[1:])
    note = opts.get("--note", "")
    if not note:
        die("--note is required when closing a task (one-sentence summary of what was done)")

    data = load_backlog()
    tasks = data.get("tasks", [])
    task = find_task(tasks, task_id)
    if not task:
        die(f"Task #{task_id} not found in open backlog")

    # Step 1: prepare closed copy
    closed = dict(task)
    closed["status"] = "Closed"
    closed["note"] = note

    # Step 2: append to archive FIRST
    arch = load_archive()
    arch.setdefault("tasks", []).append(closed)
    save_archive(arch)

    # Step 3: delete from backlog
    data["tasks"] = [t for t in tasks if t["id"] != task_id]
    save_backlog(data)

    # Verify
    data2 = load_backlog()
    arch2 = load_archive()
    still_open = find_task(data2.get("tasks", []), task_id)
    in_archive = find_task(arch2.get("tasks", []), task_id)
    if still_open:
        die(f"BUG: #{task_id} still present in backlog after close!")
    if not in_archive:
        die(f"BUG: #{task_id} not found in archive after close!")

    print(f"\n  [OK] Closed task #{task_id}: {task['summary']}")
    print(f"    Note: {note}\n")


def cmd_edit(args: list[str]) -> None:
    if not args:
        die("edit requires a task id")
    task_id = parse_id(args[0])
    opts = parse_opts(args[1:])

    if not opts:
        die("edit requires at least one field to update (--summary, --status, --type, --priority, --milestone, --note)")

    data = load_backlog()
    tasks = data.get("tasks", [])
    task = find_task(tasks, task_id)
    if not task:
        die(f"Task #{task_id} not found in open backlog")

    changes = []
    if "--summary" in opts:
        task["summary"] = opts["--summary"]
        changes.append("summary")
    if "--status" in opts:
        s = opts["--status"]
        if s not in VALID_STATUSES:
            die(f"Invalid status '{s}'. Must be: {', '.join(VALID_STATUSES)}")
        task["status"] = s
        changes.append("status")
    if "--type" in opts:
        t = opts["--type"].capitalize()
        if t not in VALID_TYPES:
            die(f"Invalid type '{t}'")
        task["type"] = t
        changes.append("type")
    if "--priority" in opts:
        p = opts["--priority"].capitalize()
        if p not in VALID_PRIORITIES:
            die(f"Invalid priority '{p}'")
        task["priority"] = p
        changes.append("priority")
    if "--milestone" in opts:
        task["milestone"] = opts["--milestone"]
        changes.append("milestone")
    if "--note" in opts:
        task["note"] = opts["--note"]
        changes.append("note")

    save_backlog(data)
    print(f"\n  [OK] Updated task #{task_id}: {', '.join(changes)} changed\n")


def cmd_next(args: list[str]) -> None:
    opts = parse_opts(args)
    current_milestone = opts.get("--milestone")

    data = load_backlog()
    tasks = data.get("tasks", [])
    open_tasks = [t for t in tasks if t["status"] == "Open"]

    if not open_tasks:
        print("\n  No open tasks in backlog.\n")
        return

    def score(t: dict) -> tuple:
        m = str(t.get("milestone", "unscheduled"))
        if current_milestone:
            # With explicit milestone: prefer match > unscheduled > others
            if m == current_milestone:
                m_score = 0
            elif m == "unscheduled":
                m_score = 1
            else:
                m_score = 2
        else:
            # No milestone filter: treat all milestones equally, sort only by priority
            m_score = 0
        return (m_score, PRIORITY_ORDER[t["priority"]], t["id"])

    candidate = min(open_tasks, key=score)

    print(header("Next Task"))
    print(fmt_task(candidate, wide=True))
    print()


def cmd_validate(args: list[str]) -> None:
    errors: list[str] = []
    warnings: list[str] = []

    data = load_backlog()
    arch = load_archive()

    backlog_tasks = data.get("tasks", [])
    archive_tasks = arch.get("tasks", [])

    backlog_ids = set()
    archive_ids = set()

    # Validate backlog
    for t in backlog_tasks:
        tid = t.get("id")
        if tid is None:
            errors.append("Task missing 'id' field")
            continue
        if tid in backlog_ids:
            errors.append(f"Duplicate id #{tid} in backlog")
        backlog_ids.add(tid)

        for field in ("summary", "status", "type", "priority"):
            if not t.get(field):
                errors.append(f"#{tid}: missing required field '{field}'")

        if t.get("status") not in VALID_STATUSES:
            errors.append(f"#{tid}: invalid status '{t.get('status')}'"
                          f" (valid: {', '.join(VALID_STATUSES)})")
        if t.get("type") not in VALID_TYPES:
            errors.append(f"#{tid}: invalid type '{t.get('type')}'")
        if t.get("priority") not in VALID_PRIORITIES:
            errors.append(f"#{tid}: invalid priority '{t.get('priority')}'")

    # Validate archive
    for t in archive_tasks:
        tid = t.get("id")
        if tid is None:
            errors.append("Archive task missing 'id' field")
            continue
        if tid in archive_ids:
            errors.append(f"Duplicate id #{tid} in archive")
        archive_ids.add(tid)

        if t.get("status") != "Closed":
            errors.append(f"Archive #{tid}: status must be 'Closed', got '{t.get('status')}'")

    # Cross-file duplicates
    overlap = backlog_ids & archive_ids
    for tid in overlap:
        errors.append(f"#{tid} appears in BOTH backlog and archive")

    # last_assigned_id check
    all_ids = backlog_ids | archive_ids
    declared_last = data.get("meta", {}).get("last_assigned_id", 0)
    actual_max = max(all_ids) if all_ids else 0
    if declared_last != actual_max:
        warnings.append(f"last_assigned_id is {declared_last} but highest id seen is {actual_max}")

    # Report
    print(header("Backlog Validation"))
    print(f"\n  Backlog tasks : {len(backlog_tasks)}")
    print(f"  Archive tasks : {len(archive_tasks)}")
    print(f"  Last id       : {declared_last}")

    if errors:
        print(f"\n  [ERROR] {len(errors)} error(s):")
        for e in errors:
            print(f"    - {e}")
    if warnings:
        print(f"\n  [WARN] {len(warnings)} warning(s):")
        for w in warnings:
            print(f"    - {w}")
    if not errors and not warnings:
        print("\n  [OK] All checks passed\n")
    else:
        print()
        sys.exit(1)

# -- Argument parsing (no external deps) --------------------------------------

def parse_opts(args: list[str]) -> dict[str, str]:
    """Parse --key=value or --key value pairs."""
    opts: dict[str, str] = {}
    i = 0
    while i < len(args):
        a = args[i]
        if "=" in a and a.startswith("--"):
            k, v = a.split("=", 1)
            opts[k] = v
        elif a.startswith("--") and i + 1 < len(args) and not args[i+1].startswith("--"):
            opts[a] = args[i + 1]
            i += 1
        i += 1
    return opts


def parse_id(s: str) -> int:
    s = s.lstrip("#")
    try:
        return int(s)
    except ValueError:
        die(f"Invalid task id: {s!r}")


def require_opt(opts: dict, key: str, msg: str) -> str:
    v = opts.get(key)
    if not v:
        die(msg)
    return v

# -- Entry point ---------------------------------------------------------------

COMMANDS = {
    "list":     cmd_list,
    "show":     cmd_show,
    "add":      cmd_add,
    "close":    cmd_close,
    "edit":     cmd_edit,
    "next":     cmd_next,
    "validate": cmd_validate,
}


def main() -> None:
    args = sys.argv[1:]
    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        return

    cmd = args[0]
    if cmd not in COMMANDS:
        die(f"Unknown command '{cmd}'. Available: {', '.join(COMMANDS)}")

    COMMANDS[cmd](args[1:])


if __name__ == "__main__":
    main()

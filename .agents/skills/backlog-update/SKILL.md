---
name: backlog-update
description: "Guide for all backlog management operations in Librova: adding new tasks, taking tasks into work, closing tasks, and maintaining the archive. Use whenever adding, modifying, or closing a backlog item."
---

# Backlog Management Playbook

## Goal

Keep `docs/backlog.yaml` and `docs/backlog-archive.yaml` consistent by using `scripts/backlog.py` only.

## When to Use

- use this skill when adding, editing, validating, or closing a backlog item
- use this skill when you need to inspect the next candidate task from the backlog
- do **not** use this skill for session-local todo tracking; use the session todo system for that

## Core Rules

- Never edit backlog YAML directly.
- Keep tasks `Open` while you work; the backlog has no `In Progress` status.
- Run `python scripts/backlog.py validate` after add / close operations and after any suspicious backlog state.

## Storage Layout

Two YAML files make up the Librova backlog:

- `docs/backlog.yaml` — active tasks: status `Open`, `Needs Reproduction`, or `Blocked`
- `docs/backlog-archive.yaml` — all `Closed` tasks

## Task Schema

```yaml
id:        <integer>          # assigned automatically by CLI
summary:   <string>           # one-line description
status:    Open | Needs Reproduction | Blocked
type:      Feature | Bug
priority:  Critical | Major | Minor | Low
milestone: "1.0" | "1.1" | unscheduled
note:      <string>           # context or one-sentence completion summary
```

**Field order in YAML**: `id` -> `summary` -> `status` -> `type` -> `priority` -> `milestone` -> `note`

## CLI Reference

All commands are run from the repository root:

```powershell
python scripts/backlog.py <command> [options]
```

| Command | Purpose |
|---|---|
| `list` | Show open backlog, optionally filtered |
| `show <id>` | Show full details for one task (searches archive too) |
| `add` | Create a new task |
| `close <id> --note="..."` | Move task to archive with a completion note |
| `edit <id>` | Update fields on an open task |
| `next [--milestone=<m>]` | Pick the highest-priority open task |
| `validate` | Run integrity checks on both files |

### Common Options

```powershell
--priority=Critical|Major|Minor|Low
--status=Open|Blocked|Needs\ Reproduction
--type=Feature|Bug
--milestone=1.1
--summary="..."
--note="..."
```

## Recommended Workflows

### Inspect backlog state

1. Run `python scripts/backlog.py list`.
2. If one item matters, run `python scripts/backlog.py show <id>`.
3. If you need a candidate task, run `python scripts/backlog.py next --milestone=<m>`.

### Add a new task

```powershell
python scripts/backlog.py add `
  --summary="short one-line description" `
  --type=Feature `
  --priority=Major `
  --milestone=1.1 `
  --note="context or acceptance criteria"
```

The CLI assigns the next id automatically and updates `meta.last_assigned_id`. Do not compute ids manually.

### Edit an existing task

```powershell
python scripts/backlog.py edit <id> --status=Blocked --note="waiting on X"
python scripts/backlog.py edit <id> --priority=Critical
```

Only the passed fields change; all others stay untouched.

### Close a task

```powershell
python scripts/backlog.py close <id> --note="one sentence: what was actually done"
```

The tool:

1. builds the closed entry in memory
2. appends it to `docs/backlog-archive.yaml`
3. removes it from `docs/backlog.yaml`
4. verifies the id is absent from the backlog and present exactly once in the archive

The note must describe what was actually done, not restate the original summary.

### Validate backlog integrity

```powershell
python scripts/backlog.py validate
```

Checks performed:

- every task has all required fields with valid enum values
- no id appears in both files
- `meta.last_assigned_id` equals the highest id across both files
- archive contains only `Closed` tasks; backlog contains no `Closed` tasks

## Priority Guide

| Priority | When to use |
|---|---|
| `Critical` | Breaks a core user flow or leaves the current release scope functionally incomplete |
| `Major` | Strong UX or functional defect that noticeably hurts day-to-day use |
| `Minor` | Local polish or visual consistency work; no broken flow |
| `Low` | Useful but least urgent polish |

## Done Criteria

A backlog item may be closed only when:

- the behavior is actually changed in code or documentation, as appropriate
- automated tests are added where practical
- repository docs are updated according to `AGENTS.md` document-maintenance policy
- required manual verification is completed for UI-sensitive work

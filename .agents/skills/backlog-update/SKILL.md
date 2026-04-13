---
name: backlog-update
description: "Guide for all backlog management operations in Librova: adding new tasks, taking tasks into work, closing tasks, and maintaining the archive. Use whenever adding, modifying, or closing a backlog item."
---

# Backlog Management Playbook

## Storage layout

Two YAML files make up the Librova backlog:

- `docs/backlog.yaml` — active tasks: status `Open`, `Needs Reproduction`, or `Blocked`
- `docs/backlog-archive.yaml` — all `Closed` tasks

A CLI tool manages both files. **Never edit the YAML directly.** Always go through `scripts/backlog.py`.

---

## Task schema

```yaml
id:        <integer>          # assigned automatically by CLI
summary:   <string>           # one-line description
status:    Open | Needs Reproduction | Blocked
type:      Feature | Bug
priority:  Critical | Major | Minor | Low
milestone: "1.0" | "1.1" | unscheduled   # optional; omit only when deliberately undefined
note:      <string>           # context or one-sentence completion summary
```

**Field order in YAML**: `id` → `summary` → `status` → `type` → `priority` → `milestone` → `note`

---

## CLI reference

All commands are run from the **repository root**:

```
python scripts/backlog.py <command> [options]
```

| Command | Purpose |
|---------|---------|
| `list` | Show open backlog, optionally filtered |
| `show <id>` | Full details for one task (searches archive too) |
| `add` | Create a new task |
| `close <id> --note="…"` | Move task to archive with a completion note |
| `edit <id>` | Update fields on an open task |
| `next [--milestone=<m>]` | Pick the highest-priority open task |
| `validate` | Run integrity checks on both files |

### Common options

```
--priority=Critical|Major|Minor|Low
--status=Open|Blocked|Needs\ Reproduction
--type=Feature|Bug
--milestone=1.1          # or: unscheduled
--summary="…"
--note="…"
```

---

## Choosing the next task

```
python scripts/backlog.py next --milestone=1.1
```

When `--milestone` is given the tool prefers tasks matching that milestone, falls back to `unscheduled`, then ignores milestone and sorts purely by priority. Within a priority tier tasks are ordered by id (oldest first). Tasks with status `Blocked` or `Needs Reproduction` are treated as Open by the tool — skip them manually if needed and pick the next one.

---

## Adding a new task

```
python scripts/backlog.py add \
  --summary="short one-line description" \
  --type=Feature \
  --priority=Major \
  --milestone=1.1 \
  --note="context or acceptance criteria"
```

The CLI assigns the next id automatically and updates `meta.last_assigned_id`. Do not compute the id manually.

**Priority guide**

| Priority | When to use |
|----------|-------------|
| `Critical` | Breaks a core user flow or leaves the current release scope functionally incomplete |
| `Major` | Strong UX or functional defect that noticeably hurts day-to-day use |
| `Minor` | Local polish or visual consistency work; no broken flow |
| `Low` | Useful but least urgent polish |

---

## Closing a task

```
python scripts/backlog.py close <id> --note="one sentence: what was actually done"
```

The tool performs the three steps in the required order:
1. Builds the closed entry in memory.
2. Appends it to `docs/backlog-archive.yaml`.
3. Removes it from `docs/backlog.yaml`.

Then it verifies the result: the id must be absent from the backlog and present exactly once in the archive. The command exits with a non-zero code if either check fails.

**The note must describe what was done, not what the task said.** One sentence is enough.

---

## Editing an open task

```
python scripts/backlog.py edit <id> --status=Blocked --note="waiting on X"
python scripts/backlog.py edit <id> --priority=Critical
```

Only the fields you pass are changed. All other fields are preserved.

---

## Validation

```
python scripts/backlog.py validate
```

Checks performed:
- Every task has all required fields with valid enum values.
- No id appears in both files.
- `meta.last_assigned_id` equals the highest id across both files.
- Archive contains only `Closed` tasks; backlog contains no `Closed` tasks.

Run this after any manual intervention. The CI pipeline should run it on every push.

---

## Taking a task into work

Do not change status to "In Progress" or similar — there is no such status. Keep the task `Open` and start working. If something blocks you, use:

```
python scripts/backlog.py edit <id> --status=Blocked --note="reason"
```

---

## Done criteria

A task may be closed only when:

- The behaviour is actually changed in code.
- Automated tests are added where practical.
- Documentation is updated if user-visible behaviour or working rules changed.
- The user confirms manual verification when the task involves UI or manual-test remarks.

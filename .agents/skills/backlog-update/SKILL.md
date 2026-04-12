---
name: backlog-update
description: "Guide for all backlog management operations in Librova: adding new tasks, taking tasks into work, closing tasks, and maintaining the archive. Use whenever adding, modifying, or closing a backlog item."
---

# Backlog Management Playbook

Two files make up the Librova backlog:

- `docs/Librova-Backlog.md` — active tasks only: status `Open`, `Needs Reproduction`, or `Blocked`
- `docs/Librova-Backlog-Archive.md` — all `Closed` tasks, organized by priority section

---

## Task Format

Every task entry is four required lines plus one optional line:

```
- `#<id>` <summary>
  - Status: `<status>`
  - Type: `<type>`
  - Milestone: `<version>`
  - Note: <note text>
```

**Status values**: `Open` · `Needs Reproduction` · `Blocked` · `Closed`  
**Type values**: `Feature` · `Bug`  
**Milestone values**: a version string such as `1.0` or `1.1`, or the literal `unscheduled`  
**Priority sections** (used in both files): `Critical` → `Major` → `Minor` → `Low`

`Milestone` is optional. When omitted, the task is treated as `unscheduled`. When present, it must appear between `Type` and `Note`.

---

## Choosing the Next Task

When the user says "next task" or "take the next task":

1. Filter to `Open` tasks whose `Milestone` matches the current active release (e.g. `1.0`).
2. Among those, pick the highest-priority one: `Critical` → `Major` → `Minor` → `Low`.
3. If no tasks with the current milestone remain, fall back to `unscheduled` tasks in the same priority order.
4. Never pick tasks milestoned to a future release (e.g. `1.1`) while the current release is still open.
5. If the top candidate is `Needs Reproduction` or `Blocked`, skip it and take the next qualifying task.
6. Among equal-priority tasks, prefer the most local and easiest-to-verify one first.

---

## 1. Adding a New Task

- [ ] Read `Last assigned id: #N` from `docs/Librova-Backlog.md`.
- [ ] Use N+1 as the new task id; update `Last assigned id` to N+1 in the same change.
- [ ] Choose priority: `Critical` / `Major` / `Minor` / `Low` (see Priority Guide below).
- [ ] Choose type: `Feature` (new capability or improvement) / `Bug` (deviation from expected behavior).
- [ ] Choose milestone: assign the target release version (`1.0`, `1.1`, …) or `unscheduled` if not yet decided. Omit the field entirely only for tasks where milestone is deliberately undefined.
- [ ] Insert the entry in the correct priority subsection under `## 3. Open Backlog`.
- [ ] Write a `Note` that explains context or acceptance criteria concisely.

---

## 2. Taking a Task Into Work

- Do **not** move, duplicate, or rename the task entry yet.
- Keep it in `Open Backlog` with its current status.
- If reprioritization is intentional, move it to a different priority section in the same change and update the Note to explain the reason.

---

## 3. Closing a Task

**The close operation is EXACTLY three steps, in this order. Do not skip or reorder.**

**Step 1 — Prepare the closed entry** (do this in memory, not yet in any file):
- Write a one-sentence `Note` summarizing what was actually done (no file-by-file detail).
- Set `Status` to `Closed`.
- Keep all other fields (`Type`, `Milestone`) unchanged.

**Step 2 — Append to archive FIRST** (write to `docs/Librova-Backlog-Archive.md`):
- Append the full closed entry (title + all sub-fields) to the matching priority section (`### Critical` / `### Major` / `### Minor` / `### Low`).
- Preserve the `Milestone` field.

**Step 3 — Delete from open backlog** (write to `docs/Librova-Backlog.md`):
- Remove the ENTIRE entry — title line and all sub-field lines — from `docs/Librova-Backlog.md`.
- ⚠️ **Do NOT just change the status to `Closed` and leave the entry in place. The entry must be physically deleted from this file.**
- ⚠️ **Do NOT leave the entry in `docs/Librova-Backlog.md` with `Status: Closed`. That is an invalid state.**

**Immediate verification after close:**
- [ ] `grep "#<id>" docs/Librova-Backlog.md` returns no results.
- [ ] `grep "#<id>" docs/Librova-Backlog-Archive.md` returns exactly one result.

---

## 4. Validation Checklist

After any backlog edit, confirm all of the following:

- [ ] Every task has the required fields in the correct order: title line → `Status` → `Type` → `Note`. The optional `Milestone` field, when present, appears between `Type` and `Note`.
- [ ] No orphan title-only lines without the required sub-fields.
- [ ] No id appears in both `Librova-Backlog.md` and `Librova-Backlog-Archive.md`.
- [ ] `Last assigned id` in `Librova-Backlog.md` equals the highest id ever assigned across both files.
- [ ] `Open Backlog` contains only tasks with status `Open`, `Needs Reproduction`, or `Blocked`.
- [ ] Archive contains only tasks with status `Closed`.
- [ ] `Milestone` values, when present, are one of: a version string (e.g. `1.0`, `1.1`) or the literal `unscheduled`.

---

## 5. Priority Guide

| Priority | When to use |
|----------|-------------|
| `Critical` | Breaks a core user flow or leaves the current release scope functionally incomplete |
| `Major` | Strong UX or functional defect that noticeably hurts day-to-day use |
| `Minor` | Local polish or visual consistency work; no broken flow |
| `Low` | Useful but least urgent polish |

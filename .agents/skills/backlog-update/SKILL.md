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

Every task entry is exactly four lines:

```
- `#<id>` <summary>
  - Status: `<status>`
  - Type: `<type>`
  - Note: <note text>
```

**Status values**: `Open` · `Needs Reproduction` · `Blocked` · `Closed`  
**Type values**: `Feature` · `Bug`  
**Priority sections** (used in both files): `Critical` → `Major` → `Minor` → `Low`

---

## Choosing the Next Task

When the user says "next task" or "take the next task":

1. Take the highest-priority task with status `Open`.
2. If the top candidate is `Needs Reproduction` or `Blocked`, skip it and take the next `Open` task.
3. Among equal-priority tasks, prefer the most local and easiest-to-verify one first.

---

## 1. Adding a New Task

- [ ] Read `Last assigned id: #N` from `docs/Librova-Backlog.md`.
- [ ] Use N+1 as the new task id; update `Last assigned id` to N+1 in the same change.
- [ ] Choose priority: `Critical` / `Major` / `Minor` / `Low` (see Priority Guide below).
- [ ] Choose type: `Feature` (new capability or improvement) / `Bug` (deviation from expected behavior).
- [ ] Insert the four-line entry in the correct priority subsection under `## 3. Open Backlog`.
- [ ] Write a `Note` that explains context or acceptance criteria concisely.

---

## 2. Taking a Task Into Work

- Do **not** move, duplicate, or rename the task entry yet.
- Keep it in `Open Backlog` with its current status.
- If reprioritization is intentional, move it to a different priority section in the same change and update the Note to explain the reason.

---

## 3. Closing a Task

- [ ] Write a one-sentence `Note` summarizing what was actually done (no file-by-file detail, no exhaustive list of changes).
- [ ] Set `Status` to `Closed`.
- [ ] Remove the full four-line entry from `docs/Librova-Backlog.md`.
- [ ] Append the entry to the matching priority section (`### Critical` / `### Major` / `### Minor` / `### Low`) in `docs/Librova-Backlog-Archive.md`.
- [ ] Verify the task id is no longer present in `docs/Librova-Backlog.md`.

---

## 4. Validation Checklist

After any backlog edit, confirm all of the following:

- [ ] Every task has all four fields in the correct order: title line → `Status` → `Type` → `Note`.
- [ ] No orphan title-only lines without the three sub-fields.
- [ ] No id appears in both `Librova-Backlog.md` and `Librova-Backlog-Archive.md`.
- [ ] `Last assigned id` in `Librova-Backlog.md` equals the highest id ever assigned across both files.
- [ ] `Open Backlog` contains only tasks with status `Open`, `Needs Reproduction`, or `Blocked`.
- [ ] Archive contains only tasks with status `Closed`.

---

## 5. Priority Guide

| Priority | When to use |
|----------|-------------|
| `Critical` | Breaks a core user flow or leaves the current release scope functionally incomplete |
| `Major` | Strong UX or functional defect that noticeably hurts day-to-day use |
| `Minor` | Local polish or visual consistency work; no broken flow |
| `Low` | Useful but least urgent polish |

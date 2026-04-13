# Librova — Execution Plan Template (ExecPlan)

This file defines the ExecPlan format for complex Librova tasks that span multiple sessions
or cross multiple layers. Codex should create a task-specific plan file for any non-trivial
feature or stabilization effort, following this skeleton.

> **When to create an ExecPlan:** Any task that touches more than one architectural layer,
> requires multiple sessions, or has explicit "done when" criteria that span C++ and C# sides.

---

## ExecPlan Skeleton

Copy this template to `.agent/plans/<task-name>.md` and fill in the sections.

```markdown
# ExecPlan: <Task Name>

## Scope
What is in scope. What is explicitly out of scope.

## Why It Matters
Why this task closes a real backlog gap (reference the backlog item id, e.g. `python scripts/backlog.py show <id>`).

## Steps

### Phase 1: <Phase Name>
- [ ] Step 1
- [ ] Step 2

### Phase 2: <Phase Name>
- [ ] Step 1
- [ ] Step 2

## Done When
Concrete, verifiable criteria. Examples:
- Debug and Release build pass
- Native tests (Catch2) green
- Managed tests (xUnit) green
- scripts/ValidateProto.ps1 passes
- Manual UI scenario X walks through successfully
- docs/ updated to match implemented reality

## Related Files
- docs/backlog.yaml (item id: ...)
- docs/Librova-Architecture.md (section: ...)
```

---

## Example: Series and Genres ExecPlan

```markdown
# ExecPlan: Series and Genres End-to-End

## Scope
Add series and genres as first-class metadata:
parser output → persistence → transport contracts → browser filter → details panel.
Out of scope: ratings, shelves, reading lists.

## Why It Matters
Closes the open "series and genres" backlog item (see `python scripts/backlog.py show <id>`).

## Steps

### Phase 1: Parser and Persistence
- [ ] Extend FB2 and EPUB parsers to extract series/genres fields
- [ ] Extend SQLite schema (additive migration)
- [ ] Update FTS5 index if genres need text search

### Phase 2: Transport
- [ ] Follow $transport-rpc skill to add new proto fields and pipe methods
- [ ] scripts/ValidateProto.ps1 passes

### Phase 3: UI
- [ ] Extend ViewModel for details panel
- [ ] Add genre/series filter to browser
- [ ] Update ManualUiTestScenarios.md (Russian)

## Done When
- Debug and Release build pass
- All Catch2 and xUnit tests green
- Genre and series data visible in details panel
- Browser filter by series/genre works end-to-end
- Manual UI scenario walked through

## Related Files
- docs/backlog.yaml (open item: series and genres)
- docs/Librova-Architecture.md (Section 5: Search Model)
```

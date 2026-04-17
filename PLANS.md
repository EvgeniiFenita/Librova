# Librova — Execution Plan Template

This file defines the plan shape for complex Librova work that spans multiple phases,
multiple sessions, or multiple architectural layers.

Use it as a **template reference** only.

- For an active Copilot / Codex task, keep the live plan in the session `plan.md`.
- Do **not** create repository markdown planning files for ordinary task execution unless the user explicitly asks for one.
- Do **not** use the plan as a status tracker; operational task state belongs in the backlog and session todo tracking.

> **When to create a plan:** Any task that touches more than one architectural layer, requires multiple sessions, or has explicit "done when" criteria spanning both native and managed sides.

---

## Recommended Structure

```markdown
# Plan: <Task Name>

## Problem
What is changing, why it matters, and what is explicitly out of scope.

## Backlog Link
Reference the active backlog item, for example:
`python scripts/backlog.py show <id>`

## Approach
Short description of the intended implementation direction.

## Phases

### Phase 1: <Name>
- outcome
- key files or modules

### Phase 2: <Name>
- outcome
- key files or modules

## Done When
Concrete verification criteria, for example:
- Debug and Release build pass
- Native tests green
- Managed tests green
- `scripts/ValidateProto.ps1` passes after proto changes
- relevant manual UI scenario completed when the workflow is user-visible
- docs match implemented reality

## References
- `docs/CodebaseMap.md`
- `docs/Librova-Architecture.md`
- `docs/engineering/TestStrategy.md`
- relevant skill(s)
```

---

## Example

```markdown
# Plan: Series And Genres End-To-End

## Problem
Expose series and genres from parser output through persistence, transport, and UI filtering.
Out of scope: ratings, shelves, reading lists.

## Backlog Link
`python scripts/backlog.py show <id>`

## Approach
Implement a single vertical slice: parser output -> database -> proto -> UI filters/details.

## Phases

### Phase 1: Parser And Persistence
- extend FB2 and EPUB parsers
- extend SQLite schema and repositories
- update any FTS coverage if required

### Phase 2: Transport
- follow `$transport-rpc` for new proto fields and transport plumbing
- regenerate / validate proto changes

### Phase 3: UI
- extend ViewModels and binding
- update the relevant file under `docs/manual-tests/`
- add the registry row in `docs/ManualUiTestScenarios.md`

## Done When
- Debug and Release build pass
- Catch2 and xUnit tests green
- series and genres visible in `Book Details`
- browser filtering works end-to-end
- documentation matches the implementation

## References
- `docs/backlog.yaml`
- `docs/CodebaseMap.md`
- `docs/Librova-Architecture.md`
```

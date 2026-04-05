# Librova — Agent Instructions

## Project Snapshot

Librova is a Windows-first desktop e-book library manager.  
Two-process architecture: `Librova.UI` (C# / .NET / Avalonia) ↔ `Librova.Core` (C++20).  
IPC: Protobuf over Windows named pipes. Storage: SQLite + FTS5. Build: CMake + vcpkg (native), .csproj (managed).

Full product, architecture, and backlog context lives in `docs/`. Read them before any substantive change.

Repository-level overview lives in `README.md`. Keep it aligned with the current implemented project state.

---

## Mandatory Read Order

Before making changes, read these documents in order:

1. `docs/Librova-Product.md` — what the product is and what is in scope
2. `docs/Librova-Backlog.md` — active backlog and task priorities
3. `docs/engineering/CommitMessageGuidelines.md` — commit format
4. `docs/engineering/TestStrategy.md` — what tests to add and when
5. `docs/engineering/TransportInvariants.md` — IPC contract rules
6. `docs/UiDesignSystem.md` — UI design system: colour tokens, typography, components, shell layout (read before any UI change)

When writing new C++, C#, Protobuf, or script code: use the `$code-style` skill.  
Architecture reference (frozen decisions, read when a structural question is not answered by Hard Constraints below): `docs/Librova-Architecture.md`.

---

## Build and Test

Run the full build and test suite (native + managed, `Debug` by default):

```powershell
.\Run-Tests.ps1
```

Individual steps:

```powershell
# Configure and build native (C++)
cmake --preset x64-debug
cmake --build --preset x64-debug --config Debug

# Run native tests
ctest --test-dir out\build\x64-debug -C Debug --output-on-failure

# Build and run managed tests
dotnet test tests\Librova.UI.Tests\Librova.UI.Tests.csproj -c Debug

# Proto validation — required after any change under proto/
scripts\ValidateProto.ps1

# Build and launch the app (for manual verification)
.\Run-Librova.ps1
.\Run-Librova.ps1 -FirstRun    # first-run setup screen
```

> `build → test` must always be **sequential** — never run tests against a stale build.

---

## Hard Constraints (Never Violate)

- MVP targets **Windows only**.
- Architecture is **two-process**: UI in C# / Avalonia, core in C++20. Do not collapse into one process.
- IPC is **Protobuf over named pipes**. Do not introduce gRPC runtime or P/Invoke as the main transport.
- **One user, one managed library, offline-first.** No cloud, no multi-library in MVP.
- **Unicode and Cyrillic correctness** are non-negotiable everywhere: storage, search, UI, and logs.
- All build artifacts go under repository-root `out/`. No project-local `bin/` or `obj/` as steady state.
- CMake is the **canonical native build system**. Visual Studio solutions are for developer convenience only.
- Do not introduce gRPC runtime dependencies without an explicit architecture decision.
- Conversion cancellation is **not** ordinary converter failure; never silently fall back to storing the original FB2.
- `build → test` must be **sequential**, never parallel when tests depend on freshly built binaries.
- After any change under `proto/`, run `scripts/ValidateProto.ps1` before marking the checkpoint done.

---

## Working Rules

- Check the actual repository state before assuming structure.
- Do not invent architecture that conflicts with frozen decisions in `docs/Librova-Architecture.md`.
- Fix the true cause of a bug whenever it is reasonably reachable in the current task; do not stop at a local patch that only hides downstream symptoms while leaving the source inconsistency in place.
- Keep domain logic out of Avalonia views and transport DTOs.
- Prefer small vertical slices that preserve clean layer boundaries.
- Map every new task to an open backlog item in `docs/Librova-Backlog.md` before starting.
- For all backlog operations — adding tasks, taking into work, closing, and validating — follow the `$backlog-update` skill.
- When implementation state and `docs/Librova-Backlog.md` diverge, update the backlog in the same task instead of leaving stale statuses behind.
- Do not start convenience or side-feature work unless it directly closes an active backlog item.
- Finish one end-to-end vertical slice before branching into adjacent polish.
- For native code: one static library per logical slice under `libs/<SliceName>/` with a local `CMakeLists.txt`.
- In native libraries: keep `.hpp` and `.cpp` together unless a different layout is clearly necessary.
- Disposable runtime files, logs, and transient state go under `out/` rather than scattered across source.
- If SQLite schema depends on optional modules (e.g., FTS5), declare them explicitly in `vcpkg.json`.
- External converter integration stays user-configurable; `fb2cng` is the first built-in profile, not a hard-wired exclusive.
- Process-level IPC tests must use explicit readiness checks and deterministic cleanup — no fixed sleeps.
- Important execution paths in both C++ and C# must emit actionable logs through the repository logging facade.
- If a bug fix reveals duplicated infrastructure helpers for Unicode, path safety, encoding conversion, or resource ownership, consolidate them in the same task instead of patching copies independently.
- Native UTF-8, wide-string, and `std::filesystem::path` conversions must go through `libs/Unicode/UnicodeConversion.*`; do not add ad-hoc `WideCharToMultiByte`, `MultiByteToWideChar`, `generic_u8string`, or duplicate path-conversion helpers outside that shared slice.
- When closing a review-pass issue, add a regression test for the exact failure mode before marking the checkpoint done.
- When the user asks for a code review or review-pass style findings, write the review results in Russian by default.

---

## Implementation Priorities

When tradeoffs appear, prefer in this order:

1. Correctness
2. Crash safety
3. Explicit boundaries
4. Testability
5. Developer convenience

Do not trade away Unicode correctness, rollback safety, or contract clarity for speed.

---

## Document Maintenance

When a task completes, update docs **in the same task** if any of the following changed:

| What changed | Update these files |
|---|---|
| Product scope or user-visible behavior | `docs/Librova-Product.md` |
| Architecture decision | `docs/Librova-Architecture.md` |
| Backlog item closed, added, or reprioritized | `docs/Librova-Backlog.md`; closed items appended to `docs/Librova-Backlog-Archive.md` via `$backlog-update` skill (append-only; do not load archive as upfront context) |
| Repository overview or project-status summary visible from the root | `README.md` |
| New convention or constraint | `AGENTS.md` (this file) |
| Verified checkpoint reached | no archive checkpoint document is maintained |
| UI workflow changed (user-visible) | `docs/ManualUiTestScenarios.md` — Russian; `## N. Feature` sections, numbered steps, `Ожидаемое поведение:` blocks, UI labels in English as-is; append new sections at end; do not load as upfront context |
| UI styles, colours, components, or layout changed | `docs/UiDesignSystem.md` |

`docs/ManualUiTestScenarios.md` must be written in **Russian**. UI labels, button names, and on-screen strings must appear exactly as they are in the English UI.

If a file stops matching implemented reality, fix it before closing the task — not later.

This applies to `README.md` as well: keep it current when project scope, architecture summary, or top-level usage framing changes.

---

## Commit Discipline

- **Never commit** unless explicitly requested by the user.
- When a commit is requested, follow `docs/engineering/CommitMessageGuidelines.md`.
- Prefer one coherent commit per logical change.
- Stage only the intended files.

---

## Skills Available

Use these skills for common recurring workflows (type `$` in the CLI to pick a skill):

- `$backlog-update` — add, close, or validate backlog tasks
- `$code-style` — project-specific naming and formatting rules for C++, C#, proto, Python (full reference in `docs/engineering/CodeStyleGuidelines.md`)
- `$vertical-slice` — end-to-end checklist for any new feature
- `$transport-rpc` — step-by-step guide for adding a new IPC method
- `$epub-import` — import pipeline extension checklist
- `$review-pass` — pre-release review and verification checklist

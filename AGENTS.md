# Librova — Agent Instructions

## Project Snapshot

Librova is a Windows-first desktop e-book library manager.  
Two-process architecture: `Librova.UI` (C# / .NET / Avalonia) ↔ `Librova.Core` (C++20).  
IPC: Protobuf over Windows named pipes. Storage: SQLite + FTS5. Build: CMake + vcpkg (native), .csproj (managed).

Full product, architecture, and roadmap context lives in `docs/`. Read them before any substantive change.

---

## Mandatory Read Order

Before making changes, read these documents in order:

1. `docs/Librova-Product.md` — what the product is and what is in scope
2. `docs/Librova-Architecture.md` — frozen architectural decisions
3. `docs/Librova-Roadmap.md` — active MVP buckets and release criteria
4. `docs/engineering/CodeStyleGuidelines.md` — naming and style rules
5. `docs/engineering/CommitMessageGuidelines.md` — commit format
6. `docs/engineering/TestStrategy.md` — what tests to add and when
7. `docs/engineering/TransportInvariants.md` — IPC contract rules

Use `docs/archive/` only when the active documents are insufficient and historical detail is genuinely needed.

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
- Keep domain logic out of Avalonia views and transport DTOs.
- Prefer small vertical slices that preserve clean layer boundaries.
- Map every new task to an open roadmap bucket in `docs/Librova-Roadmap.md` before starting.
- Do not start convenience or side-feature work unless it directly closes an active roadmap item.
- Finish one end-to-end vertical slice before branching into adjacent polish.
- For native code: one static library per logical slice under `libs/<SliceName>/` with a local `CMakeLists.txt`.
- In native libraries: keep `.hpp` and `.cpp` together unless a different layout is clearly necessary.
- Disposable runtime files, logs, and transient state go under `out/` rather than scattered across source.
- If SQLite schema depends on optional modules (e.g., FTS5), declare them explicitly in `vcpkg.json`.
- External converter integration stays user-configurable; `fb2cng` is the first built-in profile, not a hard-wired exclusive.
- Process-level IPC tests must use explicit readiness checks and deterministic cleanup — no fixed sleeps.
- Important execution paths in both C++ and C# must emit actionable logs through the repository logging facade.

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
| Roadmap bucket closed or added | `docs/Librova-Roadmap.md` |
| New convention or constraint | `AGENTS.md` (this file) |
| Verified checkpoint reached | `docs/archive/Development-History.md` |
| UI workflow changed (user-visible) | `docs/ManualUiTestScenarios.md` |

`docs/ManualUiTestScenarios.md` must be written in **Russian**. UI labels, button names, and on-screen strings must appear exactly as they are in the English UI.

If a file stops matching implemented reality, fix it before closing the task — not later.

---

## Commit Discipline

- **Never commit** unless explicitly requested by the user.
- When a commit is requested, follow `docs/engineering/CommitMessageGuidelines.md`.
- Prefer one coherent commit per logical change.
- Stage only the intended files.

---

## Skills Available

Use these skills for common recurring workflows (type `$` in the CLI to pick a skill):

- `$vertical-slice` — end-to-end checklist for any new feature
- `$transport-rpc` — step-by-step guide for adding a new IPC method
- `$epub-import` — import pipeline extension checklist
- `$review-pass` — pre-release review and verification checklist

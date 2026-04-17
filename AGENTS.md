# Librova — Agent Instructions

## Project Snapshot

Librova is a Windows-first desktop e-book library manager.  
Two-process architecture: `Librova.UI` (C# / .NET / Avalonia) ↔ `Librova.Core` (C++20).  
IPC: Protobuf over Windows named pipes. Storage: SQLite + FTS5. Build: CMake + vcpkg (native), .csproj (managed).

Full product, architecture, and backlog context lives in `docs/`. Read them before any substantive change.

Repository-level overview lives in `README.md`. Keep it aligned with the current implemented project state.

---

## Documentation Model

Treat the documentation set as a layered system:

- `README.md` — navigation hub: which document to read for which task
- `AGENTS.md` — global rules, workflow policy, and document-maintenance ownership
- `docs/Librova-Product.md`, `docs/Librova-Architecture.md`, `docs/CodebaseMap.md`, `docs/engineering/*` — reference docs
- `.agents/skills/*/SKILL.md` — task procedures and checklists
- local `AGENTS.md` files under subfolders — local context only; they should defer to this file for global policy

When two docs overlap, prefer the more specific authoritative reference and remove duplicated wording instead of maintaining parallel copies.

---

## Mandatory Read Order

Before making changes, read these documents in order:

1. `docs/Librova-Product.md` — what the product is and what is in scope
2. `docs/CodebaseMap.md` — module map and task navigation (fast orientation: where things live, where to go for a given change)
3. `docs/backlog.yaml` — active backlog; use `python scripts/backlog.py list` to view
4. Use `$commit-message` skill for commit format
5. `docs/engineering/TestStrategy.md` — what tests to add and when
6. `docs/engineering/TransportInvariants.md` — IPC contract rules
7. `docs/UiDesignSystem.md` — UI design system: colour tokens, typography, components, shell layout (read before any UI change)

Use `$skill-name` in the CLI whenever this file says to use a skill.

When writing new C++, C#, Protobuf, or script code: use the `$code-style` skill.  
Architecture reference (frozen decisions, read when a structural question is not answered by Hard Constraints below): `docs/Librova-Architecture.md`.

Before changing a user-visible UI workflow, also read `docs/ManualUiTestScenarios.md` and the relevant file under `docs/manual-tests/`.

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

### Repository discipline

- Check the actual repository state before assuming structure.
- Map every new task to an open backlog item in `docs/backlog.yaml` before starting (use `python scripts/backlog.py list` or `python scripts/backlog.py add …`).
- For all backlog operations — adding tasks, taking into work, closing, and validating — follow the `$backlog-update` skill.
- When implementation state and `docs/backlog.yaml` diverge, update the backlog in the same task instead of leaving stale statuses behind.
- Do not start convenience or side-feature work unless it directly closes an active backlog item.
- Finish one end-to-end vertical slice before branching into adjacent polish.
- Disposable runtime files, logs, and transient state go under `out/` rather than scattered across source.

### Architecture and boundaries

- Do not invent architecture that conflicts with frozen decisions in `docs/Librova-Architecture.md`.
- Fix the true cause of a bug whenever it is reasonably reachable in the current task; do not stop at a local patch that only hides downstream symptoms while leaving the source inconsistency in place.
- Keep domain logic out of Avalonia views and transport DTOs.
- Prefer small vertical slices that preserve clean layer boundaries.
- For native code: one static library per logical slice under `libs/<SliceName>/` with a local `CMakeLists.txt`.
- In native libraries: keep `.hpp` and `.cpp` together unless a different layout is clearly necessary.
- If SQLite schema depends on optional modules (e.g., FTS5), declare them explicitly in `vcpkg.json`.
- **Database schema version policy**: the current schema is version 1. `CSchemaMigrator` accepts only `user_version == 0` (creates fresh DB) or `user_version == expected` (no-op). Any other version throws an incompatibility error requiring the user to delete and recreate the database. **Do not add automatic upgrade paths** unless the schema change is non-destructive and the decision is deliberate; when upgrading is appropriate, follow the dispatch pattern documented in `docs/Librova-Architecture.md § 5.2`.
- External converter integration stays user-configurable; `fb2cng` is the first built-in profile, not a hard-wired exclusive.

### Verification and test discipline

- Process-level IPC tests must use explicit readiness checks and deterministic cleanup — no fixed sleeps.
- When closing a review-pass issue, add a regression test for the exact failure mode before marking the checkpoint done.
- Native tests that open a `CSqliteBookRepository` or `CSqliteBookQueryRepository` must call `repository.CloseSession()` before removing the database file — Windows does not allow deleting a file with an open handle. Both classes cache a persistent SQLite connection internally.

### Logging and diagnostics

- Important execution paths in both C++ and C# must emit actionable logs through the repository logging facade. **IPC boundary logging is mandatory in both directions:**
  - Every method in `CLibraryJobServiceAdapter` (C++) must log the key outcome after computing the response — blocking message, job ID, result flag, or count — not only on failure.
  - Every managed `*Service.cs` class wrapping an IPC call must log the successful response outcome alongside the existing error-path log; error-only logs are a silent diagnostic gap.
  - **Log level guide:** `Info` for expected outcomes and normal state transitions; `Warn` for unexpected-but-handled states (CanStartImport=false, empty result where non-empty was expected, degraded operation); `Error` / `Critical` for caught exceptions and failures surfaced to the user.

### Unicode and storage safety

- If a bug fix reveals duplicated infrastructure helpers for Unicode, path safety, encoding conversion, or resource ownership, consolidate them in the same task instead of patching copies independently.
- Native UTF-8, wide-string, and `std::filesystem::path` conversions must go through `libs/Unicode/UnicodeConversion.*`; do not add ad-hoc `WideCharToMultiByte`, `MultiByteToWideChar`, `generic_u8string`, or duplicate path-conversion helpers outside that shared slice. **Never construct `std::filesystem::path` from `std::string`, `const char*`, or `std::string_view` with UTF-8 content** — `std::filesystem::path(std::string)` on Windows uses the ANSI codepage and silently corrupts non-ASCII (e.g. Cyrillic) paths. Use `PathFromUtf8()` for every UTF-8→path conversion, including protobuf string fields.

### Review and UI-specific policy

- When the user asks for a code review or review-pass style findings, write the review results in Russian by default.
- All popup / dropdown / flyout surfaces must share the same visual pair: `AppSurfaceElevatedBrush` (`#2E2414`) background + `AppAccentBorderBrush` (`#3D2C0A`) amber border. Applied to `Border.FilterPopup` and `ComboBox.AppComboBox /template/ Border#PopupBorder`. Any new Popup or flyout container must follow the same pair. See `docs/UiDesignSystem.md` § 14 "Popup / Dropdown surface rule".
- Controls that serve the same role in the toolbar (ComboBox, ToggleButton trigger) must share the same default height (`MinHeight="42"`), background token (`AppSurfaceAltBrush` via `/template/ ContentPresenter`), and border token (`AppBorderBrush`). Do not use a fixed `Height` setter on toolbar controls — use `MinHeight` so content can expand.

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

This table is the canonical document-update policy for the repository. Skills and local context docs should refer back here instead of carrying their own competing ownership rules.

When a task completes, update docs **in the same task** if any of the following changed:

| What changed | Update these files |
|---|---|
| Product scope or user-visible behavior | `docs/Librova-Product.md` |
| Architecture decision | `docs/Librova-Architecture.md` |
| Backlog item closed, added, or reprioritized | `docs/backlog.yaml` via `python scripts/backlog.py`; closed items appended to `docs/backlog-archive.yaml` via `$backlog-update` skill (append-only; do not load archive as upfront context) |
| Repository overview or project-status summary visible from the root | `README.md` |
| New convention or constraint | `AGENTS.md` (this file) |
| Verified checkpoint reached | no archive checkpoint document is maintained |
| UI workflow changed (user-visible) | `docs/ManualUiTestScenarios.md` — index only; добавляй сценарии в соответствующий файл в `docs/manual-tests/` (startup / import / library / export / delete / settings / regression); Russian; numbered steps, `Ожидаемое поведение:` blocks, UI labels in English as-is; добавляй строку в таблицу реестра в `ManualUiTestScenarios.md`; do not load as upfront context |
| UI styles, colours, components, or layout changed | `docs/UiDesignSystem.md` |
| New `libs/<Module>/` added or renamed; new key class added | `docs/CodebaseMap.md` §3 C++ Modules |
| New `apps/Librova.UI/<Folder>/` added or key C# type added | `docs/CodebaseMap.md` §4 C# Modules |
| New RPC method added to proto | `docs/CodebaseMap.md` §5 IPC Boundary (methods table) |
| Import pipeline stages or cancellation contract changed | `docs/CodebaseMap.md` §6 Import Pipeline Architecture |
| Library root layout, storage encoding, or DB schema changed | `docs/CodebaseMap.md` §7 Storage & Persistence Model |
| Domain interface added or signature changed | `docs/CodebaseMap.md` §12 Domain Interfaces Reference |
| New critical invariant discovered | `docs/CodebaseMap.md` §8 Critical Invariants |

`docs/ManualUiTestScenarios.md` must be written in **Russian**. UI labels, button names, and on-screen strings must appear exactly as they are in the English UI.

If a file stops matching implemented reality, fix it before closing the task — not later.

This applies to `README.md` as well: keep it current when project scope, architecture summary, or top-level usage framing changes.

---

## Commit Discipline

- **Never commit** unless explicitly requested by the user.
- When a commit is requested, use the `$commit-message` skill.
- Prefer one coherent commit per logical change.
- Stage only the intended files.

---

## Branching Policy

`main` is a **stable release branch**. No development is done directly on `main`.

`dev/<version>` is the **development branch** for the next release. All work — features, fixes, stabilization — happens there. The name deliberately says *dev*, not *release*, to make clear that the branch is a **work-in-progress**, not a released artifact.

### Release workflow

1. Create a dev branch: `dev/<version>` (e.g., `dev/1.1`) from the last release tag on `main`.
2. All development, fixes, and stabilization work happen in `dev/<version>`.
3. When the release is ready, tag the tip of `dev/<version>` with the version tag (e.g., `1.1.0`).
4. Merge into `main` with `--no-ff` to preserve full commit history:
   ```
   git checkout main
   git merge --no-ff dev/1.1 -m "Release 1.1.0"
   ```
5. Push `main` and the tag to origin. **Do not force-push `main` except to fix policy violations.**

### Branch naming

| Branch | Purpose |
|---|---|
| `main` | Stable releases only; each commit on `main` corresponds to a release tag |
| `dev/<version>` | Development branch for the next release (WIP — not yet released) |

### Rules

- **Never commit development work directly to `main`.**
- `main` must always point to a tagged release commit or a merge-from-release commit.
- Do not delete `dev/<version>` branches after merging — they serve as history anchors.
- There is only one active `dev/<version>` branch at any time.

---

## Skills Available

Use these skills for common recurring workflows (type `$` in the CLI to pick a skill):

| Skill | Use when |
|---|---|
| `$backlog-update` | adding, editing, validating, or closing backlog items |
| `$commit-message` | composing a commit message after the user explicitly asked for a commit |
| `$code-style` | resolving naming, formatting, or structure questions not already answered by local context |
| `$vertical-slice` | implementing a feature or workflow that crosses multiple layers |
| `$transport-rpc` | adding or changing an IPC / Protobuf method |
| `$epub-import` | changing import formats, stages, cancellation, duplicate handling, or archive behavior |
| `$analyze-logs` | analyzing `host.log` and `ui.log` after an import run |
| `$review-pass` | doing a high-risk hardening pass before review or release-candidate stabilization |
| `$win-desktop-design` | designing or refining Avalonia UI structure, look, and UX behavior |

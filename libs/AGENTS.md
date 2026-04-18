# Librova Native Core — Local Context

This file is for `libs/`-specific context only.

- For global repository rules, workflow, and document ownership, see the root `AGENTS.md`.
- For the canonical style guide, use `$code-style` and `docs/CodeStyleGuidelines.md`.
- For module locations and task-entry points, use `docs/CodebaseMap.md`.

## Local Scope

Libraries under `libs/` contain the C++20 domain and infrastructure slices of `Librova.Core`.

- each slice is a static library with its own `CMakeLists.txt`
- keep `.hpp` and `.cpp` together unless there is a strong local reason not to
- keep reusable code behind clear interfaces instead of reaching upward into UI concerns

## Local Reminders

- Use the repository logging facade, not `std::cout` / `std::cerr`, in reusable library code.
- Route UTF-8 / wide / path conversions through `libs/Unicode/UnicodeConversion.*`.
- Treat transport-facing DTOs and mappers as boundary code, not domain code.

## Native Quick Loop

```powershell
# Native-only fast path
cmake --preset x64-debug
cmake --build --preset x64-debug --config Debug
ctest --test-dir out\build\x64-debug -C Debug --output-on-failure

# Or use the repo script
.\Run-Tests.ps1 -SkipManaged
```

For the full build -> test workflow, including sequential validation rules and proto checks, follow the root `AGENTS.md`.

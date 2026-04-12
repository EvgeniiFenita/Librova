# Code Style Guidelines for Librova

This document defines the baseline coding conventions for the Librova repository.

These rules are intentionally strict at project start so the codebase grows with one consistent style instead of accumulating local conventions.

## 1. Scope

These rules apply to:

- `Librova.Core` in C++20;
- `Librova.UI` in C# / .NET / Avalonia;
- shared `.proto` contracts;
- automation scripts in `scripts/`;
- repository documentation when it describes code or workflows.

If a rule conflicts with generated code requirements, generated files may be exempted, but handwritten wrappers around them must follow this document.

## 2. Repository-Level Principles

- Prefer readability and explicitness over cleverness.
- Keep domain logic independent from UI, transport, database, and OS-specific code.
- Prefer small, testable units over large multipurpose classes.
- Use English only in code, comments, logs, docs, commit messages, and UI source strings.
- Treat Unicode and UTF-8 as first-class concerns from day one.

## 3. C++ Naming Rules

### Types

- Classes must start with `C`.
- Interfaces must start with `I`.
- Structs must start with `S`.
- Enums must start with `E`.

Example:

```cpp
class CImportService
{
};

class IBookRepository
{
};

struct SBookRecord
{
};

enum class EImportResult
{
    Success,
    Failed
};
```

### Functions

- Functions and methods must use `PascalCase`.
- Boolean-returning functions should read clearly as predicates when possible, for example `IsValid`, `HasCover`, `CanConvert`.
- **Infrastructure utility functions that operate on state or resources (database connections, file handles, IPC channels) must be grouped as `static` methods of a `final` class with a `C` prefix — not as free functions in a namespace.** Free functions are acceptable only for pure stateless transformations (e.g. math helpers, string formatting). When in doubt, use a class.

### Parameters and Local Variables

- Parameters must use `camelCase`.
- Local variables must use `camelCase`.

### Member Variables

- Class member variables must use the `m_` prefix.
- Plain data structs may omit `m_` when the struct is used as a simple DTO or value object.

## 4. C++ Layout and Formatting

- Use Allman braces for classes, structs, functions, methods, and control flow.
- Use four spaces for indentation.
- Do not align declarations manually with extra spaces.
- Keep one statement per line.
- Struct members must each be on their own line.

Correct:

```cpp
if (isDuplicate)
{
    return result;
}
```

Incorrect:

```cpp
if (isDuplicate) {
    return result;
}
```

## 5. Namespaces

- Use C++20 nested namespace syntax.
- Keep the opening brace on the same line as the namespace declaration.
- Always close with a namespace comment.

Correct:

```cpp
namespace Librova::Core::Import {

class CImportService
{
};

} // namespace Librova::Core::Import
```

## 6. Class Structure

- Section order must be `public`, `protected`, `private`.
- Do not insert an empty line directly after an access specifier.
- Prefer constructor injection for dependencies.
- Mark overrides with `override`.
- Use `final` only when preventing inheritance is intentional and justified.

## 7. Includes and Dependencies

Include order in C++ files:

1. Corresponding header
2. Standard library
3. Third-party headers
4. Project headers

Rules:

- Include only what the file directly uses.
- Do not rely on transitive includes.
- `using namespace` is forbidden in headers.
- Prefer forward declarations where they reduce header coupling without harming clarity.

## 8. Modern C++ Rules

- Baseline is `C++20`.
- Prefer `std::unique_ptr`, `std::shared_ptr`, and `std::weak_ptr` over raw owning pointers.
- Avoid `new` and `delete` in application code.
- Use RAII for every acquired resource.
- RAII wrappers around native handles or third-party library resources must explicitly delete copy and move operations unless shared ownership is intentional and documented.
- Use `nullptr` instead of `NULL` or `0`.
- Use `[[nodiscard]]` when ignoring a return value is likely a bug.
- Prefer in-class initialization and constructor initializer lists.
- Prefer `std::jthread` over `std::thread` unless there is a strong reason not to.

## 9. Error Handling and Logging

- Never swallow exceptions silently.
- Do not use `catch (...)` unless a boundary absolutely requires it and the handler logs and translates the failure immediately.
- Catch the most specific exception type reasonably available.
- Error messages must contain actionable context.

Logging rules:

- Domain code must not depend on concrete logging frameworks.
- Application and infrastructure code should log meaningful failures and state transitions.
- Important execution paths in both C++ and C# must have actionable logs at startup, shutdown, IPC boundaries, long-running operations, and failure paths.
- Do not write directly to `std::cout` or `std::cerr` from reusable library code.
- When a project logging facade is introduced, use it consistently instead of mixing direct framework calls.
- Command-line entry points may print usage or version text to standard output, but ordinary runtime diagnostics still belong in the logging facade.

**IPC boundary logging rules** (mandatory):

- Every method in `CLibraryJobServiceAdapter` must log the response outcome (blocking message, job ID, count, or result flag) after computing the response — not only on failure. A method that only logs errors leaves a silent diagnostic gap for every non-exception bug.
- Every managed `*Service.cs` method that wraps an IPC call must log the successful response alongside the existing error-path log. Log the key fields the caller cares about (e.g. `HasBlockingMessage`, `JobId`, `Completed`).
- Log level guide:
  - `Info` / `Debug` — expected outcomes, normal state transitions, IPC results on happy path.
  - `Warn` — unexpected-but-handled states: an operation proceeds but with a degraded or surprising result (e.g. `CanStartImport=false` after validation, empty result where non-empty was expected).
  - `Error` — caught exceptions and user-visible failures.
  - `Critical` — unrecoverable failures that require process restart.

## 10. Unicode and Path Handling

- Core text data is UTF-8.
- Avoid locale-dependent ANSI conversions.
- Prefer `std::filesystem::path` for file-system APIs instead of raw strings.
- Convert between UTF-8 text and OS-native paths only at explicit infrastructure boundaries.
- Avoid `path.string()` when Unicode correctness matters; prefer keeping values as `std::filesystem::path` or using Unicode-safe conversions.
- If the same Unicode/path-safety helper logic is needed in more than one module, extract a shared helper instead of copying anonymous-namespace utilities.
- Shared native UTF-8 / wide / path transcoding must live in `libs/Unicode/UnicodeConversion.*`; call that slice instead of introducing local conversion helpers.
- **Never construct `std::filesystem::path` from `std::string`, `const char*`, or `std::string_view` that carries UTF-8 content.** The constructor `std::filesystem::path(std::string)` on Windows uses the system ANSI codepage, silently corrupting non-ASCII paths (e.g. Cyrillic). Use `PathFromUtf8()` for every UTF-8→path conversion, including all protobuf `string` fields received over IPC.
- If bytes are transcoded from a legacy encoding into UTF-8, any retained XML or text encoding declaration must be rewritten to match the new UTF-8 content.

## 11. C# / Avalonia Rules

- Public types, methods, properties, and enums use `PascalCase`.
- Parameters and local variables use `camelCase`.
- Private fields use `_camelCase`.
- Prefer file-scoped namespaces unless the surrounding code already uses block namespaces.
- Keep code-behind thin; UI behavior belongs in ViewModels unless it is purely view-specific.
- Do not place business logic in Avalonia views.
- Use async APIs end-to-end for UI-triggered operations that can block.
- Avoid `async void` except for true event handlers.

## 12. Protobuf Contract Rules

- Keep contract types transport-oriented, not domain-oriented.
- Message and enum names use `PascalCase`.
- Field names use `snake_case`, as standard for protobuf.
- Do not leak internal database or file-layout assumptions into public DTOs unless explicitly part of the contract.
- Prefer additive contract evolution; avoid breaking field renames or number reuse.
- Treat named-pipe method ids as append-only and keep C++ and C# transport enums synchronized in the same checkpoint.
- Helper names must reflect actual ownership and return types; do not use names like `*View` for functions that return owning UTF-8 strings.

## 13. Testing Style

- Test names should describe observable behavior.
- One test should verify one behavior or one failure mode.
- Test fixtures must be deterministic.
- Unicode, Cyrillic, duplicate detection, and rollback scenarios are mandatory early test areas.
- For C++ tests, prefer explicit test data over magic helpers when the data is small.

## 14. File Naming

- Project-specific source files use `PascalCase`.
- Standard ecosystem files keep their canonical names, for example `CMakeLists.txt`, `.gitignore`, `Directory.Build.props`.
- Documentation files use `PascalCase` without underscores.
- `.proto` files use `snake_case`.

## 15. Comments and Documentation

- Write comments only when they add information that is not obvious from the code.
- Do not explain what the syntax already says.
- Comments must be in English.
- Do not leave stale TODOs without context. Use a concise actionable note.

## 16. Python Script Rules

Automation scripts in `scripts/` must follow these rules:

- File names use `PascalCase`.
- Classes use the `C` prefix.
- Methods use `PascalCase`.
- Local variables use `snake_case`.
- Function parameters use `camelCase`.
- Use `pathlib.Path` for file-system work.
- Entry-point scripts should disable bytecode generation if that matters for repo cleanliness.
- Shared logic should be extracted into reusable modules instead of copied between scripts.

## 17. Rules to Revisit Later

These guidelines are current for repository bootstrap. Revisit them once the first implementation slices exist, especially:

- concrete logging facade usage;
- `.proto` file naming convention;
- line-length and formatter enforcement;
- analyzer and linter configuration;
- test project naming and folder layout.

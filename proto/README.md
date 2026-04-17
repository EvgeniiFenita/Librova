# Proto

This folder contains the shared protobuf contract between `Librova.UI` and `Librova.Core`.

This file is intentionally **proto-local**. Use it for naming and toolchain reminders only.

- For the current IPC method inventory, message flow, and mapping points, see `docs/CodebaseMap.md` §5.
- For transport invariants and verification policy, see `docs/engineering/TransportInvariants.md`.
- For the ordered implementation checklist when adding or changing an IPC method, use `$transport-rpc`.

## Current Conventions

- package name: `librova.v1`
- file naming: `snake_case.proto`
- message and enum names: `PascalCase`
- field names: `snake_case`

## Current File

- `import_jobs.proto` — transport contract for import requests, catalog queries, export/delete actions, and import job lifecycle

## Local Notes

- contracts are transport-oriented and intentionally do not expose internal repository or managed-storage layout details
- protobuf C++ code generation is wired into the native build through `libs/ProtoContracts`
- application-to-transport mapping lives in `libs/ProtoMapping`
- service-shaped protobuf request/response handling lives in `libs/ProtoServices`
- the MVP path intentionally uses named-pipe transport and does not require gRPC runtime/code generation wiring
- `protoc` is expected to come from the repository `vcpkg` toolchain via the `protobuf` package
- current project-local `protoc` path for preset `x64-debug`:
  `out/build/x64-debug/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe`
- validation helper:
  `pwsh -File scripts/ValidateProto.ps1`

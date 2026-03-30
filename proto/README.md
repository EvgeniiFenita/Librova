# Proto

This folder contains shared protobuf contracts between `LibriFlow.UI` and `LibriFlow.Core`.

Current conventions:

- package name: `libriflow.v1`
- file naming: `snake_case.proto`
- message and enum names: `PascalCase`
- field names: `snake_case`

Current files:

- `import_jobs.proto`
  first transport contract for import requests and import job lifecycle

Notes:

- contracts are transport-oriented and intentionally do not expose internal repository or managed-storage layout details
- protobuf C++ code generation is now wired into the native build through `libs/ProtoContracts`
- application-to-transport mapping is implemented separately in `libs/ProtoMapping`
- gRPC runtime/code generation wiring is not added yet in this checkpoint
- `protoc` is expected to come from the repository `vcpkg` toolchain via the `protobuf` package
- current project-local `protoc` path for preset `x64-debug`:
  `out/build/x64-debug/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe`
- validation helper:
  `pwsh -File scripts/ValidateProto.ps1`

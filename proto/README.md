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
- gRPC runtime/code generation wiring is not added yet in this checkpoint

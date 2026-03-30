# Librova Transport Invariants

This document captures the invariants that keep the C++ and C# IPC surfaces in sync.

## 1. Protobuf Rules

- Treat `proto/import_jobs.proto` as append-only by default.
- Never reuse protobuf field numbers.
- Prefer additive evolution over breaking reshapes.
- Keep DTOs transport-oriented, not storage-oriented.

## 2. Named-Pipe Method Rules

- Named-pipe method ids are append-only.
- Never reorder or reuse method ids.
- Every new method must be added in both:
  - native `PipeProtocol`
  - C# `PipeProtocol`
- Every new method must be accepted by parser/validation logic on both sides.

## 3. Required Change Set For A New RPC

Whenever a new RPC is added, update all of these in one checkpoint:

- `proto/import_jobs.proto`
- native proto mapping
- native service adapter
- native pipe method enum and validation
- native pipe request dispatcher
- host composition if constructor shape changes
- C# pipe method enum
- C# client
- C# service and mapper
- tests on both sides

## 4. Verification Rule

After any transport change:

1. run `scripts/ValidateProto.ps1`
2. rebuild native code
3. run native tests
4. run managed tests

Do not treat a transport change as verified until both language sides are green.

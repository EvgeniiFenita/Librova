---
name: transport-rpc
description: Step-by-step guide for adding a new RPC / IPC method to the Librova Protobuf-over-named-pipes transport. Use when adding a new proto message, pipe method, service adapter method, or any change under proto/.
---

# Transport RPC Checklist

## Goal

Add or change an IPC method without drifting C++, C#, proto definitions, tests, or transport documentation.

## When to Use

- use this skill when adding a new transport method
- use this skill when changing protobuf request/response messages or pipe method IDs
- use this skill when any change under `proto/` affects the transport contract
- do **not** use this skill as a full feature checklist when the main work is outside transport; use `$vertical-slice` for that

## References

- method inventory and mapping points: `docs/CodebaseMap.md` §5 IPC Boundary

## Transport Invariants

### Protobuf Rules

- Treat `proto/import_jobs.proto` as append-only by default.
- Never reuse protobuf field numbers.
- Prefer additive evolution over breaking reshapes.
- Keep DTOs transport-oriented, not storage-oriented.

### Named-Pipe Method Rules

- Librova ships `Librova.UI` and `Librova.Core` in lockstep for a given release; cross-version named-pipe compatibility between different checkpoints is not a supported runtime contract.
- Within one checkpoint, native and managed named-pipe method ids must match exactly.
- When a method is replaced or removed, update both sides in the same checkpoint and keep the change explicit in tests and docs.
- Every new method must be added in both: native `PipeProtocol` and C# `PipeProtocol`.
- Every new method must be accepted by parser/validation logic on both sides.

## Required Change Set

### Protobuf contract

- [ ] edit `proto/import_jobs.proto`
- [ ] add new message types only with new field numbers
- [ ] prefer additive evolution over breaking reshapes
- [ ] keep DTOs transport-oriented

### Native side

- [ ] add proto mapping
- [ ] add or extend the native application facade method
- [ ] add or extend the native service adapter
- [ ] add or update the native pipe method enum so it matches the managed side for this checkpoint
- [ ] add validation for the new method id in the native request parser
- [ ] add the case to the native pipe request dispatcher
- [ ] update host composition if constructor shape changes
- [ ] add native unit tests
- [ ] add native integration coverage for the real pipe round-trip

### Managed side

- [ ] add the method to C# `PipeProtocol`
- [ ] add or extend the C# client method
- [ ] add or extend the C# service and mapper
- [ ] add managed integration coverage for the RPC

### Documentation

- [ ] update `docs/CodebaseMap.md` §5 when method inventory or mapping points changed
- [ ] update any other docs required by `AGENTS.md` document-maintenance policy

## Verification Sequence

Run in this exact order:

1. `scripts/ValidateProto.ps1`
2. rebuild native code
3. run native tests
4. run managed tests

Do not treat the change as verified until all four are green.

## Close-Out

- [ ] both language sides stay in lockstep
- [ ] transport docs match the implemented contract
- [ ] no gRPC runtime dependency was introduced

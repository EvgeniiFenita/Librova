---
name: transport-rpc
description: Step-by-step guide for adding a new RPC / IPC method to the Librova Protobuf-over-named-pipes transport. Use when adding a new proto message, pipe method, service adapter method, or any change under proto/.
---

# Transport RPC Checklist

Librova IPC: Protobuf contracts over Windows named pipes.  
Both sides (C++ and C#) must always stay synchronized.

> **Rule:** Never treat a transport change as verified until both language sides are green.

---

## Required Change Set (in order)

### Protobuf Contract

- [ ] Edit `proto/import_jobs.proto`
  - Add new message types (request + response)
  - Use **new field numbers only** — never reuse
  - Prefer additive evolution — avoid breaking reshapes
  - Keep DTOs transport-oriented, not storage-oriented

### Native Side (C++)

- [ ] Add proto mapping
- [ ] Add or extend native application facade method
- [ ] Add or extend native service adapter
- [ ] Add or update the native pipe method enum so it matches the managed side exactly for this checkpoint
- [ ] Add validation for the new method id in the native request parser
- [ ] Add case to native pipe request dispatcher
- [ ] Update host composition if constructor shape changes
- [ ] Add native unit tests for the new method
- [ ] Add native integration test (real pipe round-trip)

### Managed Side (C#)

- [ ] Add new method to C# `PipeProtocol` method enum (must match native)
- [ ] Add or extend C# client method
- [ ] Add or extend C# service and mapper
- [ ] Add C# integration test for the new RPC

### Verification Sequence

Run **in this exact order** — do not skip steps:

1. `scripts/ValidateProto.ps1`
2. Rebuild native code (CMake)
3. Run native tests (Catch2)
4. Run managed tests (xUnit)

All four steps must be green before closing the checkpoint.

---

## Transport Invariants (summary — full detail in `docs/engineering/TransportInvariants.md`)

- Protobuf field numbers: append-only, never reuse.
- Named-pipe method ids must stay synchronized between native and managed `PipeProtocol` enums for the same checkpoint.
- UI and host are shipped in lockstep; if a method is replaced or removed, update both sides, docs, and tests in the same checkpoint.
- gRPC runtime must **not** be introduced in the MVP path.

---

## Close-Out

- [ ] Both sides rebuild and all tests pass
- [ ] `docs/Librova-Architecture.md` updated if IPC boundary structure changed

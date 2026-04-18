---
name: docs-maintenance
description: Update, verify, and de-duplicate Librova documentation so it matches the implemented code, follows the repository doc hierarchy, avoids stale references, and minimizes AI context waste.
---

# Documentation Maintenance Playbook

## Goal

Keep Librova documentation accurate, non-duplicated, internally consistent, and aligned with the implemented code and repository workflow.

Use this skill when:
- updating docs after code changes
- reviewing documentation drift
- replacing deleted or moved docs
- checking AGENTS / local AGENTS / skills for contradictions
- consolidating overlapping guidance into the correct canonical file

Do **not** use this skill as a replacement for feature implementation checklists. Use it when the primary task is documentation correctness, documentation review, or doc synchronization after implementation.

## Core Principle

Librova documentation is a layered system. Do not spread the same rule across multiple files unless one file is intentionally a short routing summary that points to the canonical source.

Preferred hierarchy:

- `README.md` — repository entry point and document routing
- `AGENTS.md` — global workflow policy, constraints, document ownership
- `docs/Librova-Product.md` — product scope and user-visible behavior
- `docs/CodebaseMap.md` — code navigation, architecture map, invariants, task entry points
- `docs/CodeStyleGuidelines.md` — canonical style rules
- `docs/UiDesignSystem.md` — canonical UI system
- `docs/ReleaseChecklist.md` — flat release / manual verification checklist
- `.agents/skills/*/SKILL.md` — procedures and compact checklists that point back to canonical docs
- local `AGENTS.md` files — narrow local context only

If two documents overlap, keep the authoritative one and reduce the other to a short pointer.

## Read Strategy

Read only the sections relevant to the current doc task.

Do not load large documents speculatively unless the change truly spans multiple layers.

Use `AGENTS.md` `## Document Maintenance` to decide:
- which file owns the fact
- which secondary files may need routing updates
- which files must **not** carry duplicate detail

For backlog context:
- never open `docs/backlog.yaml` or `docs/backlog-archive.yaml` directly
- use `python scripts/backlog.py list`
- use `python scripts/backlog.py show <id>`

## What “Correct” Means

A documentation change is correct only if all of the following hold:

1. factual accuracy
- every claim is supported by code, scripts, repo structure, or active workflow
- filenames, paths, section references, commands, type names, and APIs are real

2. internal consistency
- no document contradicts another higher-priority document
- no skill contradicts `AGENTS.md`
- no local `AGENTS.md` overrides global policy unless it is truly local-only context

3. role clarity
- each file has a clear purpose
- navigation docs route
- canonical docs define
- skills summarize process and point back to canon

4. context efficiency
- no unnecessary repeated rules
- no stale references to deleted files
- no duplicate long-form explanations across docs

5. language consistency
- docs are in English unless the repository explicitly defines a narrow exception
- UI labels must match the source language exactly when quoted

## Verification Workflow

### 1. Identify the doc owner

Before editing, decide which file actually owns the fact.

Examples:
- product behavior -> `docs/Librova-Product.md`
- architecture decision -> `docs/CodebaseMap.md` §14
- code navigation / module map -> `docs/CodebaseMap.md`
- style rules -> `docs/CodeStyleGuidelines.md`
- UI style or layout rules -> `docs/UiDesignSystem.md`
- release/manual verification checklist -> `docs/ReleaseChecklist.md`
- workflow policy / constraints -> `AGENTS.md`

Do not “fix” the wrong file first.

### 2. Verify against code

For every nontrivial factual statement, check the implementation.

Typical things to verify:
- class / type names
- folder names
- file paths
- command names
- test file names
- API signatures
- enum names and values
- architecture boundaries
- runtime behavior
- logging expectations
- storage layout
- proto package / transport method inventory

Prefer direct source verification over inference.

### 3. Check for stale references

Search for:
- deleted documents
- moved documents
- old paths
- renamed files
- old section references
- old workflow names

Common stale-reference classes:
- `docs/engineering/*`
- old architecture docs
- old manual test registries
- old section names after consolidation

### 4. Check for duplication

Ask:
- Is this rule already canonical somewhere else?
- Should this file summarize with a pointer instead of duplicating?
- Is this skill restating too much instead of routing to the canonical file?

Allowed duplication:
- short routing summaries
- compact checklists that point back to canon
- local context that narrows global rules without redefining them

Not allowed:
- parallel competing specifications
- repeated long-form rules in multiple files
- stale copied text that drifts independently

### 5. Check for contradictions

Compare:
- `README.md` vs `AGENTS.md`
- `AGENTS.md` vs skills
- `CodebaseMap.md` vs code
- `ReleaseChecklist.md` vs product/UI behavior
- local `AGENTS.md` vs root `AGENTS.md`

If two docs disagree:
- the canonical owner wins
- update or shrink the weaker doc
- do not leave “we’ll fix later” drift behind

## Required Checks By File Type

### When editing `README.md`

Confirm it:
- routes to current canonical docs only
- does not point to deleted docs
- does not embed deep policy that belongs in `AGENTS.md`
- does not embed architecture detail that belongs in `CodebaseMap.md`

### When editing `AGENTS.md`

Confirm it:
- defines global rules only
- does not carry code-navigation detail that belongs in `CodebaseMap.md`
- does not contradict skill workflows
- does not require forbidden actions elsewhere in the file
- keeps document ownership table current

### When editing `docs/CodebaseMap.md`

Confirm it:
- matches actual modules, classes, scripts, tests, and file paths
- uses real namespaces and type names
- does not drift from domain interfaces
- does not carry duplicated rationale outside §14
- stays a navigation/reference map, not a bloated parallel architecture spec

### When editing `docs/Librova-Product.md`

Confirm it:
- describes current user-visible behavior only
- does not claim future behavior as implemented reality
- does not duplicate architecture details better owned by `CodebaseMap.md`

### When editing `docs/CodeStyleGuidelines.md`

Confirm it:
- is the canonical style source
- is not contradicted by local AGENTS or skills
- uses stable rules that are worth centralizing

### When editing `docs/ReleaseChecklist.md`

Confirm it:
- is a flat checklist, not a scenario manual
- reflects current UI workflows and release gates
- stays in English
- uses UI labels exactly as implemented when quoted

### When editing skills

Confirm each skill:
- is procedural and compact
- points back to canonical sources
- does not redefine the same policy in full
- does not contradict `AGENTS.md`
- does not instruct loading forbidden or high-cost context unnecessarily

## AI-Instruction Hygiene

When reviewing AGENTS / skills / local AGENTS:

Check for:
- conflicting instructions across layers
- duplicated rules with slightly different wording
- mandatory read steps that conflict with context-minimization goals
- references to files agents are not supposed to open directly
- instructions that force full-document reads when sectional reads are enough
- instructions that increase context cost without improving correctness

Good instruction pattern:
- short directive
- canonical pointer
- exact scope
- minimal required reads

Bad instruction pattern:
- repeated policy copied into many files
- unclear ownership
- contradictory read order
- references to deleted or deprecated docs
- “always read everything” guidance

## Change Rules

When making doc edits:
- preserve existing structure unless the task requires re-homing content
- prefer small, targeted edits over broad rewrites
- keep wording direct and operational
- keep ASCII unless the file already intentionally uses Unicode
- do not rewrite unrelated prose just because it could read better
- do not create new documents unless a clear owner file cannot reasonably hold the content

## Close-Out Checklist

Before considering the doc task done, verify:

- [ ] every changed claim is supported by code or current repo structure
- [ ] no changed file points to deleted or moved docs
- [ ] duplicated rules were reduced to pointers where appropriate
- [ ] no contradiction remains between `README.md`, `AGENTS.md`, `CodebaseMap.md`, and touched skills
- [ ] `ReleaseChecklist.md` remains English and flat
- [ ] backlog references use `scripts/backlog.py` guidance where required
- [ ] AI-facing instructions minimize unnecessary context loading
- [ ] touched docs match the ownership model in `AGENTS.md`

## Practical Review Prompts

Use these questions during a docs review:

- Is this fact actually true in code right now?
- Is this the right file to own this fact?
- Is there another file saying the same thing?
- If both remain, which one is canonical?
- Is the other file now just a pointer, or is it a competing spec?
- Does this instruction help an agent act correctly with less context?
- Does this wording accidentally force loading a document that policy says not to open directly?
- If a new contributor reads only this file, will they be routed correctly without being misled?

## Output Expectations For A Docs Review

When reporting findings:
- prioritize factual inaccuracies
- then contradictions
- then duplication/context-waste issues
- cite exact files and lines when possible
- distinguish clearly between:
  - code-backed factual errors
  - internal documentation contradictions
  - editorial improvement ideas

Do not present style suggestions as factual defects.
Only call something a defect when it is demonstrably false, contradictory, stale, or wasteful in the repository’s documentation model.

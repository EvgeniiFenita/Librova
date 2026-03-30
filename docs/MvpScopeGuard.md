# Librova MVP Scope Guard

This document protects the project from drifting into side features before the MVP gaps are closed.

## 1. Remaining MVP Buckets

As of the current roadmap, only these forward-looking buckets are on the active MVP path:

- first-run setup flow
- richer settings and converter configuration UI
- stabilization and packaging validation

If a proposed task does not clearly close one of these buckets or a concrete stabilization item, it is probably not the right next step.

## 2. Allowed Work

Allowed:

- direct completion of an open MVP bucket
- bug fixes discovered during work on an open MVP bucket
- review-driven stabilization when it prevents likely regressions
- transport or test work required to complete an open MVP bucket
- documentation synchronization when implemented reality changes

## 3. Not Allowed By Default

Do not start these unless the roadmap is explicitly changed:

- built-in book reader
- open-book-for-reading workflow
- cloud or sync features
- multi-library support
- plugin ecosystem work
- cosmetic UI expansion that does not unlock an MVP flow
- convenience utilities that do not close a real gap

## 4. Scope Decision Rule

Before starting a task, answer:

1. Which remaining MVP bucket does this close?
2. If none, which stabilization risk does it reduce?
3. If neither answer is solid, do not start the task.

## 5. Drift Rule

If implemented work closes a roadmap gap, remove it from the active roadmap in the same task.

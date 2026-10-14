# DPyTools Python conventions

The runtime target is Maya 2019 / Python 2.7. Portable tests use Python 3 and
controlled conversion/substitutes; they do not change the runtime baseline.

## Structure and compatibility

- Reuse existing helpers before creating another implementation.
- Canonical modules own behavior; historical imports are explicit facades with literal `__all__` exports.
- Keep Maya/PyMEL/Qt imports out of host-free modules. Load optional integrations and large UIs only when needed.
- Preserve public names and keyword signatures where archived scripts depend on them.
- Use immutable specifications/models, instance-owned runtime state and `None` instead of mutable defaults.

## Data and UI boundaries

- Validate complete input batches before scene, view or filesystem mutation.
- Reuse finite numeric/XYZ, bounded index, text, schema and table-selection helpers.
- Keep identity separate from display text. Do not use labels or stale names as substitutes for retained nodes or record IDs.
- Treat one-based UI rows/cells explicitly; empty filters stay empty and invalid coordinates do not become negative Python indices.
- Retain original selections/state when temporary changes are needed and attempt restoration in `finally`.
- Same-owner synchronous guards are not asynchronous or cross-window isolation.
- Report partial native progress honestly. Do not invent automatic Undo or claim a final pose/graph was verified merely because a command returned.

## Files and security

- Validate containment, symlinks, destination type and count/size budgets before I/O.
- Use shared bounded readers, versioned schemas and staged atomic publication.
- Retain ownership of temporary files/resources; remove only confirmed owned output.
- Assume trusted stable directories unless a separate hostile-race policy exists. Atomic publication is not concurrent read/modify/write locking.
- Supply credentials through the environment. Do not store or log secrets.
- Keep verified TLS enabled for SMTP and report partial delivery without blanket retries.
- Display-only byte replacement must never rewrite identities or paths used by another operation.

## Errors and diagnostics

- Catch specific expected errors; do not use bare exception handlers.
- Preserve interruption and primary failures during cleanup; attempt independent restorations even when another cleanup fails.
- Normalize empty native queries where appropriate without treating malformed data as successful input.
- Keep routine tracing opt-in through `core.diagnostics`.
- Explicit diagnostic reports may expose local paths/scene metadata; review output before sharing.
- Avoid unused imports, statement-ending semicolons and misleading success messages.

## Verification

Use actual converted function bodies and existing doubles for host-independent
control-flow tests. Test boundaries, failed preparation, native change-then-throw,
interruption and retained previous data. Source/architecture checks must cover
all package modules, and recursive test discovery must not silently omit modules.
Native Maya/Python 2/Qt/filesystem behavior remains a separate host boundary.

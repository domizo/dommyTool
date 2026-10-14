# Maya C++ conventions

The archive targets the Maya 2019-era C++11 API. The following rules describe its
ownership and evaluation boundaries, not a guarantee of native runtime correctness.

## Dependency-graph evaluation

- Recognize supported output plugs and return the correct unknown-parameter status for other requests.
- Check input acquisition, attribute reads, conversions, sizes and finite/range constraints before allocation or mutation.
- Use shared numeric, index, allocation and geometry helpers rather than duplicating unchecked arithmetic.
- Prepare complete results before publishing outputs. Mark outputs clean only after successful publication.
- Do not return success with silently discarded custom data or an incomplete result.
- Register complete attribute dependencies and use scheduling consistent with shared/native state.

## Ownership and failure handling

- Distinguish Maya-owned objects from plug-in-owned allocations. Never default-delete borrowed `MPxData` or native handles.
- Use RAII for owned arrays, buffers, iterators, graph resources and uncommitted results.
- Adopt only confirmed newly created objects; names or scene differences are not ownership.
- Check API statuses and required native pointers. Validate custom-data runtime types before casts.
- Contain exceptions at status/pointer callback boundaries without hiding partial effects.
- Maya value-returning callback ABIs do not always offer a recoverable status path. Do not add unsupported `noexcept` guarantees.
- Keep registration and cleanup records through failed initialization/unload, and retry only confirmed retained resources.

## Geometry, commands and integrations

- State coordinate-space and instance assumptions explicitly; use supplied matrices instead of arbitrary DAG discovery.
- Preserve serialized names, IDs, attribute flags and enum values. Unsupported archival modes fail explicitly.
- Keep command parse/prepare/execute/undo responsibilities distinct. Do not claim rollback for unknown native side effects.
- Treat MEL/Python callback text as trusted executable code, not sandboxed input.
- Make external file output explicit, bounded and ownership-aware; assume trusted namespaces where hostile-race protection is absent.
- Keep optional SDKs and incomplete integrations behind their feature switches.
- Pair viewport/OpenGL scopes and release only owned renderer resources. Stage geometry before native publication where possible.

## Verification and style

- Prefer clear names, small reusable helpers, explicit types and overrides, and deterministic initialization.
- Compile portable fixtures with warnings and sanitizers; use focused static analysis and source/schema guards.
- Keep test-only SDK substitutes separate from actual Autodesk/renderer dependencies.
- Do not equate portable test success with native evaluation, memory-leak freedom, thread safety or ABI compatibility.

See [Scene compatibility](MAYA_COMPATIBILITY_CONTRACT.md) and
[Verification limits](MAYA_2019_VALIDATION.md).

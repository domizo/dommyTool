# Verification scope and limits

This repository is a historical Maya 2019 source archive. Portable checks are
available, but this source snapshot has not been rebuilt and validated in the
original Maya 2019 environment. No Maya runtime verification is implied by publication.

## Available checks

- CMake C++ regression targets, with optional AddressSanitizer and UndefinedBehaviorSanitizer.
- Clang static analysis for selected host-independent code.
- Python 2 grammar and baseline conversion/compile checks for archived sources.
- Host-free Python tests and converted Maya-facing function tests with controlled substitutes.
- Registration/type-ID contracts, source inventories, compatibility facades and dependency boundaries.
- Publication scans for credentials, private paths, binaries, caches and personal IDE state.
- Checksum-pinned external source/header API checks when the dependencies are available.

Test-only API substitutes are not Autodesk, Arnold or OpenVDB SDKs. Header or
syntax checks against another Maya SDK do not establish Maya 2019 ABI compatibility.
Portable sanitizer results do not establish whole-process Maya leak freedom.

## Unverified host work

| Area | Required validation before a runnable release |
| --- | --- |
| Build and lifecycle | Assemble compatible Maya 2019 SDK/dependencies, compile/link the complete selected profile, then repeatedly load/unload it. |
| Dependency graph | Exercise representative point, curve, mesh, volume, particle, deformer and simulation paths, including dirty propagation and Evaluation Manager behavior. |
| Scene compatibility | Save/reopen `.ma` and `.mb` scenes, inspect custom-data persistence, type-ID conflicts, optional profiles and missing plug-ins. |
| Geometry and animation | Compare native units, coordinate spaces, normals, pivots, curve domains, instances, hierarchy, transforms and key/skin behavior. |
| User interfaces | Exercise filtering, empty/stale selection, callbacks, window lifetime and Script Editor/Qt behavior. |
| Native failures and Undo | Inject failures/interruption on disposable data, inspect remaining edits and verify native Undo/Redo where supported. |
| External I/O | Validate cache, image, Alembic, Arnold, texture and metadata publication on supported filesystems and with the matching SDKs. |
| Python runtime | Verify actual Python 2.7/PyMEL/Qt behavior, Unicode encoding, TLS/CA support and Windows paths. |
| Memory and integrations | Observe real process memory during repeated creation/evaluation/deletion, rendering, simulation and unload. |

## Important current constraints

- DPoint attributes/groups, DCurve and DMesh do not have a complete scene-persistence codec. Computed outputs are non-storable. See [Scene compatibility](MAYA_COMPATIBILITY_CONTRACT.md).
- Historical type IDs may collide with other plug-ins. Do not reuse them in a new product.
- Optional integrations and dependency-graph file writers are disabled in the default CMake profile.
- Native operations may leave partial scene or file changes after failure; there is no universal automatic rollback.
- Some legacy Human Rig replacement/import paths remove the previous hierarchy before the replacement completes.
- Filesystem identity/path checks assume trusted, stable namespaces; they are not protection against every hostile race or concurrent writer.
- Experimental nodes, retired enum modes and incomplete prototypes are not production features. See [Experimental scope](EXPERIMENTAL_IDEAS.md).

## Safe validation conditions

Use an isolated workstation and disposable scenes/files. Treat scenes, caches,
VDB files, callback strings, MEL/Python scripts and plug-in binaries as trusted-code
inputs, not sandboxed documents. Review diagnostic logs for local paths or private
metadata before sharing them. Preserve upstream notices when assembling dependencies.

The archive can be inspected without completing this host work. A future runnable
release requires the applicable checks above, not just a passing portable suite.

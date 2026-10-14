# Repository architecture

dommyTool is a historical source archive of procedural Maya plug-ins and Python
tools. The layout separates project code, external components, tests and build
metadata while retaining Maya-facing names and historical Python import paths.

## Top-level directories

| Directory | Contents |
| --- | --- |
| `src/` | C++ plug-in entry point, custom data, geometry algorithms, nodes, commands and integrations. |
| `python/` | Maya startup shim and the `DPyTools` package. |
| `third_party/` | Vendored libraries and their license texts. |
| `tests/` | Portable C++/Python tests, source checks and test-only API substitutes. |
| `cmake/` | Full-source and link-check configuration for an externally supplied SDK stack. |
| `config/` | Explicit C++ source manifest, third-party source/notice inventory, Maya module template and example SDK build environment. |
| `docs/` | Architecture, compatibility, dependency and verification notes; README imagery. |
| `tools/` | Dependency-source fetching, source-manifest inspection and validation orchestration. |
| `.github/` | CI workflows and dependency-update configuration. |

The root `CMakeLists.txt` defines the portable safety targets and optional SDK
builds. Root formatting files are shared code conventions, not personal IDE state.
`LICENSE`, `THIRD_PARTY_NOTICES.md` and `SECURITY.md` define the public source and
security boundaries.

## C++ source

| Directory | Responsibility |
| --- | --- |
| `src/plugin/` | Plug-in load/unload, shared feature switches and historical type IDs. |
| `src/plugin/registration/` | Registration lifetime support and feature-family manifests; `modules/` contains include-only registration lists. |
| `src/core/` | Shared custom-data infrastructure, algorithms, math, utilities and noise. |
| `src/modules/` | Maya dependency-graph nodes grouped by feature family. |
| `src/commands/` | `MPxCommand` implementations and command helpers; `Cmd/` retains legacy command support. |
| `src/integrations/arnold/` | Arnold/MtoA translators, export support and rendering integration. |
| `src/integrations/DJet/` | Optional experimental Jet integration. |

### Shared infrastructure

| Directory under `src/core/` | Contents |
| --- | --- |
| `DDataUtility/` | DPoint/DCurve/DMesh data handling, attributes, groups, generation and common data operations. |
| `Data/` | Additional data containers and shared representations. |
| `DataModifier/` | Reusable data modification helpers. |
| `Debug/` | Diagnostic support. |
| `Utility/` | Legacy utility APIs, spatial algorithms and lower-level helpers. |
| `UtilityNode/` | General-purpose utility-node implementations. |
| `common/` | Shared object, geometry and Maya-facing helpers. |
| `geometry/` | Host-independent triangulation, slicing, merging, remeshing and 2D Blue Noise sampling algorithms. |
| `math/` | Shared numerical primitives and checked arithmetic. |
| `noise/` | Noise and sampling support. |

### Feature modules

| Directory under `src/modules/` | Contents |
| --- | --- |
| `point/` | DPoint generation, attributes, groups, deformation, animation and spatial operations. |
| `curve/` | DCurve operations, Maya curves, curve generation and emitters. |
| `mesh/` | DMesh operations, polygon algorithms, mesh utilities and polygon-modifier support. |
| `volume/` | OpenVDB data, volume operations, conversion and cache I/O. |
| `dynamics/` | Bullet rigid/soft-body simulation and related solver infrastructure. |
| `particles/` | Particle data, emitters and Nucleus-facing workflows. |
| `deformers/` | Maya deformers and point-to-geometry transformation nodes. |
| `generators/` | Procedural geometry and controller generation, including icosahedron support. |
| `rendering/` | Rendering utilities, procedural scene objects and display support. |

Historical child directories such as `DPoint/`, `DCurve/`, `DMesh/`, `VDB/` and
`DBullet/` keep their original subsystem names. A `legacy/` child identifies
archival implementation details, not an additional supported product.

The repository currently contains 1200 C/C++ source, header, and inline files under `src/`.
`config/active_cpp_sources.txt` explicitly lists the 457 checked-in C/C++
implementations used by optional native targets: 417 project sources and 40
vendored sources. The list is checked independently against the files on disk;
feature switches select subsets, and external FastNoise.cpp is supplied separately.

`volume/VDB/Common/DVdbGridDispatch.h` implements project-owned, typed OpenVDB
dispatch using public APIs and C++ RTTI. It does not require the Houdini SDK.
`mesh/polyModifier/` contains a project-owned edit transaction, node base and
operation interface. It preserves the historical Maya-facing command contract;
portable transaction tests use a bounded host substitute, not Maya itself.

`common/utility/DViewportBuffer.h` owns checked, ordered stream-requirement
traversal and buffer acquisition. Eight Viewport 2.0 consumers supply their
existing semantic handlers; a failed descriptor or handler stops before buffer
packing/publication and leaves the callback eligible for retry.

`Utility/3d/Math/DVec.h` owns the fixed-width vector implementation; `Utility/DUtility.h`
is a compatibility facade over project-owned scalar and numerical helpers.
Their tests protect storage layout, exact overloads, IEEE edge behavior, aliasing,
division policies and the deterministic random-engine interface without Maya.

`DDataUtility/Data/DPAttributeData.h` implements project-owned typed arrays with
one type catalog for supported tags, names and initial values. Point and primitive
owners share the checked typed lookup in `common/utility/DAttributeLookup.h`.
`DPointCurveEmit` separates its independently implemented Pyro disk placement
from curve sampling, random draws and the external libnoise backend. Portable
tests cover these interfaces and their failure/publication behavior.

## Python package

`python/common.py` is the historical startup shim. `DPyTools/bootstrap.py`
creates menus and resolves feature implementations when an action is invoked.

| Directory under `python/DPyTools/` | Contents |
| --- | --- |
| `core/` | Maya-independent strings, numeric/geometry admission, paths, bounded text, atomic files and JSON. |
| `scene/` | Maya node, attribute, selection, transform, naming, ownership and display adapters. |
| `features/` | Canonical animation, curve, mesh, rigging, generation and diagnostic implementations. |
| `pipeline/` | Asset management, scene assembly, look development and Alembic workflows. |
| `integrations/` | Optional SMTP and RenderMan integrations. |
| `plugins/` | Standalone Python Maya plug-in entry points. |
| `deformer/` | Canonical curve-cluster/deformer helpers. |
| `ui/` | Windows, dialogs, table models, viewport-state helpers and retained UI tools. |
| `node/` | Node Info View and historical attribute-inspection entry points. |
| `operation/` | Parameter-sharing and parameter-copying interfaces. |
| `rigging/` | Face, eye and human rig tools plus historical rig entry points. |
| `joint/` | Joint fitting and other historical Joint entry points. |
| `create/` | Rig-controller creation and historical procedural creation entry points. |
| `anim/`, `curve/`, `mesh/` | Historical animation/curve/mesh import facades. |
| `dpoint/`, `dcurve/`, `dmesh/` | Custom-data generation, conversion and display entry points. |
| `general/` | Historical selection, attribute, naming, transform and utility imports. |
| `system/` | Historical path/file APIs and system-information compatibility exports. |
| `utility/` | Historical asset-tool imports, Renamer and File Cleaner. |
| `io/`, `lookdev/` | Historical I/O and material/texture import facades. |
| `locator/` | Historical standalone locator import path. |
| `network/`, `renderman/` | Historical optional-integration import paths. |
| `defomer/` | Compatibility spelling for the canonical `deformer/` package. |

Canonical modules own their implementation. Historical facades use explicit
exports; maintained code does not import back through those facades. Host-free
modules do not import Maya, PyMEL or Qt. Large native rig assembly remains a
separate host-validation boundary. See [Python module status](DPYTOOLS_MODULE_STATUS.md).

## Tests, dependencies and supporting files

| Directory | Contents |
| --- | --- |
| `tests/cpp/` | C++ regression fixtures and focused algorithm/resource tests. |
| `tests/fixtures/` | Stable source, attribute and initialization contracts. |
| `tests/stubs/` | Minimal Maya, Arnold, OpenVDB and OpenEXR substitutes for portable tests; not real SDKs. |
| `tests/compat/` | Test-only compatibility headers for legacy dependency interfaces. |
| `third_party/libnoise/` | Vendored procedural noise library. |
| `third_party/fastnoise_simd/` | Vendored SIMD noise implementation and platform support. |
| `third_party/voro++/` | Vendored Voronoi-cell implementation. |
| `third_party/licenses/` | License texts for vendored/extracted components. |
| `docs/media/` | The infographic used by the README. |
| `.github/workflows/` | Portable safety, publication and dependency API checks. |

Some licensed adapters remain beside the subsystem that consumes them. Their
locations and upstream notices are listed in [Third-party notices](../THIRD_PARTY_NOTICES.md).
Non-vendored SDKs are supplied externally; see [Dependencies](DEPENDENCIES.md).

## Compatibility boundaries

Registered names, type IDs, serialized attributes, custom-data identifiers and
historical Python import paths are public compatibility boundaries. Tests protect
selected source contracts; they do not establish Maya runtime or ABI compatibility.
See [Scene compatibility](MAYA_COMPATIBILITY_CONTRACT.md) and
[Verification limits](MAYA_2019_VALIDATION.md).

# Third-party notices

This repository combines original dommyTool code with third-party source and locally modified derivatives. The root MIT license applies to original project code, not to the third-party components listed below or their derivatives. These components retain their respective terms even when located under `src/`. Keep all embedded copyright, license and local-modification notices, together with the license texts under `third_party/licenses/`.

## Vendored components with an identified license

| Component | Location | Declared license / notice |
| --- | --- | --- |
| libnoise (Jason Bevins) | `third_party/libnoise/source/` | GNU LGPL 2.1 or later, as stated in the source headers. See `third_party/licenses/LGPL-2.1.txt`. This copy is locally modified; upstream and source-level modification notices are retained. |
| FastNoiseSIMD (Jordan Peck) | `third_party/fastnoise_simd/source/` | MIT; the full notice is retained in the source files. |
| Android `cpu-features` | `third_party/fastnoise_simd/source/ARM/` | Android Open Source Project redistribution terms are retained in both source files. |
| Voro++ 0.4-era sources (Chris H. Rycroft / LBNL) | `third_party/voro++/` | Modified BSD; see `third_party/licenses/Voro++-modified-BSD.txt`. |
| jc_voronoi (Mathias Westerdahl) | `src/modules/point/DPoint/mesh/voro/jc_voronoi.h` | MIT; full text and local-modification notice retained. |
| Dynamica / Disney Bullet Maya plug-in adapters | Listed files under `src/modules/dynamics/DBullet/`, the mesh-side Bullet shape/constraint/body/drawing helpers and solvers, and a simulation-time adapter in `src/integrations/DJet/DJetSmokeSolver.cpp`. | zlib-style file-level terms, Walt Disney Studios notices and the original contributor credits are retained. See `third_party/licenses/Dynamica-zlib.txt`. The archive's project-level metadata is not a substitute for these file-level notices. |
| Jet smoke example (Doyub Kim) | Smoke-scene initialization in `DJetSmokeSolver::initSolver`, in `src/integrations/DJet/DJetSmokeSolver.cpp`. | MIT; the example's copyright 2016, upstream license copyright 2017, complete permission/disclaimer text and local-modification statement are retained. See `third_party/licenses/Jet-MIT.txt`. This file also contains separately attributed Dynamica code. |
| Bullet fracture and MPR example derivatives | `src/modules/mesh/DMesh/Dynamics/Bullet/DBFractureDynamicsWorld.*`, `dbtFractureBody.*`, `btConvexConvexMprAlgorithm.*` | zlib; retain notices and identify altered versions. See `third_party/licenses/Bullet-zlib.txt`. |
| Alembic Maya exporter derivatives (Sony Pictures Imageworks / Industrial Light & Magic) | Listed writers under `src/modules/curve/DCurve/Cmd/Abc/`, `DDataExportUtil.*` and `DExportAbcCmd.*`. | BSD 3-clause; source-specific copyright years, conditions and disclaimer are retained. See `third_party/licenses/Alembic-BSD-3-Clause.txt`. |
| OpenVDB 5.2 derivatives (DreamWorks Animation) | Listed common/grid-data/geometry adapters, Combine, Copy, Convert, Transform, Filter Level Set, Advect Points, Remove Divergence, Write, From Mesh/Fluid, Vector Merge/Split, Remap and potential-flow compatibility code under `src/modules/volume/VDB/`. | MPL 2.0 with upstream copyright/disclaimer notices. These derivative files, including local modifications within them, remain under MPL. See `third_party/licenses/MPL-2.0.txt`. |
| PoissonGenerator 1.1.4 (Sergey Kosarevsky) | `src/core/noise/Poisson/PoissonGenerator.h` | MIT; copyright 2014-2016 and the complete permission/disclaimer text are retained. See `third_party/licenses/PoissonGenerator-MIT.txt`. |
| nanoflann 1.2.3 example adaptor (Jose Luis Blanco) | The `PointCloud` dataset adaptor in `src/core/common/object/DPointOp.h`. | BSD 2-clause; the example's copyright 2011-2016 and complete conditions/disclaimer are retained. See `third_party/licenses/Nanoflann-BSD-2-Clause.txt`. Other routines in this file are project-owned. |
| OpenSubdiv 3.3.3 tutorial adaptor (Pixar) | The `Vertex` interpolation adaptor in `src/modules/mesh/DMesh/Utility/DMSubDivide.cpp`. | Historical Modified Apache 2.0, including its replacement for Section 6. The source copyright, modification statement, full `third_party/licenses/OpenSubdiv-modified-Apache-2.0.txt` and `third_party/licenses/OpenSubdiv-NOTICE.txt` are retained. Other routines in this file are project-owned. |

The historical build also links to or directly includes headers from Autodesk Maya, Arnold/MtoA, Alembic, OpenVDB, Boost, TBB, IlmBase/OpenEXR, Bullet, OpenSubdiv, SeExpr, nanoflann, FreeType, FastNoise, and the optional Jet integration. Their transitive dependencies and binaries are not relicensed by this repository; their own installation and redistribution terms apply. The compatibility scripts download pinned official source archives only into a temporary directory; they do not add those sources to this repository. See `docs/DEPENDENCIES.md`.

## File-level scope

`config/third_party_sources.json` records the identified derivative files, their
comparison references and notice fingerprints. References identify reviewed
official versions, not necessarily the exact revision originally imported.
For a file combining several Dynamica adapters, the inventory records additional
archive members and the embedded notice retains their distinct contributor credits.
For a file combining different components, `additional_sources` records each
additional component and its source reference; the notice fingerprint protects
all embedded terms together. `DJetSmokeSolver.cpp` retains both the Dynamica
zlib-style notice and the Jet example's MIT notice. These scopes do not relicense
one another. The Jet reference identifies the reviewed example revision, not a
verified SDK version used by the original build; the Jet SDK is not bundled.
`tests/check_third_party_notices.py` protects this recorded scope and the notices;
it is not an exhaustive ownership or legal-clearance test for unknown material.

The libnoise copy retains its original LGPL headers and license text. File-level
notices identify the legacy flattened include paths and parenthesized checkerboard
expression. The original adaptation dates are unknown; 2026-10-14 is the date
those notices were added, not an invented date for the original code changes.
The separately dated numerical maintenance notices are also retained. The
inventory/checker protects all 18 modified libnoise headers and the LGPL text.

The checker also protects the embedded MIT headers in all nine FastNoiseSIMD
source files, the separate Android redistribution notices in its two
`cpu-features` files, the author headers in all 24 Voro++ source files together
with the complete Modified BSD license, and the MIT license section in
`jc_voronoi.h`. `embedded_notice_groups` records their notice fingerprints.
Fixed source scopes and complete file membership checks prevent a component or
file from silently falling out of this coverage. The check runs offline through
the existing publication, regression and CTest entry points; implementation code
and unrelated usage examples are not part of these notice fingerprints.

The project-owned grid dispatcher in `DVdbGridDispatch.h` uses public OpenVDB
APIs and does not contain the old UT helper code or require Houdini headers.
The `basePolyModifierCmd`, `basePolyModifierNode` and `basePolyModifierFty`
implementations are project-owned mesh-edit infrastructure, not redistributed
Autodesk sample implementations. Existing subclass names and Maya attributes
are retained as compatibility interfaces. The proprietary Maya SDK remains an
external dependency. These independent files are covered by the project MIT
license; including them in an MPL-derived consumer does not relicense the
separate files.

`DVec.h` and the unverified legacy portions of `DUtility.h` have independent,
project-owned implementations. They reuse the project's checked numerical
helpers and retain the public vector storage, overloads and numerical policies.
The historical projection-option macro names are compatibility interfaces only;
no third-party RX implementation is included. These separate files are covered
by the project MIT license.

The typed attribute arrays in `DPAttributeData.h` and the named lookup adapters
in `DPData.h` / `DPrimBaseData.h` are independently implemented project code.
The public type tags, names and method signatures remain compatibility interfaces;
the implementation uses a project-owned type catalog and array operations, not
OpenVDB's Metadata or MetaMap implementation. These files retain the project MIT
license and do not require OpenVDB.

The Wood mode in `DNoiseOp.cpp` uses the independently implemented
`DWoodNoise.h`: radial rings, axial grain and a seeded coordinate warp built on
the project's Perlin implementation. It does not redistribute libnoise's GPL
texture-example implementation. A libnoise Module interface adapter is retained
for the existing workers; the separately vendored libnoise source remains LGPL.

The Field input/output adapters in `DMayaFieldData.h` and the bounded field icon
in `DFieldIconGeometry.h` are project-owned implementations used by
`DCurlNoiseField` and `DOpenVDBField`. They use public Maya/OpenGL interfaces,
not Autodesk's torusField sample implementation. The comments in `DCurveShape`
describe this project's own shape behavior. These separate implementations are
covered by the project MIT license; Maya itself is still an external SDK.

The stream-requirement traversal in `DViewportBuffer.h` is independently
implemented project code shared by the Point, Mesh, Curve, Instancer and VDB
Viewport 2.0 adapters. Each consumer retains its own semantic handlers and
buffer packing/publication behavior. This separate helper is covered by the
project MIT license; it does not change the licensing of its consumers, including
the separately identified MPL-derived VDB adapter.

`DPointCurveEmit` uses an independent disk-to-curve placement implementation for
its Pyro mode. Curve-based volumetric instancing is discussed in
[Volumetric Methods in Visual Effects, SIGGRAPH 2010 course notes](https://cg.informatik.uni-freiburg.de/intern/seminar/volumeRenderingInVisualEffects2010.pdf)
(Magnus Wrenninge and collaborators); that is a conceptual reference, not a
redistributed reference-code implementation. The node's noise backend remains
the separately licensed libnoise component.

## Upstream and license references

- [zlib license — Open Source Initiative](https://opensource.org/license/zlib)
- [GNU LGPL 2.1 — Free Software Foundation](https://www.gnu.org/licenses/old-licenses/lgpl-2.1)
- [Voro++ licensing and downloads — Lawrence Berkeley National Laboratory](https://math.lbl.gov/voro++/download/)

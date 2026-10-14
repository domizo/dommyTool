# Legacy dependencies

The complete original Maya 2019 binary environment is not included. The source
candidates below are pinned to official historical commits; they are not proof
of the exact revisions used on the original development workstation.

## External source candidates

| Dependency | Version | Pinned release commit |
| --- | --- | --- |
| Bullet | 2.88 | `126b6762200b1a9ec8df39b8fd53cb2ad62fb146` |
| OpenVDB | 5.2.0 | `a79df2f32b8ebb5d15fd73325c46cee845a81e49` |
| IlmBase/OpenEXR | 2.2.1 | `dcaf5fdb4d1244d8e60a58832cfe9c54734a2257` |
| OpenSubdiv | 3.3.3 | `3b5c3d99d29ba078385013d15f8b2ebeac0daf45` |
| SeExpr | 2.11 | `a5f02bb03199630759b0b94a64f37ce56c08675a` |
| nanoflann | 1.2.3 | `f2efec0110eb68edf8cae90fce394a5044bf3db8` |

`tools/fetch_legacy_dependency_sources.sh` downloads these archives outside the
repository and verifies committed SHA-256 values. `tests/check_legacy_dependency_headers.sh`
checks includes and representative APIs when those sources are available.
Network access is needed for fetching. Neither tool supplies a complete binary
SDK environment or copies dependency archives into this repository.

Sources come from the official [Bullet](https://github.com/bulletphysics/bullet3),
[OpenVDB](https://github.com/AcademySoftwareFoundation/openvdb),
[OpenEXR](https://github.com/AcademySoftwareFoundation/openexr),
[OpenSubdiv](https://github.com/PixarAnimationStudios/OpenSubdiv),
[SeExpr](https://github.com/wdas/SeExpr) and [nanoflann](https://github.com/jlblancoc/nanoflann)
repositories. Their own licenses apply.

## Complete SDK stack

A selected native profile may also require Maya 2019/DevKit, Alembic, Arnold/MtoA,
Boost, TBB, Blosc, zlib, FreeType, libpng and a compatible compiler/runtime.
OpenVDB's transitive libraries must be mutually compatible. FastNoise and Jet
are external optional integrations with no claim that a current release is a
drop-in replacement. Legacy global SeExpr and pre-Imath-3 header layouts are
part of the expected API environment.

`config/legacy-build.env.example` lists dependency prefixes. The CMake source/link
configuration lives in `cmake/DommyFullSourceCheck.cmake` and reads the explicit
checked-in source list in `config/active_cpp_sources.txt`. No IDE project is
required. No complete Maya 2019 ABI/runtime compatibility is established by a
header or source-only check.

## Feature profiles

The default CMake profile is dependency-light. Optional components are explicitly
selected when their compatible SDKs are available.

| CMake option | Scope |
| --- | --- |
| `DOMMY_ENABLE_OPENVDB_NODES` | OpenVDB data, volume operations and VDB-specific point nodes. |
| `DOMMY_ENABLE_BULLET_NODES` | Legacy Bullet rigid/soft-body systems. |
| `DOMMY_ENABLE_ARNOLD_NODES` | Arnold/MtoA nodes, locators and translators. |
| `DOMMY_ENABLE_ALEMBIC_EXPORT` | Alembic export command and writer support. |
| `DOMMY_ENABLE_FASTNOISE_NODES` | External legacy FastNoise nodes; keep disabled without a pinned, independently reviewed backend. |
| `DOMMY_ENABLE_FREETYPE_TEXT_NODE` | FreeType-backed curve text generation. |
| `DOMMY_ENABLE_JET_NODES` | Unfinished Jet smoke integration. |
| `DOMMY_ENABLE_DG_FILE_WRITERS` | Filesystem-writing DG behavior, in addition to the owning integration. |
| `DOMMY_ENABLE_EXPERIMENTAL_BULLET_TEST_NODE` | Incomplete `DMBulletTest`; also requires Bullet. |
| `DOMMY_ENABLE_EXPERIMENTAL_FRACTURE_SOLVER` | Incomplete fracture solver; also requires Bullet. |
| `DOMMY_ENABLE_EXPERIMENTAL_STRETCH_NODE` | Incomplete `DStretchData`. |
| `DOMMY_ENABLE_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE` | Incomplete `DPArrayToCompound`. |

The geometry layer and Voro++ supply supported triangulation, slicing, remeshing
and Voronoi operations without CGAL/GMP/MPFR. Unsupported historical 3D Delaunay
and Volume Boolean enum values fail explicitly.

## Blue Noise sampling

`DPointPlaneEmit` uses a project-owned, standard-library-only 2D sampler in
`src/core/geometry/DBlueNoiseSampler.h`. Its greedy crowding-elimination approach
is informed by [Sample Elimination for Generating Poisson Disk Sample Sets](https://www.cemyuksel.com/research/sampleelimination/)
(Cem Yuksel, 2015); the implementation and compact quadratic weighting are
independent of the paper's reference-code release.

For a positive output count N, the sampler generates N+1000 unit-square
candidates and removes crowded points using a bounded uniform grid and indexed
priority array. The requested count and seed interface are retained; coordinates
and output order are not a compatibility contract with historical results.
Identical inputs are repeatable within the same build/standard-library environment.

The existing distance attribute is a normalized interaction radius in this mode,
not a guaranteed minimum separation. Finite values in [-1,0] select an automatic
radius. Invalid input or requests exceeding candidate, working-memory or operation
budgets fail without publishing a partial result. Default admission limits are
10 million candidates, 256 MiB estimated working storage and 100 million work
units. These are algorithm safety limits, not a bound on Maya's native
allocations. The separate Random/Poisson
mode continues to use `PoissonGenerator.h` under its own third-party terms.

## Wood noise and field display

Wood mode uses project-owned radial-ring/grain/domain-warp math and the existing
project Perlin generator. Its mode IDs and node attributes are retained, but its
specific texture pattern is not identical to the historical sample-derived mode.
The existing worker interface still uses libnoise's Module base; other noise
modes continue to use the separately licensed libnoise generators.

The Curl and VDB fields share checked logical-element input/output adapters.
Their force calculations and attributes are retained; the legacy viewport icon
is a project-owned three-circle/axis symbol rather than the historical torus.

## Validation targets

Source, link and packaging checks are separate options: `DOMMY_BUILD_MAYA_SDK_CHECK`,
`DOMMY_BUILD_FULL_SOURCE_CHECK`, `DOMMY_BUILD_FULL_LINK_CHECK` and `DOMMY_BUILD_PLUGIN`.
Use the matching SDK/library profiles; successful configuration alone is not a
successful build, load or evaluation.

See [Third-party notices](../THIRD_PARTY_NOTICES.md),
[Experimental scope](EXPERIMENTAL_IDEAS.md) and [Verification limits](MAYA_2019_VALIDATION.md).

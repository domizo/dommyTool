# Experimental scope

The archive preserves incomplete ideas as well as implemented systems. These
entries must not be interpreted as production-complete Maya features.

| Idea or entry | Current scope |
| --- | --- |
| Procedural-node catalogue | `python/DPyTools/ui/DNodeGenUI.py` is a disabled, read-only catalogue, absent from the default menu. |
| Match Scale | Historical names are warning-only placeholders; BBox measurement is a separate operation. |
| Symmetry-Joint prototype | `JointOp.createSymmetryJoint` is a warning-only compatibility entry. The Face Joint mirror tool is separate. |
| Standalone Python locator | Legacy viewport experiment requiring native Maya/OpenGL validation. |
| `DMBulletTest` | Optional incomplete Bullet locator; requires the experimental Bullet-test profile. |
| `DBulletFractureSolver` | Optional incomplete fracture/demo solver; not a validated procedural fracture system. |
| `DStretchData` | Optional strain-visualization prototype with incomplete controls. |
| `DPArrayToCompound` | Optional array-unpacking prototype; only its integer branch publishes values. |
| Jet smoke integration | Optional unfinished integration requiring an externally supplied compatible Jet SDK. |
| 3D Delaunay / Volume Boolean | Historical enum values are reserved, but these modes are unsupported and fail explicitly. |

Experimental CMake options default to OFF. Historical type IDs remain reserved
where necessary for scene compatibility and must not be reused for unrelated
nodes. Enabling an option does not make a prototype runtime-validated.

See [Dependencies](DEPENDENCIES.md) for the feature switches and
[Verification limits](MAYA_2019_VALIDATION.md) before attempting native use.

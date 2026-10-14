# Maya scene compatibility

The archive retains Maya-facing identities used by historical scenes and scripts.
It does not promise that every archived scene can be loaded or evaluated without
the original SDKs, optional integrations and native verification.

## Stable public schema

- Registered node, command, custom-data and viewport classification names.
- `MTypeId` values, including reserved identities for unsupported prototypes.
- Attribute long/short names, types, defaults, arrays/compounds and serialization flags.
- Command syntax and historical Python import paths used by scenes or shelf scripts.

Historical spelling mistakes may be part of this schema. File layout and internal
helper names are not serialized interfaces. Registration manifests and selected
source/attribute fixtures are checked by the portable tests.

## Type-ID limitations

Some early nodes use IDs from Autodesk's internal range rather than an allocated
third-party block. These IDs exist for archival compatibility and must not be
silently reused or renumbered. A future product needs an allocated block and an
explicit scene migration. Coexistence with other plug-ins remains a risk.

## Custom-data persistence

| Data type | ASCII | Binary | Intended scene use |
| --- | --- | --- | --- |
| `DPData` / `DPointData` | Positions only; writing with attributes/groups fails | Unsupported | Rebuild non-storable computed outputs through evaluation. |
| `DCurveData` | Unsupported | Unsupported | Rebuild non-storable computed outputs through evaluation. |
| `DMeshData` | Unsupported | Unsupported | Rebuild non-storable computed outputs through evaluation. |
| `DOpenVDBData` | Unsupported | Bounded OpenVDB stream I/O | Requires the optional OpenVDB profile; computed outputs remain non-storable. |

Unsupported callbacks fail rather than pretending to serialize a complete value.
There is no complete codec for DPoint attributes/groups, DCurve or DMesh. Any
future persistence format must be versioned, bounded and verified with native
Maya save/reopen tests.

## Coordinate and file-output boundaries

`DRayDeformer` and `DCollisionDeformer` use `inObjectMatrix/iomat` to describe the
world transform of connected object-space geometry. Identity is appropriate only
when the supplied geometry is already in world space.

Dependency-graph file writers require an explicit build profile. `DWriteVDB`
also requires its current-session `WriteFile/wrt` control. `DPCache` can read
existing files, while creating a missing cache requires `CreateMissingCache/cmc`.
These controls are non-storable and non-connectable. Other export/callback APIs
remain trusted-code and trusted-filesystem boundaries.

## Optional and unsupported behavior

Feature switches determine which registrations are available. Historical enum
values for unsupported 3D Delaunay and Volume Boolean modes remain reserved and
fail explicitly. Incomplete experimental nodes are not enabled in the default
profile. See [Dependencies](DEPENDENCIES.md) and [Experimental scope](EXPERIMENTAL_IDEAS.md).

Native scene migration, solver/renderer behavior, reference handling, callbacks,
Undo and memory require the checks described in [Verification limits](MAYA_2019_VALIDATION.md).

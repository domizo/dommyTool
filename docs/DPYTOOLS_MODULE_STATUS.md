# DPyTools module status

DPyTools targets the Maya 2019 / Python 2.7 environment. Its source is preserved
as an archive, not a currently supported Maya distribution.

| Category | Areas | Purpose |
| --- | --- | --- |
| Canonical implementations | `core/`, `scene/`, `features/`, `pipeline/`, `integrations/`, `plugins/`, `deformer/`, shared UI helpers | Own shared behavior and maintain one implementation per operation. |
| Historical facades | `anim/`, `curve/`, `mesh/`, `general/`, `io/`, `lookdev/`, `network/`, `renderman/`, `locator/`, `defomer/` and selected `system/`, `utility/`, `rigging/` files | Preserve import paths through explicit aliases/exports. |
| Native Maya tools | Face/Eye/Human Rig, Joint tools, Node Info View, parameter UIs, asset UIs, Outliner, Renamer, File Cleaner | Depend on Maya queries, scene operations or native UI behavior. |
| Optional integrations | SMTP, RenderMan | Require an explicitly selected service or external plug-in. SMTP is disabled by default in the asset UI. |
| Experimental entries | Procedural-node catalogue, standalone locator, Match Scale and symmetry-Joint compatibility names | Preserve ideas or compatibility; do not represent completed production tools. |

## Package responsibilities

- `core/` contains host-free validation, serialization, filesystem and math helpers.
- `scene/` adapts these rules to Maya identities, attributes and selection.
- `features/` contains procedural and workflow implementations, including system reports.
- `pipeline/assets/` owns asset persistence, project context, browser models and scene assembly.
- `pipeline/lookdev/` and `pipeline/io/` own texture/material and Alembic workflows.
- `ui/` contains shared lifecycle, table-selection, immutable view-model and viewport helpers.

The [repository map](ARCHITECTURE.md) describes every top-level Python directory.
Historical spelling mistakes are retained where scripts may import them.

## Operational limits

Native queries, selection, callbacks, transforms, rig assembly, skinning, exports
and Undo require Maya validation. Successful portable tests are not evidence of
native scene rollback, memory-leak freedom or correct final poses.

Asset persistence uses validated atomic publication in trusted directories, but
does not serialize concurrent writers. Large Human Rig replacement/import can
remove an existing hierarchy before a later native failure. Use disposable scenes.

Diagnostic reports may print local paths and scene metadata when explicitly
invoked. Review output before sharing. They do not automatically save or upload it.

Optional SMTP requires certificate/hostname-verified TLS and a compatible CA
store. Native Python 2 encoding, Windows filesystem behavior, network services
and renderer integration remain external validation boundaries.

See [Python conventions](PYTHON_STYLE.md), [Experimental scope](EXPERIMENTAL_IDEAS.md)
and [Verification limits](MAYA_2019_VALIDATION.md).

#pragma once

// Optional integrations are disabled unless the build selects them explicitly.
#ifndef DOMMY_WITH_ARNOLD
#define DOMMY_WITH_ARNOLD 0
#endif

#ifndef DOMMY_WITH_BULLET
#define DOMMY_WITH_BULLET 0
#endif

#ifndef DOMMY_WITH_EXPERIMENTAL_BULLET_TEST_NODE
#define DOMMY_WITH_EXPERIMENTAL_BULLET_TEST_NODE 0
#endif

// DBulletFractureSolver is retained as an archival prototype, but its node
// contract is incomplete and therefore must be enabled explicitly.
#ifndef DOMMY_WITH_EXPERIMENTAL_FRACTURE_SOLVER
#define DOMMY_WITH_EXPERIMENTAL_FRACTURE_SOLVER 0
#endif

// DStretchData exposes several historical attributes whose intended behavior
// was never completed. Keep the implementation available for study without
// presenting the node as production-complete in the default plug-in surface.
#ifndef DOMMY_WITH_EXPERIMENTAL_STRETCH_NODE
#define DOMMY_WITH_EXPERIMENTAL_STRETCH_NODE 0
#endif

// DPArrayToCompound advertises several Maya array types, but the archived
// implementation only publishes integer elements. Preserve its type ID and
// source without exposing the incomplete contract in the default plug-in.
#ifndef DOMMY_WITH_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE
#define DOMMY_WITH_EXPERIMENTAL_ARRAY_TO_COMPOUND_NODE 0
#endif

#ifndef DOMMY_WITH_OPENVDB
#define DOMMY_WITH_OPENVDB 0
#endif

#ifndef DOMMY_WITH_JET
#define DOMMY_WITH_JET 0
#endif

#ifndef DOMMY_WITH_ALEMBIC
#define DOMMY_WITH_ALEMBIC 0
#endif

// These historical nodes depend on source or libraries that are not vendored
// in this archive. Keep them out of the default registration surface so a
// dependency-light build cannot expose nodes whose implementations are absent.
#ifndef DOMMY_WITH_FASTNOISE
#define DOMMY_WITH_FASTNOISE 0
#endif

#ifndef DOMMY_WITH_FREETYPE_TEXT
#define DOMMY_WITH_FREETYPE_TEXT 0
#endif

// Dependency-graph writers can be evaluated as a side effect of graph
// evaluation. Keep them outside the default registration surface even when
// their OpenVDB or Arnold integration is enabled.
#ifndef DOMMY_WITH_DG_FILE_WRITERS
#define DOMMY_WITH_DG_FILE_WRITERS 0
#endif

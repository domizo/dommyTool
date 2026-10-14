// Serialized IDs that belonged to experiments which never became registered
// plug-in nodes.  Keep these values occupied so a future node cannot silently
// reuse an ID that may appear in an archived development scene.

#include "dvdb_ids.h"

#include <maya/MTypeId.h>

namespace {

class DOpenVDBCreate {
public:
    static const MTypeId typeId;

private:
    DOpenVDBCreate();
};

const MTypeId DOpenVDBCreate::typeId(ID_VDB_DOPENVDB_CREATE);

} // namespace

// Serialized IDs that belonged to prototypes which never became registered
// plug-in nodes. Keep these values occupied so a future node cannot silently
// reuse an ID that may appear in an archived development scene.

#include "dmesh_ids.h"
#include "dpoint_ids.h"

#include <maya/MTypeId.h>

namespace {

class DPAttributePromote {
public:
    static const MTypeId typeId;

private:
    DPAttributePromote();
};

class BTHingeConstraint {
public:
    static const MTypeId typeId;

private:
    BTHingeConstraint();
};

class BTSliderConstraint {
public:
    static const MTypeId typeId;

private:
    BTSliderConstraint();
};

const MTypeId DPAttributePromote::typeId(ID_DP_ATTR_PROMOTE);
const MTypeId BTHingeConstraint::typeId(ID_BT_HINGE_CONSTRAINT);
const MTypeId BTSliderConstraint::typeId(ID_BT_SLIDER_CONSTRAINT);

} // namespace
